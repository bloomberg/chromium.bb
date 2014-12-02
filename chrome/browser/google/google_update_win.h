// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"
#include "google_update/google_update_idl.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class TaskRunner;
}  // namespace base

// The status of the upgrade. UPGRADE_STARTED and UPGRADE_CHECK_STARTED are
// internal states and will not be reported as results to the listener.
// These values are used for a histogram. Do not reorder.
enum GoogleUpdateUpgradeResult {
  // The upgrade has started.
  UPGRADE_STARTED = 0,
  // A check for upgrade has been initiated.
  UPGRADE_CHECK_STARTED = 1,
  // An update is available.
  UPGRADE_IS_AVAILABLE = 2,
  // The upgrade happened successfully.
  UPGRADE_SUCCESSFUL = 3,
  // No need to upgrade, Chrome is up to date.
  UPGRADE_ALREADY_UP_TO_DATE = 4,
  // An error occurred.
  UPGRADE_ERROR = 5,
  NUM_UPGRADE_RESULTS
};

// These values are used for a histogram. Do not reorder.
enum GoogleUpdateErrorCode {
  // The upgrade completed successfully (or hasn't been started yet).
  GOOGLE_UPDATE_NO_ERROR = 0,
  // Google Update only supports upgrading if Chrome is installed in the default
  // location. This error will appear for developer builds and with
  // installations unzipped to random locations.
  CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY = 1,
  // Failed to create Google Update JobServer COM class.
  GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED = 2,
  // Failed to create Google Update OnDemand COM class.
  GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND = 3,
  // Google Update OnDemand COM class reported an error during a check for
  // update (or while upgrading).
  GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR = 4,
  // A call to GetResults failed. DEPRECATED.
  // GOOGLE_UPDATE_GET_RESULT_CALL_FAILED = 5,
  // A call to GetVersionInfo failed. DEPRECATED
  // GOOGLE_UPDATE_GET_VERSION_INFO_FAILED = 6,
  // An error occurred while upgrading (or while checking for update).
  // Check the Google Update log in %TEMP% for more details.
  GOOGLE_UPDATE_ERROR_UPDATING = 7,
  // Updates can not be downloaded because the administrator has disabled all
  // types of updating.
  GOOGLE_UPDATE_DISABLED_BY_POLICY = 8,
  // Updates can not be downloaded because the administrator has disabled
  // manual (on-demand) updates.  Automatic background updates are allowed.
  GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY = 9,
  NUM_ERROR_CODES
};

// A callback run when a check for updates has completed. |result| indicates the
// end state of the operation. When |result| is UPGRADE_ERROR, |error_code| and
// |error_message| indicate the the nature of the error. When |result| is
// UPGRADE_IS_AVAILABLE or UPGRADE_SUCCESSFUL, |version| may indicate the new
// version Google Update detected (it may, however, be empty).
typedef base::Callback<void(GoogleUpdateUpgradeResult result,
                            GoogleUpdateErrorCode error_code,
                            const base::string16& error_message,
                            const base::string16& version)> UpdateCheckCallback;

// Begins an asynchronous update check on |task_runner|, which must run a
// TYPE_UI message loop. If |install_if_newer| is true, an update will be
// applied. |elevation_window| is the window which should own any necessary
// elevation UI. |callback| will be run on the caller's thread when the check
// completes.
void BeginUpdateCheck(const scoped_refptr<base::TaskRunner>& task_runner,
                      bool install_if_newer,
                      gfx::AcceleratedWidget elevation_window,
                      const UpdateCheckCallback& callback);

// A type of callback supplied by tests to provide a custom IGoogleUpdate
// implementation.
typedef base::Callback<HRESULT(base::win::ScopedComPtr<IGoogleUpdate>*)>
    OnDemandAppsClassFactory;

// For use by tests that wish to provide a custom IGoogleUpdate implementation
// independent of Google Update's.
void SetGoogleUpdateFactoryForTesting(
    const OnDemandAppsClassFactory& google_update_factory);

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_
