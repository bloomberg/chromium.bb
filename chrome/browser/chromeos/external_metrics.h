// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use
// of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/task.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

class Profile;

namespace chromeos {

// ExternalMetrics is a service that Chrome offers to Chrome OS to upload
// metrics to the UMA server on its behalf.  Chrome periodically reads the
// content of a well-know file, and parses it into name-value pairs, each
// representing a Chrome OS metrics event. The events are logged using the
// normal UMA mechanism. The file is then truncated to zero size. Chrome uses
// flock() to synchronize accesses to the file.
class ExternalMetrics : public base::RefCountedThreadSafe<ExternalMetrics> {
  FRIEND_TEST(ExternalMetricsTest, ParseExternalMetricsFile);
  friend class base::RefCountedThreadSafe<ExternalMetrics>;

 public:
  ExternalMetrics() {}

  // Begins the external data collection.  Profile is passed through to
  // UserMetrics::RecordAction.  The lifetime of profile must exceed that of
  // the external metrics object.
  void Start(Profile* profile);

 private:
  // There is one function with this type for each action or histogram.
  typedef void (*RecordFunctionType)(const char*);
  // The type of event associated with each name.
  typedef enum {
    EVENT_TYPE_ACTION,
    EVENT_TYPE_HISTOGRAM
  } MetricsEventType;
  // Used in mapping names (C strings) into event-recording functions.
  typedef struct {
    const char* name;
    RecordFunctionType function;
    MetricsEventType type;
  } RecordFunctionTableEntry;
  typedef void (*RecorderType)(const char*, const char*);  // See SetRecorder.

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 4096;

  ~ExternalMetrics();

  // Protect action recorders from being called when external_metrics_profile is
  // null.  This could happen when testing, or in the unlikely case that the
  // order of object deletion at shutdown changes.
  static void RecordActionWrapper(RecordFunctionType);

  // Maps a name to an entry in the record function table.  Return NULL on
  // failure.
  static const ExternalMetrics::RecordFunctionTableEntry* FindRecordEntry(
      const char* name);

  // Initializes a table that maps a metric name to a function that logs that
  // metric.
  void InitializeFunctionTable();

  // Passes an event, either an ACTION or HISTOGRAM depending on |name|, to the
  // UMA service.  For a histogram, |value| contains the numeric value, in a
  // format that depends on |name|.
  static void RecordEvent(const char* name, const char* value);

  // Collects external events from metrics log file.  This is run at periodic
  // intervals.
  void CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a metrics event collection in the future.
  void ScheduleCollector();

  // Sets the event logging function.  Exists only because of testing.
  void SetRecorder(RecorderType recorder) {
    recorder_ = recorder;
  }

  // This table maps event names to event recording functions.
  static RecordFunctionTableEntry function_table_[];
  RecorderType recorder_;  // See SetRecorder.
  DISALLOW_COPY_AND_ASSIGN(ExternalMetrics);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_
