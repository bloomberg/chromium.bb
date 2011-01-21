// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/task.h"

namespace chromeos {

// ExternalMetrics is a service that Chrome offers to Chrome OS to upload
// metrics to the UMA server on its behalf.  Chrome periodically reads the
// content of a well-know file, and parses it into name-value pairs, each
// representing a Chrome OS metrics event. The events are logged using the
// normal UMA mechanism. The file is then truncated to zero size. Chrome uses
// flock() to synchronize accesses to the file.
class ExternalMetrics : public base::RefCountedThreadSafe<ExternalMetrics> {
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, ParseExternalMetricsFile);
  friend class base::RefCountedThreadSafe<ExternalMetrics>;

 public:
  ExternalMetrics();

  // Begins the external data collection.  This service is started and stopped
  // by the chrome metrics service.  Calls to RecordAction originate in the
  // File thread but are executed in the UI thread.
  void Start();

 private:
  // There is one function with this type for each action.
  typedef void (*RecordFunctionType)();

  typedef void (*RecorderType)(const char*, const char*);  // For testing only.

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 1024;  // be generous

  ~ExternalMetrics() {}

  // Passes an action event to the UMA service on the UI thread.
  void RecordActionUI(std::string action_string);

  // Passes an action event to the UMA service.
  void RecordAction(const char* action_name);

  // Records an external crash of the given string description to
  // UMA service on the UI thread.
  void RecordCrashUI(const std::string& crash_kind);

  // Records an external crash of the given string description.
  void RecordCrash(const std::string& crash_kind);

  // Passes an histogram event to the UMA service.  |histogram_data| is in the
  // form <histogram-name> <sample> <min> <max> <buckets_count>.
  void RecordHistogram(const char* histogram_data);

  // Passes a linear histogram event to the UMA service.  |histogram_data| is
  // in the form <histogram-name> <sample> <max>.
  void RecordLinearHistogram(const char* histogram_data);

  // Collects external events from metrics log file.  This is run at periodic
  // intervals.
  void CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a metrics event collection in the future.
  void ScheduleCollector();

  // Maps histogram or action names to recorder structs.
  base::hash_map<std::string, RecordFunctionType> action_recorders_;

  // Set containing known user actions.
  base::hash_set<std::string> valid_user_actions_;

  // Used for testing only.
  RecorderType test_recorder_;
  FilePath test_path_;
  DISALLOW_COPY_AND_ASSIGN(ExternalMetrics);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTERNAL_METRICS_H_
