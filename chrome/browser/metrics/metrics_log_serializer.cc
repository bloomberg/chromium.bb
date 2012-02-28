// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log_serializer.h"

#include "base/base64.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"

// The number of "initial" logs we're willing to save, and hope to send during
// a future Chrome session.  Initial logs contain crash stats, and are pretty
// small.
static const size_t kMaxInitialLogsPersisted = 20;

// The number of ongoing logs we're willing to save persistently, and hope to
// send during a this or future sessions.  Note that each log may be pretty
// large, as presumably the related "initial" log wasn't sent (probably nothing
// was, as the user was probably off-line).  As a result, the log probably kept
// accumulating while the "initial" log was stalled, and couldn't be sent.  As a
// result, we don't want to save too many of these mega-logs.
// A "standard shutdown" will create a small log, including just the data that
// was not yet been transmitted, and that is normal (to have exactly one
// ongoing_log_ at startup).
static const size_t kMaxOngoingLogsPersisted = 8;

// We append (2) more elements to persisted lists: the size of the list and a
// checksum of the elements.
static const size_t kChecksumEntryCount = 2;

namespace {
// TODO(ziadh): This is here temporarily for a side experiment. Remove later
// on.
enum LogStoreStatus {
  STORE_SUCCESS,    // Successfully presisted log.
  ENCODE_FAIL,      // Failed to encode log.
  COMPRESS_FAIL,    // Failed to compress log.
  END_STORE_STATUS  // Number of bins to use to create the histogram.
};

MetricsLogSerializer::LogReadStatus MakeRecallStatusHistogram(
    MetricsLogSerializer::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PrefService.PersistentLogRecall", status,
                            MetricsLogSerializer::END_RECALL_STATUS);
  return status;
}

// TODO(ziadh): Remove this when done with experiment.
void MakeStoreStatusHistogram(LogStoreStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PrefService.PersistentLogStore2", status,
                            END_STORE_STATUS);
}
}  // namespace


MetricsLogSerializer::MetricsLogSerializer() {}

MetricsLogSerializer::~MetricsLogSerializer() {}

void MetricsLogSerializer::SerializeLogs(const std::vector<std::string>& logs,
                                         MetricsLogManager::LogType log_type) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const char* pref = NULL;
  size_t max_store_count = 0;
  switch (log_type) {
    case MetricsLogManager::INITIAL_LOG:
      pref = prefs::kMetricsInitialLogs;
      max_store_count = kMaxInitialLogsPersisted;
      break;
    case MetricsLogManager::ONGOING_LOG:
      pref = prefs::kMetricsOngoingLogs;
      max_store_count = kMaxOngoingLogsPersisted;
      break;
    default:
      NOTREACHED();
      return;
  };
  ListPrefUpdate update(local_state, pref);
  ListValue* pref_list = update.Get();
  WriteLogsToPrefList(logs, max_store_count, pref_list);
}

void MetricsLogSerializer::DeserializeLogs(MetricsLogManager::LogType log_type,
                                           std::vector<std::string>* logs) {
  DCHECK(logs);
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  const char* pref = (log_type == MetricsLogManager::INITIAL_LOG) ?
      prefs::kMetricsInitialLogs : prefs::kMetricsOngoingLogs;
  const ListValue* unsent_logs = local_state->GetList(pref);
  ReadLogsFromPrefList(*unsent_logs, logs);
}

// static
void MetricsLogSerializer::WriteLogsToPrefList(
    const std::vector<std::string>& local_list,
    const size_t kMaxLocalListSize,
    ListValue* list) {
  list->Clear();
  size_t start = 0;
  if (local_list.size() > kMaxLocalListSize)
    start = local_list.size() - kMaxLocalListSize;
  DCHECK(start <= local_list.size());
  if (local_list.size() <= start)
    return;

  // Store size at the beginning of the list.
  list->Append(Value::CreateIntegerValue(local_list.size() - start));

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  std::string encoded_log;
  for (std::vector<std::string>::const_iterator it = local_list.begin() + start;
       it != local_list.end(); ++it) {
    // We encode the compressed log as Value::CreateStringValue() expects to
    // take a valid UTF8 string.
    if (!base::Base64Encode(*it, &encoded_log)) {
      MakeStoreStatusHistogram(ENCODE_FAIL);
      list->Clear();
      return;
    }
    base::MD5Update(&ctx, encoded_log);
    list->Append(Value::CreateStringValue(encoded_log));
  }

  // Append hash to the end of the list.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  list->Append(Value::CreateStringValue(base::MD5DigestToBase16(digest)));
  DCHECK(list->GetSize() >= 3);  // Minimum of 3 elements (size, data, hash).
  MakeStoreStatusHistogram(STORE_SUCCESS);
}

// static
MetricsLogSerializer::LogReadStatus MetricsLogSerializer::ReadLogsFromPrefList(
    const ListValue& list,
    std::vector<std::string>* local_list) {
  DCHECK(local_list->empty());
  if (list.GetSize() == 0)
    return MakeRecallStatusHistogram(LIST_EMPTY);
  if (list.GetSize() < 3)
    return MakeRecallStatusHistogram(LIST_SIZE_TOO_SMALL);

  // The size is stored at the beginning of the list.
  int size;
  bool valid = (*list.begin())->GetAsInteger(&size);
  if (!valid)
    return MakeRecallStatusHistogram(LIST_SIZE_MISSING);

  // Account for checksum and size included in the list.
  if (static_cast<unsigned int>(size) !=
      list.GetSize() - kChecksumEntryCount) {
    return MakeRecallStatusHistogram(LIST_SIZE_CORRUPTION);
  }

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  std::string encoded_log;
  std::string decoded_log;
  for (ListValue::const_iterator it = list.begin() + 1;
       it != list.end() - 1; ++it) {  // Last element is the checksum.
    valid = (*it)->GetAsString(&encoded_log);
    if (!valid) {
      local_list->clear();
      return MakeRecallStatusHistogram(LOG_STRING_CORRUPTION);
    }

    base::MD5Update(&ctx, encoded_log);

    if (!base::Base64Decode(encoded_log, &decoded_log)) {
      local_list->clear();
      return MakeRecallStatusHistogram(DECODE_FAIL);
    }
    local_list->push_back(decoded_log);
  }

  // Verify checksum.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  std::string recovered_md5;
  // We store the hash at the end of the list.
  valid = (*(list.end() - 1))->GetAsString(&recovered_md5);
  if (!valid) {
    local_list->clear();
    return MakeRecallStatusHistogram(CHECKSUM_STRING_CORRUPTION);
  }
  if (recovered_md5 != base::MD5DigestToBase16(digest)) {
    local_list->clear();
    return MakeRecallStatusHistogram(CHECKSUM_CORRUPTION);
  }
  return MakeRecallStatusHistogram(RECALL_SUCCESS);
}
