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
#include "components/metrics/compression_utils.h"

namespace metrics {

namespace {

PersistedLogs::LogReadStatus MakeRecallStatusHistogram(
    PersistedLogs::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PrefService.PersistentLogRecallProtobufs",
                            status, PersistedLogs::END_RECALL_STATUS);
  return status;
}

// Reads the value at |index| from |list_value| as a string and Base64-decodes
// it into |result|. Returns true on success.
bool ReadBase64String(const base::ListValue& list_value,
                      size_t index,
                      std::string* result) {
  std::string base64_result;
  if (!list_value.GetString(index, &base64_result))
    return false;
  return base::Base64Decode(base64_result, result);
}

// Base64-encodes |str| and appends the result to |list_value|.
void AppendBase64String(const std::string& str, base::ListValue* list_value) {
  std::string base64_str;
  base::Base64Encode(str, &base64_str);
  list_value->AppendString(base64_str);
}

}  // namespace

void PersistedLogs::LogHashPair::Init(const std::string& log_data) {
  DCHECK(!log_data.empty());

  if (!GzipCompress(log_data, &compressed_log_data)) {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_PERCENTAGE(
      "UMA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_log_data.size() / log_data.size()));
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "UMA.ProtoGzippedKBSaved",
      static_cast<int>((log_data.size() - compressed_log_data.size()) / 1024),
      1, 2000, 50);

  hash = base::SHA1HashString(log_data);
}

void PersistedLogs::LogHashPair::Clear() {
  compressed_log_data.clear();
  hash.clear();
}

void PersistedLogs::LogHashPair::Swap(PersistedLogs::LogHashPair* input) {
  compressed_log_data.swap(input->compressed_log_data);
  hash.swap(input->hash);
}

PersistedLogs::PersistedLogs(PrefService* local_state,
                             const char* pref_name,
                             const char* old_pref_name,
                             size_t min_log_count,
                             size_t min_log_bytes,
                             size_t max_log_size)
    : local_state_(local_state),
      pref_name_(pref_name),
      old_pref_name_(old_pref_name),
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
      size_t log_size = it->compressed_log_data.length();
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

  // Clear the old pref now that we've written to the new one.
  // TODO(asvitkine): Remove the old pref in M39.
  local_state_->ClearPref(old_pref_name_);
}

PersistedLogs::LogReadStatus PersistedLogs::DeserializeLogs() {
  // First, try reading from old pref. If it's empty, read from the new one.
  // TODO(asvitkine): Remove the old pref in M39.
  const base::ListValue* unsent_logs = local_state_->GetList(old_pref_name_);
  if (!unsent_logs->empty())
    return ReadLogsFromOldPrefList(*unsent_logs);

  unsent_logs = local_state_->GetList(pref_name_);
  return ReadLogsFromPrefList(*unsent_logs);
}

void PersistedLogs::StoreLog(const std::string& log_data) {
  list_.push_back(LogHashPair());
  list_.back().Init(log_data);
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
  staged_log_.Clear();
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
      const size_t log_size = it->compressed_log_data.length();
      if (bytes_used >= min_log_bytes_ &&
          (list_.size() - start) >= min_log_count_) {
        break;
      }
      bytes_used += log_size;
      --start;
    }
  }
  DCHECK_LT(start, list_.size());

  for (size_t i = start; i < list_.size(); ++i) {
    AppendBase64String(list_[i].compressed_log_data, list_value);
    AppendBase64String(list_[i].hash, list_value);
  }
}

PersistedLogs::LogReadStatus PersistedLogs::ReadLogsFromPrefList(
    const base::ListValue& list_value) {
  if (list_value.empty())
    return MakeRecallStatusHistogram(LIST_EMPTY);

  // For each log, there's two entries in the list (the data and the hash).
  DCHECK_EQ(0U, list_value.GetSize() % 2);
  const size_t log_count = list_value.GetSize() / 2;

  // Resize |list_| ahead of time, so that values can be decoded directly into
  // the elements of the list.
  DCHECK(list_.empty());
  list_.resize(log_count);

  for (size_t i = 0; i < log_count; ++i) {
    if (!ReadBase64String(list_value, i * 2, &list_[i].compressed_log_data) ||
        !ReadBase64String(list_value, i * 2 + 1, &list_[i].hash)) {
      list_.clear();
      return MakeRecallStatusHistogram(LOG_STRING_CORRUPTION);
    }
  }

  return MakeRecallStatusHistogram(RECALL_SUCCESS);
}

PersistedLogs::LogReadStatus PersistedLogs::ReadLogsFromOldPrefList(
    const base::ListValue& list_value) {
  // We append (2) more elements to persisted lists: the size of the list and a
  // checksum of the elements.
  const size_t kChecksumEntryCount = 2;

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
    list_[local_index].Init(log_text);
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
