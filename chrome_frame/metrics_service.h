// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef CHROME_FRAME_METRICS_SERVICE_H_
#define CHROME_FRAME_METRICS_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "chrome/common/metrics_helpers.h"

// TODO(ananta)
// Refactor more common code from chrome/browser/metrics/metrics_service.h into
// the MetricsServiceBase class.
class MetricsService : public MetricsServiceBase {
 public:
  static MetricsService* GetInstance();
  // Start/stop the metrics recording and uploading machine.  These should be
  // used on startup and when the user clicks the checkbox in the prefs.
  // StartRecordingOnly starts the metrics recording but not reporting, for use
  // in tests only.
  static void Start();
  static void Stop();
  // Set up client ID, session ID, etc.
  void InitializeMetricsState();

 private:
  // To enable the default traits object to create an instance of this class.
  friend struct base::DefaultLazyInstanceTraits<MetricsService>;

  MetricsService();
  virtual ~MetricsService();
  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    INITIALIZED,            // Constructor was called.
    ACTIVE,                 // Accumalating log data
    STOPPED,                // Service has stopped
  };

  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, base::Histogram::SampleSet> LoggedSampleMap;

  // Sets and gets whether metrics recording is active.
  // SetRecording(false) also forces a persistent save of logging state (if
  // anything has been recorded, or transmitted).
  void SetRecording(bool enabled);

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  // Return value "true" indicates setting was definitively set as requested).
  // Return value of "false" indicates that the enable state is effectively
  // stuck in the other logical setting.
  // Google Update maintains the authoritative preference in the registry, so
  // the caller *might* not be able to actually change the setting.
  // It is always possible to set this to at least one value, which matches the
  // current value reported by querying Google Update.
  void SetReporting(bool enabled);

  // If in_idle is true, sets idle_since_last_transmission to true.
  // If in_idle is false and idle_since_last_transmission_ is true, sets
  // idle_since_last_transmission to false and starts the timer (provided
  // starting the timer is permitted).
  void HandleIdleSinceLastTransmission(bool in_idle);

  // Generates a new client ID to use to identify self to metrics server.
  static std::string GenerateClientID();

  // ChromeFrame UMA data is uploaded when this timer proc gets invoked.
  static void CALLBACK TransmissionTimerProc(HWND window, unsigned int message,
                                             unsigned int event_id,
                                             unsigned int time);

  // Called to start recording user experience metrics.
  // Constructs a new, empty current_log_.
  void StartRecording();

  // Called to stop recording user experience metrics.
  void StopRecording(bool save_log);

  // Takes whatever log should be uploaded next (according to the state_)
  // and makes it the pending log.  If pending_log_ is not NULL,
  // MakePendingLog does nothing and returns.
  void MakePendingLog();

  // Determines from state_ and permissions set out by the server and by
  // the user whether the pending_log_ should be sent or discarded.  Called by
  // TryToStartTransmission.
  bool TransmissionPermitted() const;

  bool recording_active() const {
    return recording_active_;
  }

  bool reporting_active() const {
    return reporting_active_;
  }

  // Convert pending_log_ to XML in pending_log_text_ for transmission.
  std::string PrepareLogSubmissionString();

  // Upload pending data to the server by converting it to XML and compressing
  // it. Returns true on success.
  bool UploadData();

  // Get the current version of the application as a string.
  static std::string GetVersionString();

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  bool recording_active_;
  bool reporting_active_;

  // Coincides with the check box in options window that lets the user control
  // whether to upload.
  bool user_permits_upload_;

  // The progession of states made by the browser are recorded in the following
  // state.
  State state_;

  // The URL for the metrics server.
  std::wstring server_url_;

  // The identifier that's sent to the server with the log reports.
  std::string client_id_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  base::PlatformThreadId thread_;

  // Indicates if this is the first uma upload from this instance.
  bool initial_uma_upload_;

  // The transmission timer id returned by SetTimer
  int transmission_timer_id_;

  // Used to serialize the Start and Stop operations on the metrics service.
  static base::Lock metrics_service_lock_;

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

#endif  // CHROME_FRAME_METRICS_SERVICE_H_
