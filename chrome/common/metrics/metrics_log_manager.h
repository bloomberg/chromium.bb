// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_METRICS_LOG_MANAGER_H_
#define CHROME_COMMON_METRICS_METRICS_LOG_MANAGER_H_

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

  enum LogType {
    INITIAL_LOG,  // The first log of a session.
    ONGOING_LOG,  // Subsequent logs in a session.
    NO_LOG,       // Placeholder value for when there is no log.
  };

  enum StoreType {
    NORMAL_STORE,       // A standard store operation.
    PROVISIONAL_STORE,  // A store operation that can be easily reverted later.
  };

  // Takes ownership of |log|, which has type |log_type|, and makes it the
  // current_log. This should only be called if there is not a current log.
  void BeginLoggingWithLog(MetricsLogBase* log, LogType log_type);

  // Returns the in-progress log.
  MetricsLogBase* current_log() { return current_log_.get(); }

  // Closes current_log(), compresses it, and stores the compressed log for
  // later, leaving current_log() NULL.
  void FinishCurrentLog();

  // Returns true if there are any logs waiting to be uploaded.
  bool has_unsent_logs() const {
    return !unsent_initial_logs_.empty() || !unsent_ongoing_logs_.empty();
  }

  // Populates staged_log_text() with the next stored log to send.
  // Should only be called if has_unsent_logs() is true.
  void StageNextLogForUpload();

  // Returns true if there is a log that needs to be, or is being, uploaded.
  bool has_staged_log() const;

  // The text of the staged log, as a serialized protobuf. Empty if there is no
  // staged log, or if compression of the staged log failed.
  const std::string& staged_log_text() const { return staged_log_text_; }

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

  // Restores the previously paused log (if any) to current_log().
  // This should only be called if there is not a current log.
  void ResumePausedLog();

  // Saves the staged log, then clears staged_log().
  // If |store_type| is PROVISIONAL_STORE, it can be dropped from storage with
  // a later call to DiscardLastProvisionalStore (if it hasn't already been
  // staged again).
  // This is intended to be used when logs are being saved while an upload is in
  // progress, in case the upload later succeeds.
  // This can only be called if has_staged_log() is true.
  void StoreStagedLogAsUnsent(StoreType store_type);

  // Discards the last log stored with StoreStagedLogAsUnsent with |store_type|
  // set to PROVISIONAL_STORE, as long as it hasn't already been re-staged. If
  // the log is no longer present, this is a no-op.
  void DiscardLastProvisionalStore();

  // Sets the threshold for how large an onging log can be and still be written
  // to persistant storage. Ongoing logs larger than this will be discarded
  // before persisting. 0 is interpreted as no limit.
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
  // Saves |log_text| as the given type (or discards it in accordance with
  // |max_ongoing_log_store_size_|).
  // NOTE: This clears the contents of |log_text| (to avoid an expensive
  // string copy), so the log should be discarded after this call.
  void StoreLog(std::string* log_text,
                LogType log_type,
                StoreType store_type);

  // Compresses current_log_ into compressed_log.
  void CompressCurrentLog(std::string* compressed_log);

  // The log that we are still appending to.
  scoped_ptr<MetricsLogBase> current_log_;
  LogType current_log_type_;

  // A paused, previously-current log.
  scoped_ptr<MetricsLogBase> paused_log_;
  LogType paused_log_type_;

  // Helper class to handle serialization/deserialization of logs for persistent
  // storage. May be NULL.
  scoped_ptr<LogSerializer> log_serializer_;

  // The text representations of the staged log, ready for upload to the server.
  std::string staged_log_text_;
  LogType staged_log_type_;

  // Logs from a previous session that have not yet been sent.
  // Note that the vector has the oldest logs listed first (early in the
  // vector), and we'll discard old logs if we have gathered too many logs.
  std::vector<std::string> unsent_initial_logs_;
  std::vector<std::string> unsent_ongoing_logs_;

  size_t max_ongoing_log_store_size_;

  // The index and type of the last provisional store. If nothing has been
  // provisionally stored, or the last provisional store has already been
  // re-staged, the index will be -1;
  // This is necessary because during an upload there are two logs (staged
  // and current) and a client might store them in either order, so it's
  // not necessarily the case that the provisional store is the last store.
  int last_provisional_store_index_;
  LogType last_provisional_store_type_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogManager);
};

#endif  // CHROME_COMMON_METRICS_METRICS_LOG_MANAGER_H_
