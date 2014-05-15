// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persisted_logs.h"

#include <string>

#include "base/base64.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sha1.h"
#include "base/timer/elapsed_timer.h"

namespace metrics {

namespace {

// We append (2) more elements to persisted lists: the size of the list and a
// checksum of the elements.
const size_t kChecksumEntryCount = 2;

PersistedLogs::LogReadStatus MakeRecallStatusHistogram(
    PersistedLogs::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PrefService.PersistentLogRecallProtobufs",
                            status, PersistedLogs::END_RECALL_STATUS);
  return status;
}

}  // namespace

void PersistedLogs::LogHashPair::SwapLog(std::string* input) {
  log.swap(*input);
  if (!log.empty())
    hash = base::SHA1HashString(log);
  else
    hash.clear();
}

void PersistedLogs::LogHashPair::Swap(PersistedLogs::LogHashPair* input) {
  log.swap(input->log);
  hash.swap(input->hash);
}

PersistedLogs::PersistedLogs(PrefService* local_state,
                             const char* pref_name,
                             size_t min_log_count,
                             size_t min_log_bytes,
                             size_t max_log_size)
    : local_state_(local_state),
      pref_name_(pref_name),
      min_log_count_(min_log_count),
      min_log_bytes_(min_log_bytes),
      max_log_size_(max_log_size),
      last_provisional_store_index_(-1) {
  DCHECK(local_state_);
  // One of the limit arguments must be non-zero.
  DCHECK(min_log_count_ > 0 || min_log_bytes_ > 0);
}

PersistedLogs::~PersistedLogs() {}

void PersistedLogs::SerializeLogs() {
  // Remove any logs that are over the serialization size limit.
  if (max_log_size_) {
    for (std::vector<LogHashPair>::iterator it = list_.begin();
         it != list_.end();) {
      size_t log_size = it->log.length();
      if (log_size > max_log_size_) {
        UMA_HISTOGRAM_COUNTS("UMA.Large Accumulated Log Not Persisted",
                             static_cast<int>(log_size));
        it = list_.erase(it);
      } else {
        ++it;
      }
    }
  }
  ListPrefUpdate update(local_state_, pref_name_);
  WriteLogsToPrefList(update.Get());
}

PersistedLogs::LogReadStatus PersistedLogs::DeserializeLogs() {
  const base::ListValue* unsent_logs = local_state_->GetList(pref_name_);
  return ReadLogsFromPrefList(*unsent_logs);
}

void PersistedLogs::StoreLog(std::string* input) {
  list_.push_back(LogHashPair());
  list_.back().SwapLog(input);
}

void PersistedLogs::StageLog() {
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!list_.empty());
  DCHECK(!has_staged_log());
  staged_log_.Swap(&list_.back());
  list_.pop_back();

  // If the staged log was the last provisional store, clear that.
  if (static_cast<size_t>(last_provisional_store_index_) == list_.size())
    last_provisional_store_index_ = -1;
  DCHECK(has_staged_log());
}

void PersistedLogs::DiscardStagedLog() {
  DCHECK(has_staged_log());
  staged_log_.log.clear();
}

void PersistedLogs::StoreStagedLogAsUnsent(StoreType store_type) {
  list_.push_back(LogHashPair());
  list_.back().Swap(&staged_log_);
  if (store_type == PROVISIONAL_STORE)
    last_provisional_store_index_ = list_.size() - 1;
}

void PersistedLogs::DiscardLastProvisionalStore() {
  if (last_provisional_store_index_ == -1)
    return;
  DCHECK_LT(static_cast<size_t>(last_provisional_store_index_), list_.size());
  list_.erase(list_.begin() + last_provisional_store_index_);
  last_provisional_store_index_ = -1;
}

void PersistedLogs::WriteLogsToPrefList(base::ListValue* list_value) {
  list_value->Clear();

  // Leave the list completely empty if there are no storable values.
  if (list_.empty())
    return;

  size_t start = 0;
  // If there are too many logs, keep the most recent logs up to the length
  // limit, and at least to the minimum number of bytes.
  if (list_.size() > min_log_count_) {
    start = list_.size();
    size_t bytes_used = 0;
    std::vector<LogHashPair>::const_reverse_iterator end = list_.rend();
    for (std::vector<LogHashPair>::const_reverse_iterator it = list_.rbegin();
         it != end; ++it) {
      size_t log_size = it->log.length();
      if (bytes_used >= min_log_bytes_ &&
          (list_.size() - start) >= min_log_count_) {
        break;
      }
      bytes_used += log_size;
      --start;
    }
  }
  DCHECK_LT(start, list_.size());
  if (start >= list_.size())
    return;

  // Store size at the beginning of the list_value.
  list_value->Append(base::Value::CreateIntegerValue(list_.size() - start));

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  std::string encoded_log;
  for (std::vector<LogHashPair>::const_iterator it = list_.begin() + start;
       it != list_.end(); ++it) {
    // We encode the compressed log as Value::CreateStringValue() expects to
    // take a valid UTF8 string.
    base::Base64Encode(it->log, &encoded_log);
    base::MD5Update(&ctx, encoded_log);
    list_value->Append(base::Value::CreateStringValue(encoded_log));
  }

  // Append hash to the end of the list_value.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  list_value->Append(base::Value::CreateStringValue(
      base::MD5DigestToBase16(digest)));
  // Minimum of 3 elements (size, data, hash).
  DCHECK_GE(list_value->GetSize(), 3U);
}

PersistedLogs::LogReadStatus PersistedLogs::ReadLogsFromPrefList(
    const base::ListValue& list_value) {
  if (list_value.GetSize() == 0)
    return MakeRecallStatusHistogram(LIST_EMPTY);
  if (list_value.GetSize() <= kChecksumEntryCount)
    return MakeRecallStatusHistogram(LIST_SIZE_TOO_SMALL);

  // The size is stored at the beginning of the list_value.
  int size;
  bool valid = (*list_value.begin())->GetAsInteger(&size);
  if (!valid)
    return MakeRecallStatusHistogram(LIST_SIZE_MISSING);
  // Account for checksum and size included in the list_value.
  if (static_cast<size_t>(size) != list_value.GetSize() - kChecksumEntryCount)
    return MakeRecallStatusHistogram(LIST_SIZE_CORRUPTION);

  // Allocate strings for all of the logs we are going to read in.
  // Do this ahead of time so that we can decode the string values directly into
  // the elements of |list_|, and thereby avoid making copies of the
  // serialized logs, which can be fairly large.
  DCHECK(list_.empty());
  list_.resize(size);

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  std::string encoded_log;
  size_t local_index = 0;
  for (base::ListValue::const_iterator it = list_value.begin() + 1;
       it != list_value.end() - 1;  // Last element is the checksum.
       ++it, ++local_index) {
    bool valid = (*it)->GetAsString(&encoded_log);
    if (!valid) {
      list_.clear();
      return MakeRecallStatusHistogram(LOG_STRING_CORRUPTION);
    }

    base::MD5Update(&ctx, encoded_log);

    std::string log_text;
    if (!base::Base64Decode(encoded_log, &log_text)) {
      list_.clear();
      return MakeRecallStatusHistogram(DECODE_FAIL);
    }

    DCHECK_LT(local_index, list_.size());
    list_[local_index].SwapLog(&log_text);
  }

  // Verify checksum.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  std::string recovered_md5;
  // We store the hash at the end of the list_value.
  valid = (*(list_value.end() - 1))->GetAsString(&recovered_md5);
  if (!valid) {
    list_.clear();
    return MakeRecallStatusHistogram(CHECKSUM_STRING_CORRUPTION);
  }
  if (recovered_md5 != base::MD5DigestToBase16(digest)) {
    list_.clear();
    return MakeRecallStatusHistogram(CHECKSUM_CORRUPTION);
  }
  return MakeRecallStatusHistogram(RECALL_SUCCESS);
}

}  // namespace metrics
