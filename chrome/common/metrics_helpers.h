// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef CHROME_COMMON_METRICS_HELPERS_H_
#define CHROME_COMMON_METRICS_HELPERS_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "content/common/page_transition_types.h"

class GURL;
class MetricsLog;

// This class provides base functionality for logging metrics data.
class MetricsLogBase {
 public:
  // Creates a new metrics log
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLogBase(const std::string& client_id, int session_id,
                 const std::string& version_string);
  virtual ~MetricsLogBase();

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
                       PageTransition::Type origin,
                       int session_index,
                       base::TimeDelta load_time);

  // Record any changes in a given histogram for transmission.
  void RecordHistogramDelta(const base::Histogram& histogram,
                            const base::Histogram::SampleSet& snapshot);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // These methods allow retrieval of the encoded representation of the
  // record.  They can only be called after CloseLog() has been called.
  // GetEncodedLog returns false if buffer_size is less than
  // GetEncodedLogSize();
  int GetEncodedLogSize();
  bool GetEncodedLog(char* buffer, int buffer_size);
  // Returns an empty string on failure.
  std::string GetEncodedLogString();

  // Returns the amount of time in seconds that this log has been in use.
  int GetElapsedSeconds();

  int num_events() { return num_events_; }

  void set_hardware_class(const std::string& hardware_class) {
    hardware_class_ = hardware_class;
  }

  // Creates an MD5 hash of the given value, and returns hash as a byte
  // buffer encoded as a std::string.
  static std::string CreateHash(const std::string& value);

  // Return a base64-encoded MD5 hash of the given string.
  static std::string CreateBase64Hash(const std::string& string);

  // Get the GMT buildtime for the current binary, expressed in seconds since
  // Januray 1, 1970 GMT.
  // The value is used to identify when a new build is run, so that previous
  // reliability stats, from other builds, can be abandoned.
  static int64 GetBuildTime();

  // Use |extension| in all uploaded appversions in addition to the standard
  // version string.
  static void set_version_extension(const std::string& extension) {
    version_extension_ = extension;
  }

  virtual MetricsLog* AsMetricsLog();

 protected:
  class XmlWrapper;

  // Returns a string containing the current time.
  // Virtual so that it can be overridden for testing.
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

  // An extension that is appended to the appversion in each log.
  static std::string version_extension_;

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

  int num_events_;  // the number of events recorded in this log

  DISALLOW_COPY_AND_ASSIGN(MetricsLogBase);
};

// HistogramSender handles the logistics of gathering up available histograms
// for transmission (such as from renderer to browser, or from browser to UMA
// upload).  It has several pure virtual functions that are replaced in
// derived classes to allow the exact lower level transmission mechanism,
// or error report mechanism, to be replaced.  Since histograms can sit in
// memory for an extended period of time, and are vulnerable to memory
// corruption, this class also validates as much rendundancy as it can before
// calling for the marginal change (a.k.a., delta) in a histogram to be sent
// onward.
class HistogramSender {
 protected:
  HistogramSender();
  virtual ~HistogramSender();

  // Snapshot all histograms, and transmit the delta.
  // The arguments allow a derived class to select only a subset for
  // transmission, or to set a flag in each transmitted histogram.
  void TransmitAllHistograms(base::Histogram::Flags flags_to_set,
                             bool send_only_uma);

  // Send the histograms onward, as defined in a derived class.
  // This is only called with a delta, listing samples that have not previously
  // been transmitted.
  virtual void TransmitHistogramDelta(
      const base::Histogram& histogram,
      const base::Histogram::SampleSet& snapshot) = 0;

  // Record various errors found during attempts to send histograms.
  virtual void InconsistencyDetected(int problem) = 0;
  virtual void UniqueInconsistencyDetected(int problem) = 0;
  virtual void SnapshotProblemResolved(int amount) = 0;

 private:
  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, base::Histogram::SampleSet> LoggedSampleMap;
  // List of histograms names, and their encontered corruptions.
  typedef std::map<std::string, int> ProblemMap;

  // Snapshot this histogram, and transmit the delta.
  void TransmitHistogram(const base::Histogram& histogram);

  // For histograms, record what we've already transmitted (as a sample for each
  // histogram) so that we can send only the delta with the next log.
  LoggedSampleMap logged_samples_;

  // List of histograms found corrupt to be corrupt, and their problems.
  scoped_ptr<ProblemMap> inconsistencies_;

  DISALLOW_COPY_AND_ASSIGN(HistogramSender);
};

// This class provides base functionality for logging metrics data.
// TODO(ananta)
// Factor out more common code from chrome and chrome frame metrics service
// into this class.
class MetricsServiceBase : public HistogramSender {
 protected:
  MetricsServiceBase();
  virtual ~MetricsServiceBase();

  // Check to see if there is a log that needs to be, or is being, transmitted.
  bool pending_log() const {
    return pending_log_ || !compressed_log_.empty();
  }

  // Compress the report log in |input| using bzip2, store the result in
  // |output|.
  bool Bzip2Compress(const std::string& input, std::string* output);

  // Discard |pending_log_|, and clear |compressed_log_|. Called after
  // processing of this log is complete.
  void DiscardPendingLog();

  // Record complete list of histograms into the current log.
  // Called when we close a log.
  void RecordCurrentHistograms();

  // A log that we are currently transmiting, or about to try to transmit.
  MetricsLogBase* pending_log_;

  // An alternate form of |pending_log_|.  We persistently save this version
  // into prefs if we can't transmit it.  As a result, sometimes all we have is
  // the compressed text version.
  std::string compressed_log_;

  // The log that we are still appending to.
  MetricsLogBase* current_log_;

 private:
  // HistogramSender interface (override) methods.
  virtual void TransmitHistogramDelta(
      const base::Histogram& histogram,
      const base::Histogram::SampleSet& snapshot);
  virtual void InconsistencyDetected(int problem);
  virtual void UniqueInconsistencyDetected(int problem);
  virtual void SnapshotProblemResolved(int amount);

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBase);
};

#endif  // CHROME_COMMON_METRICS_HELPERS_H_
