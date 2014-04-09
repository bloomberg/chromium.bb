// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_manager.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/common/metrics/metrics_log_base.h"

MetricsLogManager::SerializedLog::SerializedLog() {}
MetricsLogManager::SerializedLog::~SerializedLog() {}

bool MetricsLogManager::SerializedLog::IsEmpty() const {
  return log_text_.empty();
}

void MetricsLogManager::SerializedLog::SwapLogText(std::string* log_text) {
  log_text_.swap(*log_text);
  if (log_text_.empty())
    log_hash_.clear();
  else
    log_hash_ = base::SHA1HashString(log_text_);
}

void MetricsLogManager::SerializedLog::Clear() {
  log_text_.clear();
  log_hash_.clear();
}

void MetricsLogManager::SerializedLog::Swap(
    MetricsLogManager::SerializedLog* other) {
  log_text_.swap(other->log_text_);
  log_hash_.swap(other->log_hash_);
}

MetricsLogManager::MetricsLogManager()
    : unsent_logs_loaded_(false),
      staged_log_type_(MetricsLogBase::NO_LOG),
      max_ongoing_log_store_size_(0),
      last_provisional_store_index_(-1),
      last_provisional_store_type_(MetricsLogBase::INITIAL_STABILITY_LOG) {}

MetricsLogManager::~MetricsLogManager() {}

void MetricsLogManager::BeginLoggingWithLog(MetricsLogBase* log) {
  DCHECK(!current_log_.get());
  current_log_.reset(log);
}

void MetricsLogManager::FinishCurrentLog() {
  DCHECK(current_log_.get());
  current_log_->CloseLog();
  SerializedLog compressed_log;
  CompressCurrentLog(&compressed_log);
  if (!compressed_log.IsEmpty())
    StoreLog(&compressed_log, current_log_->log_type(), NORMAL_STORE);
  current_log_.reset();
}

void MetricsLogManager::StageNextLogForUpload() {
  // Prioritize initial logs for uploading.
  std::vector<SerializedLog>* source_list =
      unsent_initial_logs_.empty() ? &unsent_ongoing_logs_
                                   : &unsent_initial_logs_;
  LogType source_type = (source_list == &unsent_ongoing_logs_) ?
      MetricsLogBase::ONGOING_LOG : MetricsLogBase::INITIAL_STABILITY_LOG;
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!source_list->empty());
  DCHECK(staged_log_.IsEmpty());
  DCHECK_EQ(MetricsLogBase::NO_LOG, staged_log_type_);
  staged_log_.Swap(&source_list->back());
  staged_log_type_ = source_type;
  source_list->pop_back();

  // If the staged log was the last provisional store, clear that.
  if (last_provisional_store_index_ != -1) {
    if (source_type == last_provisional_store_type_ &&
        static_cast<unsigned int>(last_provisional_store_index_) ==
            source_list->size()) {
      last_provisional_store_index_ = -1;
    }
  }
}

bool MetricsLogManager::has_staged_log() const {
  return !staged_log_.IsEmpty();
}

void MetricsLogManager::DiscardStagedLog() {
  staged_log_.Clear();
  staged_log_type_ = MetricsLogBase::NO_LOG;
}

void MetricsLogManager::DiscardCurrentLog() {
  current_log_->CloseLog();
  current_log_.reset();
}

void MetricsLogManager::PauseCurrentLog() {
  DCHECK(!paused_log_.get());
  paused_log_.reset(current_log_.release());
}

void MetricsLogManager::ResumePausedLog() {
  DCHECK(!current_log_.get());
  current_log_.reset(paused_log_.release());
}

void MetricsLogManager::StoreStagedLogAsUnsent(StoreType store_type) {
  DCHECK(has_staged_log());

  // If compressing the log failed, there's nothing to store.
  if (staged_log_.IsEmpty())
    return;

  StoreLog(&staged_log_, staged_log_type_, store_type);
  DiscardStagedLog();
}

void MetricsLogManager::StoreLog(SerializedLog* log,
                                 LogType log_type,
                                 StoreType store_type) {
  DCHECK_NE(MetricsLogBase::NO_LOG, log_type);
  std::vector<SerializedLog>* destination_list =
      (log_type == MetricsLogBase::INITIAL_STABILITY_LOG) ?
      &unsent_initial_logs_ : &unsent_ongoing_logs_;
  destination_list->push_back(SerializedLog());
  destination_list->back().Swap(log);

  if (store_type == PROVISIONAL_STORE) {
    last_provisional_store_index_ = destination_list->size() - 1;
    last_provisional_store_type_ = log_type;
  }
}

void MetricsLogManager::DiscardLastProvisionalStore() {
  if (last_provisional_store_index_ == -1)
    return;
  std::vector<SerializedLog>* source_list =
      (last_provisional_store_type_ == MetricsLogBase::ONGOING_LOG) ?
          &unsent_ongoing_logs_ : &unsent_initial_logs_;
  DCHECK_LT(static_cast<unsigned int>(last_provisional_store_index_),
            source_list->size());
  source_list->erase(source_list->begin() + last_provisional_store_index_);
  last_provisional_store_index_ = -1;
}

void MetricsLogManager::PersistUnsentLogs() {
  DCHECK(log_serializer_.get());
  if (!log_serializer_.get())
    return;
  DCHECK(unsent_logs_loaded_);
  if (!unsent_logs_loaded_)
    return;

  base::ElapsedTimer timer;
  // Remove any ongoing logs that are over the serialization size limit.
  if (max_ongoing_log_store_size_) {
    for (std::vector<SerializedLog>::iterator it = unsent_ongoing_logs_.begin();
         it != unsent_ongoing_logs_.end();) {
      size_t log_size = it->log_text().length();
      if (log_size > max_ongoing_log_store_size_) {
        UMA_HISTOGRAM_COUNTS("UMA.Large Accumulated Log Not Persisted",
                             static_cast<int>(log_size));
        it = unsent_ongoing_logs_.erase(it);
      } else {
        ++it;
      }
    }
  }
  log_serializer_->SerializeLogs(unsent_initial_logs_,
                                 MetricsLogBase::INITIAL_STABILITY_LOG);
  log_serializer_->SerializeLogs(unsent_ongoing_logs_,
                                 MetricsLogBase::ONGOING_LOG);
  UMA_HISTOGRAM_TIMES("UMA.StoreLogsTime", timer.Elapsed());
}

void MetricsLogManager::LoadPersistedUnsentLogs() {
  DCHECK(log_serializer_.get());
  if (!log_serializer_.get())
    return;

  base::ElapsedTimer timer;
  log_serializer_->DeserializeLogs(MetricsLogBase::INITIAL_STABILITY_LOG,
                                   &unsent_initial_logs_);
  log_serializer_->DeserializeLogs(MetricsLogBase::ONGOING_LOG,
                                   &unsent_ongoing_logs_);
  UMA_HISTOGRAM_TIMES("UMA.LoadLogsTime", timer.Elapsed());

  unsent_logs_loaded_ = true;
}

void MetricsLogManager::CompressCurrentLog(SerializedLog* compressed_log) {
  std::string log_text;
  current_log_->GetEncodedLog(&log_text);
  compressed_log->SwapLogText(&log_text);
}
