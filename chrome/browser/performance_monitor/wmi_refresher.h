// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_WMI_REFRESHER_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_WMI_REFRESHER_H_

#include <wrl/client.h>

#include <atomic>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/system/system_monitor.h"
#include "base/win/wmi.h"

namespace performance_monitor {
namespace win {

// Wrapper around an IWbemRefresher instance used to retrieve some system
// performance counters.
//
// The constructor and InitializeDiskIdleTimeConfig() don't have any sequencing
// requirements. Calls to RefreshAndGetDiskIdleTimeInPercent() must happen on
// the same sequence, after InitializeDiskIdleTimeConfig() has returned. All
// methods except the constructor must be invoked in a context that accepts
// blocking calls (e.g. a MayBlock() sequence).
//
// This class relies on the fact that by default all sequences are in a
// multithreaded COM apartment.
class WMIRefresher {
 public:
  WMIRefresher();
  ~WMIRefresher();

  // Initialize everything needed to retrieve the disk idle time data via WMI.
  // This should only be called once.
  //
  // Returns true on success, false otherwise.
  bool InitializeDiskIdleTimeConfig();

  // Refresh the disk idle time data from WMI. This cannot be called until the
  // call to InitializeDiskIdleTimeConfig has completed.
  //
  // Returns the disk idle time in percent if available (between 0.0 and 100.0),
  // base::nullopt otherwise.
  base::Optional<float> RefreshAndGetDiskIdleTimeInPercent();

 private:
  enum class InitStatus;
  enum class RefreshStatus;

  // Implementation of |Start|.
  void InitializeDiskIdleTimeConfigImpl(InitStatus* res);

  // Implementation of |Refresh|.
  base::Optional<float> RefreshAndGetDiskIdleTimeInPercentImpl(
      RefreshStatus* res);

  // The WMI service instance.
  Microsoft::WRL::ComPtr<IWbemServices> wmi_services_;

  // The refresher responsible for collecting some local performance data via
  // WMI. |wmi_refresher_->Refresh()| should be called at regular intervals to
  // refresh the data.
  Microsoft::WRL::ComPtr<IWbemRefresher> wmi_refresher_;

  // The WMI enumerator.
  Microsoft::WRL::ComPtr<IWbemHiPerfEnum> wmi_refresher_enum_;

  // The latest number of objects returned by the refresher, initialized to 2 as
  // it's the minimal number of objects returned by the refresher (one for the
  // "Total" value and one per hard drive).
  size_t wmi_refresher_enum_objects_latest_count_ = 2;

  // The handle to the properties that need to be read in |Refresh|.
  base::Optional<long> percent_idle_time_prop_handle_;
  base::Optional<long> percent_idle_time_base_prop_handle_;

  // Latest values that have been read in |Refresh|.
  base::Optional<DWORD> latest_percent_idle_time_val_;
  base::Optional<DWORD> latest_percent_idle_time_base_val_;

  // Indicates if |InitializeDiskIdleTimeConfig| has already been called.
  std::atomic<bool> initialized_called_;

  // Indicates if everything is properly initialized and if this object is
  // ready to be used.
  bool refresh_ready_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WMIRefresher);
};

}  // namespace win
}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_WMI_REFRESHER_H_
