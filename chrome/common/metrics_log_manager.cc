// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics_log_manager.h"

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/metrics_helpers.h"

MetricsLogManager::MetricsLogManager() : max_ongoing_log_store_size_(0) {}

MetricsLogManager::~MetricsLogManager() {}

void MetricsLogManager::BeginLoggingWithLog(MetricsLogBase* log) {
  DCHECK(!current_log_.get());
  current_log_.reset(log);
}

void MetricsLogManager::StageCurrentLogForUpload() {
  DCHECK(current_log_.get());
  current_log_->CloseLog();
  staged_log_.reset(current_log_.release());
  CompressStagedLog();
}

bool MetricsLogManager::has_staged_log() const {
  return staged_log_.get() || !compressed_staged_log_text_.empty();
}

void MetricsLogManager::DiscardStagedLog() {
  staged_log_.reset();
  compressed_staged_log_text_.clear();
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

void MetricsLogManager::StoreStagedLogAsUnsent(LogType log_type) {
  DCHECK(has_staged_log());
  // If compressing the log failed, there's nothing to store.
  if (compressed_staged_log_text_.empty())
    return;

  if (log_type == INITIAL_LOG) {
    unsent_initial_logs_.push_back(compressed_staged_log_text_);
  } else {
    // If it's too large, just note that and discard it.
    if (max_ongoing_log_store_size_ &&
        compressed_staged_log_text_.length() > max_ongoing_log_store_size_) {
      UMA_HISTOGRAM_COUNTS(
          "UMA.Large Accumulated Log Not Persisted",
          static_cast<int>(compressed_staged_log_text_.length()));
    } else {
      unsent_ongoing_logs_.push_back(compressed_staged_log_text_);
    }
  }
  DiscardStagedLog();
}

void MetricsLogManager::StageNextStoredLogForUpload() {
  // Prioritize initial logs for uploading.
  std::vector<std::string>* source_list = unsent_initial_logs_.empty() ?
      &unsent_ongoing_logs_ : &unsent_initial_logs_;
  DCHECK(!source_list->empty());
  DCHECK(compressed_staged_log_text_.empty());
  compressed_staged_log_text_ = source_list->back();
  source_list->pop_back();
}

void MetricsLogManager::PersistUnsentLogs() {
  DCHECK(log_serializer_.get());
  if (!log_serializer_.get())
    return;
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

void MetricsLogManager::CompressStagedLog() {
  int text_size = staged_log_->GetEncodedLogSize();
  std::string staged_log_text;
  DCHECK_GT(text_size, 0);
  staged_log_->GetEncodedLog(WriteInto(&staged_log_text, text_size + 1),
                             text_size);

  bool success = Bzip2Compress(staged_log_text, &compressed_staged_log_text_);
  if (success) {
    // Allow security-conscious users to see all metrics logs that we send.
    DVLOG(1) << "METRICS LOG: " << staged_log_text;
  } else {
    NOTREACHED() << "Failed to compress log for transmission.";
  }
}

// static
// This implementation is based on the Firefox MetricsService implementation.
bool MetricsLogManager::Bzip2Compress(const std::string& input,
                                      std::string* output) {
  bz_stream stream = {0};
  // As long as our input is smaller than the bzip2 block size, we should get
  // the best compression.  For example, if your input was 250k, using a block
  // size of 300k or 500k should result in the same compression ratio.  Since
  // our data should be under 100k, using the minimum block size of 100k should
  // allocate less temporary memory, but result in the same compression ratio.
  int result = BZ2_bzCompressInit(&stream,
                                  1,   // 100k (min) block size
                                  0,   // quiet
                                  0);  // default "work factor"
  if (result != BZ_OK) {  // out of memory?
    return false;
  }

  output->clear();

  stream.next_in = const_cast<char*>(input.data());
  stream.avail_in = static_cast<int>(input.size());
  // NOTE: we don't need a BZ_RUN phase since our input buffer contains
  //       the entire input
  do {
    output->resize(output->size() + 1024);
    stream.next_out = &((*output)[stream.total_out_lo32]);
    stream.avail_out = static_cast<int>(output->size()) - stream.total_out_lo32;
    result = BZ2_bzCompress(&stream, BZ_FINISH);
  } while (result == BZ_FINISH_OK);
  if (result != BZ_STREAM_END) {  // unknown failure?
    output->clear();
    // TODO(jar): See if it would be better to do a CHECK() here.
    return false;
  }
  result = BZ2_bzCompressEnd(&stream);
  DCHECK(result == BZ_OK);

  output->resize(stream.total_out_lo32);

  return true;
}
