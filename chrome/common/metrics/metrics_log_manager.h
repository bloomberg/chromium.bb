// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_METRICS_LOG_MANAGER_H_
#define CHROME_COMMON_METRICS_METRICS_LOG_MANAGER_H_


#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/metrics/metrics_log_base.h"

// Manages all the log objects used by a MetricsService implementation. Keeps
// track of both an in progress log and a log that is staged for uploading as
// text, as well as saving logs to, and loading logs from, persistent storage.
class MetricsLogManager {
 public:
  typedef MetricsLogBase::LogType LogType;

  MetricsLogManager();
  ~MetricsLogManager();

  class SerializedLog {
   public:
    SerializedLog();
    ~SerializedLog();

    const std::string& log_text() const { return log_text_; }
    const std::string& log_hash() const { return log_hash_; }

    // Returns true if the log is empty.
    bool IsEmpty() const;

    // Swaps log text with |log_text| and updates the hash. This is more
    // performant than a regular setter as it avoids doing a large string copy.
    void SwapLogText(std::string* log_text);

    // Clears the log.
    void Clear();

    // Swaps log contents with |other|.
    void Swap(SerializedLog* other);

   private:
    // Non-human readable log text (serialized proto).
    std::string log_text_;

    // Non-human readable SHA1 of |log_text| or empty if |log_text| is empty.
    std::string log_hash_;

    // Intentionally omits DISALLOW_COPY_AND_ASSIGN() so that it can be used
    // in std::vector<SerializedLog>.
  };

  enum StoreType {
    NORMAL_STORE,       // A standard store operation.
    PROVISIONAL_STORE,  // A store operation that can be easily reverted later.
  };

  // Takes ownership of |log| and makes it the current_log. This should only be
  // called if there is not a current log.
  void BeginLoggingWithLog(MetricsLogBase* log);

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
  const std::string& staged_log_text() const { return staged_log_.log_text(); }

  // The SHA1 hash (non-human readable) of the staged log or empty if there is
  // no staged log.
  const std::string& staged_log_hash() const { return staged_log_.log_hash(); }

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
    virtual void SerializeLogs(const std::vector<SerializedLog>& logs,
                               LogType log_type) = 0;

    // Populates |logs| with logs of type |log_type| deserialized from
    // persistent storage.
    virtual void DeserializeLogs(LogType log_type,
                                 std::vector<SerializedLog>* logs) = 0;
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
  // Saves |log| as the given type (or discards it in accordance with
  // |max_ongoing_log_store_size_|).
  // NOTE: This clears the contents of |log| (to avoid an expensive copy),
  // so the log should be discarded after this call.
  void StoreLog(SerializedLog* log,
                LogType log_type,
                StoreType store_type);

  // Compresses |current_log_| into |compressed_log|.
  void CompressCurrentLog(SerializedLog* compressed_log);

  // Tracks whether unsent logs (if any) have been loaded from the serializer.
  bool unsent_logs_loaded_;

  // The log that we are still appending to.
  scoped_ptr<MetricsLogBase> current_log_;

  // A paused, previously-current log.
  scoped_ptr<MetricsLogBase> paused_log_;

  // Helper class to handle serialization/deserialization of logs for persistent
  // storage. May be NULL.
  scoped_ptr<LogSerializer> log_serializer_;

  // The current staged log, ready for upload to the server.
  SerializedLog staged_log_;
  LogType staged_log_type_;

  // Logs from a previous session that have not yet been sent.
  // Note that the vector has the oldest logs listed first (early in the
  // vector), and we'll discard old logs if we have gathered too many logs.
  std::vector<SerializedLog> unsent_initial_logs_;
  std::vector<SerializedLog> unsent_ongoing_logs_;

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
