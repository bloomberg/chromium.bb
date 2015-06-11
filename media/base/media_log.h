// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log_event.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"

namespace media {

class MEDIA_EXPORT MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:
  enum MediaLogLevel {
    MEDIALOG_ERROR,
    MEDIALOG_INFO,
    MEDIALOG_DEBUG,
  };

  // Convert various enums to strings.
  static std::string MediaLogLevelToString(MediaLogLevel level);
  static MediaLogEvent::Type MediaLogLevelToEventType(MediaLogLevel level);
  static std::string EventTypeToString(MediaLogEvent::Type type);
  static std::string PipelineStatusToString(PipelineStatus status);

  static std::string MediaEventToLogString(const MediaLogEvent& event);

  MediaLog();

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  virtual void AddEvent(scoped_ptr<MediaLogEvent> event);

  // Helper methods to create events and their parameters.
  scoped_ptr<MediaLogEvent> CreateEvent(MediaLogEvent::Type type);
  scoped_ptr<MediaLogEvent> CreateBooleanEvent(
      MediaLogEvent::Type type, const std::string& property, bool value);
  scoped_ptr<MediaLogEvent> CreateStringEvent(MediaLogEvent::Type type,
                                              const std::string& property,
                                              const std::string& value);
  scoped_ptr<MediaLogEvent> CreateTimeEvent(MediaLogEvent::Type type,
                                            const std::string& property,
                                            base::TimeDelta value);
  scoped_ptr<MediaLogEvent> CreateLoadEvent(const std::string& url);
  scoped_ptr<MediaLogEvent> CreateSeekEvent(float seconds);
  scoped_ptr<MediaLogEvent> CreatePipelineStateChangedEvent(
      Pipeline::State state);
  scoped_ptr<MediaLogEvent> CreatePipelineErrorEvent(PipelineStatus error);
  scoped_ptr<MediaLogEvent> CreateVideoSizeSetEvent(
      size_t width, size_t height);
  scoped_ptr<MediaLogEvent> CreateBufferedExtentsChangedEvent(
      int64 start, int64 current, int64 end);

  // Report a log message at the specified log level.
  void AddLogEvent(MediaLogLevel level, const std::string& message);

  // Report a property change without an accompanying event.
  void SetStringProperty(const std::string& key, const std::string& value);
  void SetIntegerProperty(const std::string& key, int value);
  void SetDoubleProperty(const std::string& key, double value);
  void SetBooleanProperty(const std::string& key, bool value);
  void SetTimeProperty(const std::string& key, base::TimeDelta value);

 protected:
  friend class base::RefCountedThreadSafe<MediaLog>;
  virtual ~MediaLog();

 private:
  // A unique (to this process) id for this MediaLog.
  int32 id_;

  DISALLOW_COPY_AND_ASSIGN(MediaLog);
};

// Indicates a string should be added to the log.
// First parameter - The log level for the string.
// Second parameter - The string to add to the log.
typedef base::Callback<void(MediaLog::MediaLogLevel, const std::string&)> LogCB;

// Helper class to make it easier to use LogCB or MediaLog like DVLOG().
class LogHelper {
 public:
  LogHelper(MediaLog::MediaLogLevel level, const LogCB& log_cb);
  LogHelper(MediaLog::MediaLogLevel level,
            const scoped_refptr<MediaLog>& media_log);
  ~LogHelper();

  std::ostream& stream() { return stream_; }

 private:
  MediaLog::MediaLogLevel level_;
  LogCB log_cb_;
  const scoped_refptr<MediaLog> media_log_;
  std::stringstream stream_;
};

// Provides a stringstream to collect a log entry to pass to the provided
// logger (LogCB or MediaLog) at the requested level.
#define MEDIA_LOG(level, logger) \
  LogHelper((MediaLog::MEDIALOG_##level), (logger)).stream()

// Logs only while count < max. Increments count for each log. Use LAZY_STREAM
// to avoid wasteful evaluation of subsequent stream arguments.
#define LIMITED_MEDIA_LOG(level, logger, count, max) \
  LAZY_STREAM(MEDIA_LOG(level, logger), (count) < (max) && ((count)++ || true))

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_H_
