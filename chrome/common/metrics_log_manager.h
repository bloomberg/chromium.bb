// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_LOG_MANAGER_H_
#define CHROME_COMMON_METRICS_LOG_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#include <string>
#include <vector>

class MetricsLogBase;

// Manages all the log objects used by a MetricsService implementation. Keeps
// track of both an in progress log and a log that is staged for uploading as
// text, as well as saving logs to, and loading logs from, persistent storage.
class MetricsLogManager {
 public:
  MetricsLogManager();
  ~MetricsLogManager();

  // Takes ownership of |log|, and makes it the current_log.
  // This should only be called if there is not a current log.
  void BeginLoggingWithLog(MetricsLogBase* log);

  // Returns the in-progress log.
  MetricsLogBase* current_log() { return current_log_.get(); }

  // Closes |current_log| and stages it for upload, leaving |current_log| NULL.
  void StageCurrentLogForUpload();

  // Returns true if there is a log that needs to be, or is being, uploaded.
  // Note that this returns true even if compressing the log text failed.
  bool has_staged_log() const;

  // The compressed text of the staged log. Empty if there is no staged log,
  // or if compression of the staged log failed.
  const std::string& staged_log_text() { return compressed_staged_log_text_; }

  // Discards the staged log.
  void DiscardStagedLog();

  // Closes and discards |current_log|.
  void DiscardCurrentLog();

  // Sets current_log to NULL, but saves the current log for future use with
  // ResumePausedLog(). Only one log may be paused at a time.
  // TODO(stuartmorgan): Pause/resume support is really a workaround for a
  // design issue in initial log writing; that should be fixed, and pause/resume
  // removed.
  void PauseCurrentLog();

  // Restores the previously paused log (if any) to current_log.
  // This should only be called if there is not a current log.
  void ResumePausedLog();

  // Returns true if there are any logs left over from previous sessions that
  // need to be uploaded.
  bool has_unsent_logs() const {
    return !unsent_initial_logs_.empty() || !unsent_ongoing_logs_.empty();
  }

  enum LogType {
    INITIAL_LOG,  // The first log of a session.
    ONGOING_LOG,  // Subsequent logs in a session.
  };

  // Saves the staged log as the given type (or discards it in accordance with
  // set_max_ongoing_log_store_size), then clears the staged log.
  // This can only be called after StageCurrentLogForUpload.
  void StoreStagedLogAsUnsent(LogType log_type);

  // Populates staged_log_text with the next stored log to send.
  void StageNextStoredLogForUpload();

  // Sets the threshold for how large an onging log can be and still be stored.
  // Ongoing logs larger than this will be discarded. 0 is interpreted as no
  // limit.
  void set_max_ongoing_log_store_size(size_t max_size) {
    max_ongoing_log_store_size_ = max_size;
  }

  // Interface for a utility class to serialize and deserialize logs for
  // persistent storage.
  class LogSerializer {
   public:
    virtual ~LogSerializer() {}

    // Serializes |logs| to persistent storage, replacing any previously
    // serialized logs of the same type.
    virtual void SerializeLogs(const std::vector<std::string>& logs,
                               LogType log_type) = 0;

    // Populates |logs| with logs of type |log_type| deserialized from
    // persistent storage.
    virtual void DeserializeLogs(LogType log_type,
                                 std::vector<std::string>* logs) = 0;
  };

  // Sets the serializer to use for persisting and loading logs; takes ownership
  // of |serializer|.
  void set_log_serializer(LogSerializer* serializer) {
    log_serializer_.reset(serializer);
  }

  // Saves any unsent logs to persistent storage using the current log
  // serializer. Can only be called after set_log_serializer.
  void PersistUnsentLogs();

  // Loads any unsent logs from persistent storage using the current log
  // serializer. Can only be called after set_log_serializer.
  void LoadPersistedUnsentLogs();

 private:
  // Compresses staged_log_ and stores the result in
  // compressed_staged_log_text_.
  void CompressStagedLog();

  // Compresses the text in |input| using bzip2, store the result in |output|.
  static bool Bzip2Compress(const std::string& input, std::string* output);

  // The log that we are still appending to.
  scoped_ptr<MetricsLogBase> current_log_;

  // A paused, previously-current log.
  scoped_ptr<MetricsLogBase> paused_log_;

  // The log that we are currently transmiting, or about to try to transmit.
  // Note that when using StageNextStoredLogForUpload, this can be NULL while
  // compressed_staged_log_text_ is non-NULL.
  scoped_ptr<MetricsLogBase> staged_log_;

  // Helper class to handle serialization/deserialization of logs for persistent
  // storage. May be NULL.
  scoped_ptr<LogSerializer> log_serializer_;

  // The compressed text of the staged log, ready for upload to the server.
  std::string compressed_staged_log_text_;

  // Logs from a previous session that have not yet been sent.
  // Note that the vector has the oldest logs listed first (early in the
  // vector), and we'll discard old logs if we have gathered too many logs.
  std::vector<std::string> unsent_initial_logs_;
  std::vector<std::string> unsent_ongoing_logs_;

  size_t max_ongoing_log_store_size_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogManager);
};

#endif  // CHROME_COMMON_METRICS_LOG_MANAGER_H_
