// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_

#include <memory>

#include "base/callback.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

struct DeviceLocalAccount;

// Groups parameters that are necessary either directly in the
// |StatusCollectorCallback| or in async methods that work as callbacks and
// expect these exact same parameters. Absence of values indicates errors or
// that status reporting is disabled.
//
// Notice that the fields are used in different contexts, depending on the type
// of user:
// - Enterprise users: |device_status| and |session_status|.
// - Child users:
//    - During the migration away from DeviceStatusCollector:
//      |device_status|, |session_status|, |child_status|.
//    - After migration: only |child_status|.
struct StatusCollectorParams {
  StatusCollectorParams();
  ~StatusCollectorParams();

  // Move only.
  StatusCollectorParams(StatusCollectorParams&&);
  StatusCollectorParams& operator=(StatusCollectorParams&&);

  std::unique_ptr<enterprise_management::DeviceStatusReportRequest>
      device_status;
  std::unique_ptr<enterprise_management::SessionStatusReportRequest>
      session_status;
  std::unique_ptr<enterprise_management::ChildStatusReportRequest> child_status;
};

// Called in the UI thread after the statuses have been collected
// asynchronously.
using StatusCollectorCallback =
    base::RepeatingCallback<void(StatusCollectorParams)>;

// Defines the API for a status collector.
class StatusCollector {
 public:
  StatusCollector() = default;
  virtual ~StatusCollector() = default;

  // Gathers status information and calls the passed response callback.
  virtual void GetStatusAsync(const StatusCollectorCallback& callback) = 0;

  // Called after the status information has successfully been submitted to
  // the server.
  virtual void OnSubmittedSuccessfully() = 0;

  // Methods used to decide whether a specific categories of data should be
  // included in the reports or not. See:
  // https://cs.chromium.org/search/?q=AddDeviceReportingInfo
  virtual bool ShouldReportActivityTimes() const = 0;
  virtual bool ShouldReportNetworkInterfaces() const = 0;
  virtual bool ShouldReportUsers() const = 0;
  virtual bool ShouldReportHardwareStatus() const = 0;

  // Returns the DeviceLocalAccount associated with the currently active kiosk
  // session, if the session was auto-launched with zero delay (this enables
  // functionality such as network reporting). If it isn't a kiosk session,
  // nullptr is returned.
  virtual std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo();
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
