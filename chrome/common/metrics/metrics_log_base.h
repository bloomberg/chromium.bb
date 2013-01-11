// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef CHROME_COMMON_METRICS_METRICS_LOG_BASE_H_
#define CHROME_COMMON_METRICS_METRICS_LOG_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/common/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "content/public/common/page_transition_types.h"

class GURL;

namespace base {
class HistogramSamples;
}  // namespace base

// This class provides base functionality for logging metrics data.
class MetricsLogBase {
 public:
  // Creates a new metrics log
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLogBase(const std::string& client_id,
                 int session_id,
                 const std::string& version_string);
  virtual ~MetricsLogBase();

  // Computes the MD5 hash of the given string.
  // Fills |base64_encoded_hash| with the hash, encoded in base64.
  // Fills |numeric_hash| with the first 8 bytes of the hash.
  static void CreateHashes(const std::string& string,
                           std::string* base64_encoded_hash,
                           uint64* numeric_hash);

  // Get the GMT buildtime for the current binary, expressed in seconds since
  // Januray 1, 1970 GMT.
  // The value is used to identify when a new build is run, so that previous
  // reliability stats, from other builds, can be abandoned.
  static int64 GetBuildTime();

  // Convenience function to return the current time at a resolution in seconds.
  // This wraps base::TimeTicks, and hence provides an abstract time that is
  // always incrementing for use in measuring time durations.
  static int64 GetCurrentTime();

  // Records a user-initiated action.
  void RecordUserAction(const char* key);

  enum WindowEventType {
    WINDOW_CREATE = 0,
    WINDOW_OPEN,
    WINDOW_CLOSE,
    WINDOW_DESTROY
  };

  void RecordWindowEvent(WindowEventType type, int window_id, int parent_id);

  // Records a page load.
  // window_id - the index of the tab in which the load took place
  // url - which URL was loaded
  // origin - what kind of action initiated the load
  // load_time - how long it took to load the page
  void RecordLoadEvent(int window_id,
                       const GURL& url,
                       content::PageTransition origin,
                       int session_index,
                       base::TimeDelta load_time);

  // Record any changes in a given histogram for transmission.
  void RecordHistogramDelta(const std::string& histogram_name,
                            const base::HistogramSamples& snapshot);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // These methods allow retrieval of the encoded representation of the
  // record.  They can only be called after CloseLog() has been called.
  // GetEncodedLog returns false if buffer_size is less than
  // GetEncodedLogSize();
  int GetEncodedLogSizeXml();
  bool GetEncodedLogXml(char* buffer, int buffer_size);

  // Fills |encoded_log| with the protobuf representation of the record.  Can
  // only be called after CloseLog() has been called.
  void GetEncodedLogProto(std::string* encoded_log);

  // Returns the amount of time in seconds that this log has been in use.
  int GetElapsedSeconds();

  int num_events() { return num_events_; }

  void set_hardware_class(const std::string& hardware_class) {
    hardware_class_ = hardware_class;
  }

 protected:
  class XmlWrapper;

  // Returns a string containing the current time.
  // Virtual so that it can be overridden for testing.
  // TODO(isherman): Remove this method once the XML pipeline is old news.
  virtual std::string GetCurrentTimeString();
  // Helper class that invokes StartElement from constructor, and EndElement
  // from destructor.
  //
  // Use the macro OPEN_ELEMENT_FOR_SCOPE to help avoid usage problems.
  class ScopedElement {
   public:
    ScopedElement(MetricsLogBase* log, const std::string& name) : log_(log) {
      DCHECK(log);
      log->StartElement(name.c_str());
    }

    ScopedElement(MetricsLogBase* log, const char* name) : log_(log) {
      DCHECK(log);
      log->StartElement(name);
    }

    ~ScopedElement() {
      log_->EndElement();
    }

   private:
     MetricsLogBase* log_;
  };
  friend class ScopedElement;

  static const char* WindowEventTypeToString(WindowEventType type);

  // Frees the resources allocated by the XML document writer: the
  // main writer object as well as the XML tree structure, if
  // applicable.
  void FreeDocWriter();

  // Convenience versions of xmlWriter functions
  void StartElement(const char* name);
  void EndElement();
  void WriteAttribute(const std::string& name, const std::string& value);
  void WriteIntAttribute(const std::string& name, int value);
  void WriteInt64Attribute(const std::string& name, int64 value);

  // Write the attributes that are common to every metrics event type.
  void WriteCommonEventAttributes();

  bool locked() const { return locked_; }

  metrics::ChromeUserMetricsExtension* uma_proto() { return &uma_proto_; }
  const metrics::ChromeUserMetricsExtension* uma_proto() const {
    return &uma_proto_;
  }

  // TODO(isherman): Remove this once the XML pipeline is outta here.
  int num_events_;  // the number of events recorded in this log

 private:
  base::Time start_time_;
  base::Time end_time_;

  std::string client_id_;
  std::string session_id_;
  std::string hardware_class_;

  // locked_ is true when record has been packed up for sending, and should
  // no longer be written to.  It is only used for sanity checking and is
  // not a real lock.
  bool locked_;

  // Isolated to limit the dependency on the XML library for our consumers.
  XmlWrapper* xml_wrapper_;

  // Stores the protocol buffer representation for this log.
  metrics::ChromeUserMetricsExtension uma_proto_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogBase);
};

#endif  // CHROME_COMMON_METRICS_METRICS_LOG_BASE_H_
