// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_manager.h"

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/metrics/metrics_log_base.h"

namespace {

// Used to keep track of discarded protobuf logs without having to track xml and
// protobuf logs in separate lists.
const char kDiscardedLog[] = "Log discarded";

}  // anonymous namespace

MetricsLogManager::MetricsLogManager()
    : current_log_type_(NO_LOG),
      paused_log_type_(NO_LOG),
      staged_log_type_(NO_LOG),
      max_ongoing_log_store_size_(0),
      last_provisional_store_index_(-1),
      last_provisional_store_type_(INITIAL_LOG) {}

MetricsLogManager::~MetricsLogManager() {}

bool MetricsLogManager::SerializedLog::empty() const {
  DCHECK_EQ(xml.empty(), proto.empty());
  return xml.empty();
}

size_t MetricsLogManager::SerializedLog::length() const {
  return std::max(xml.length(), proto.length());
}

void MetricsLogManager::SerializedLog::swap(SerializedLog& log) {
  xml.swap(log.xml);
  proto.swap(log.proto);
}

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
  SerializedLog compressed_log;
  CompressCurrentLog(&compressed_log);
  if (!compressed_log.empty())
    StoreLog(&compressed_log, current_log_type_, NORMAL_STORE);
  current_log_.reset();
  current_log_type_ = NO_LOG;
}

void MetricsLogManager::StageNextLogForUpload() {
  // Prioritize initial logs for uploading.
  std::vector<SerializedLog>* source_list =
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
  return has_staged_log_proto() || has_staged_log_xml();
}

bool MetricsLogManager::has_staged_log_proto() const {
  return !staged_log_text().proto.empty() &&
      staged_log_text().proto != kDiscardedLog;
}

bool MetricsLogManager::has_staged_log_xml() const {
  return false;
}

void MetricsLogManager::DiscardStagedLog() {
  staged_log_text_.xml.clear();
  staged_log_text_.proto.clear();
  staged_log_type_ = NO_LOG;
}

void MetricsLogManager::DiscardStagedLogProto() {
  staged_log_text_.proto = kDiscardedLog;

  // If we're discarding the last piece of the log, reset the staged log state.
  if (!has_staged_log())
    DiscardStagedLog();
}

void MetricsLogManager::DiscardStagedLogXml() {
  staged_log_text_.xml = kDiscardedLog;

  // If we're discarding the last piece of the log, reset the staged log state.
  if (!has_staged_log())
    DiscardStagedLog();
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

void MetricsLogManager::StoreLog(SerializedLog* log_text,
                                 LogType log_type,
                                 StoreType store_type) {
  DCHECK(log_type != NO_LOG);
  std::vector<SerializedLog>* destination_list =
      (log_type == INITIAL_LOG) ? &unsent_initial_logs_
                                : &unsent_ongoing_logs_;
  destination_list->push_back(SerializedLog());
  destination_list->back().swap(*log_text);

  if (store_type == PROVISIONAL_STORE) {
    last_provisional_store_index_ = destination_list->size() - 1;
    last_provisional_store_type_ = log_type;
  }
}

void MetricsLogManager::DiscardLastProvisionalStore() {
  if (last_provisional_store_index_ == -1)
    return;
  std::vector<SerializedLog>* source_list =
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
    for (std::vector<SerializedLog>::iterator it = unsent_ongoing_logs_.begin();
         it != unsent_ongoing_logs_.end();) {
      size_t log_size = it->xml.length();
      if (log_size > max_ongoing_log_store_size_) {
        // TODO(isherman): We probably want a similar check for protobufs, but
        // we don't want to prevent XML upload just because the protobuf version
        // is too long.  In practice, I'm pretty sure the XML version should
        // always be longer, or at least on the same order of magnitude in
        // length.
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

void MetricsLogManager::CompressCurrentLog(SerializedLog* compressed_log) {
  int text_size = current_log_->GetEncodedLogSizeXml();
  DCHECK_GT(text_size, 0);
  std::string log_text;
  current_log_->GetEncodedLogXml(WriteInto(&log_text, text_size + 1),
                                 text_size);

  bool success = Bzip2Compress(log_text, &(compressed_log->xml));
  if (success) {
    // Allow security-conscious users to see all metrics logs that we send.
    DVLOG(1) << "METRICS LOG: " << log_text;

    // Note that we only save the protobuf version if we succeeded in
    // compressing the XML, so that the two data streams are the same.
    current_log_->GetEncodedLogProto(&(compressed_log->proto));
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
