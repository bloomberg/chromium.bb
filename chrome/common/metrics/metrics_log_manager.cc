// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_manager.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/metrics/metrics_log_base.h"

MetricsLogManager::MetricsLogManager()
    : current_log_type_(NO_LOG),
      paused_log_type_(NO_LOG),
      staged_log_type_(NO_LOG),
      max_ongoing_log_store_size_(0),
      last_provisional_store_index_(-1),
      last_provisional_store_type_(INITIAL_LOG) {}

MetricsLogManager::~MetricsLogManager() {}

void MetricsLogManager::BeginLoggingWithLog(MetricsLogBase* log,
                                            LogType log_type) {
  DCHECK(log_type != NO_LOG);
  DCHECK(!current_log_.get());
  current_log_.reset(log);
  current_log_type_ = log_type;
}

void MetricsLogManager::FinishCurrentLog() {
  DCHECK(current_log_.get());
  DCHECK(current_log_type_ != NO_LOG);
  current_log_->CloseLog();
  std::string compressed_log;
  CompressCurrentLog(&compressed_log);
  if (!compressed_log.empty())
    StoreLog(&compressed_log, current_log_type_, NORMAL_STORE);
  current_log_.reset();
  current_log_type_ = NO_LOG;
}

void MetricsLogManager::StageNextLogForUpload() {
  // Prioritize initial logs for uploading.
  std::vector<std::string>* source_list =
      unsent_initial_logs_.empty() ? &unsent_ongoing_logs_
                                   : &unsent_initial_logs_;
  LogType source_type = (source_list == &unsent_ongoing_logs_) ? ONGOING_LOG
                                                               : INITIAL_LOG;
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!source_list->empty());
  DCHECK(staged_log_text_.empty());
  DCHECK(staged_log_type_ == NO_LOG);
  staged_log_text_.swap(source_list->back());
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
  return !staged_log_text().empty();
}

void MetricsLogManager::DiscardStagedLog() {
  staged_log_text_.clear();
  staged_log_type_ = NO_LOG;
}

void MetricsLogManager::DiscardCurrentLog() {
  current_log_->CloseLog();
  current_log_.reset();
  current_log_type_ = NO_LOG;
}

void MetricsLogManager::PauseCurrentLog() {
  DCHECK(!paused_log_.get());
  DCHECK(paused_log_type_ == NO_LOG);
  paused_log_.reset(current_log_.release());
  paused_log_type_ = current_log_type_;
  current_log_type_ = NO_LOG;
}

void MetricsLogManager::ResumePausedLog() {
  DCHECK(!current_log_.get());
  DCHECK(current_log_type_ == NO_LOG);
  current_log_.reset(paused_log_.release());
  current_log_type_ = paused_log_type_;
  paused_log_type_ = NO_LOG;
}

void MetricsLogManager::StoreStagedLogAsUnsent(StoreType store_type) {
  DCHECK(has_staged_log());

  // If compressing the log failed, there's nothing to store.
  if (staged_log_text_.empty())
    return;

  StoreLog(&staged_log_text_, staged_log_type_, store_type);
  DiscardStagedLog();
}

void MetricsLogManager::StoreLog(std::string* log_text,
                                 LogType log_type,
                                 StoreType store_type) {
  DCHECK(log_type != NO_LOG);
  std::vector<std::string>* destination_list =
      (log_type == INITIAL_LOG) ? &unsent_initial_logs_
                                : &unsent_ongoing_logs_;
  destination_list->push_back(std::string());
  destination_list->back().swap(*log_text);

  if (store_type == PROVISIONAL_STORE) {
    last_provisional_store_index_ = destination_list->size() - 1;
    last_provisional_store_type_ = log_type;
  }
}

void MetricsLogManager::DiscardLastProvisionalStore() {
  if (last_provisional_store_index_ == -1)
    return;
  std::vector<std::string>* source_list =
      (last_provisional_store_type_ == ONGOING_LOG) ? &unsent_ongoing_logs_
                                                    : &unsent_initial_logs_;
  DCHECK_LT(static_cast<unsigned int>(last_provisional_store_index_),
            source_list->size());
  source_list->erase(source_list->begin() + last_provisional_store_index_);
  last_provisional_store_index_ = -1;
}

void MetricsLogManager::PersistUnsentLogs() {
  DCHECK(log_serializer_.get());
  if (!log_serializer_.get())
    return;
  // Remove any ongoing logs that are over the serialization size limit.
  if (max_ongoing_log_store_size_) {
    for (std::vector<std::string>::iterator it = unsent_ongoing_logs_.begin();
         it != unsent_ongoing_logs_.end();) {
      size_t log_size = it->length();
      if (log_size > max_ongoing_log_store_size_) {
        UMA_HISTOGRAM_COUNTS("UMA.Large Accumulated Log Not Persisted",
                             static_cast<int>(log_size));
        it = unsent_ongoing_logs_.erase(it);
      } else {
        ++it;
      }
    }
  }
  log_serializer_->SerializeLogs(unsent_initial_logs_, INITIAL_LOG);
  log_serializer_->SerializeLogs(unsent_ongoing_logs_, ONGOING_LOG);
}

void MetricsLogManager::LoadPersistedUnsentLogs() {
  DCHECK(log_serializer_.get());
  if (!log_serializer_.get())
    return;
  log_serializer_->DeserializeLogs(INITIAL_LOG, &unsent_initial_logs_);
  log_serializer_->DeserializeLogs(ONGOING_LOG, &unsent_ongoing_logs_);
}

void MetricsLogManager::CompressCurrentLog(std::string* compressed_log) {
  current_log_->GetEncodedLogProto(compressed_log);
}
