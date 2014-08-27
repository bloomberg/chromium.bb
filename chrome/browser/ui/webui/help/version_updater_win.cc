// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/google/google_update_win.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

// Windows implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterWin : public VersionUpdater,
                          public GoogleUpdateStatusListener {
 private:
  friend class VersionReader;
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterWin();
  virtual ~VersionUpdaterWin();

  // VersionUpdater implementation.
  virtual void CheckForUpdate(const StatusCallback& callback) OVERRIDE;
  virtual void RelaunchBrowser() const OVERRIDE;

  // GoogleUpdateStatusListener implementation.
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const base::string16& error_message,
                               const base::string16& version) OVERRIDE;

  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code,
                    const base::string16& error_message);

  // Got the intalled version so the handling of the UPGRADE_ALREADY_UP_TO_DATE
  // result case can now be completeb on the UI thread.
  void GotInstalledVersion(const Version& version);

  // Little helper function to create google_updater_.
  void CreateGoogleUpdater();

  // Helper function to clear google_updater_.
  void ClearGoogleUpdater();

  // Returns a window that can be used for elevation.
  HWND GetElevationParent();

  // The class that communicates with Google Update to find out if an update is
  // available and asks it to start an upgrade.
  scoped_refptr<GoogleUpdate> google_updater_;

  // Used for callbacks.
  base::WeakPtrFactory<VersionUpdaterWin> weak_factory_;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterWin);
};

// This class is used to read the version on the FILE thread and then call back
// the version updater in the UI thread. Using a class helps better control
// the lifespan of the Version independently of the lifespan of the version
// updater, which may die while asynchonicity is happening, thus the usage of
// the WeakPtr, which can only be used from the thread that created it.
class VersionReader
    : public base::RefCountedThreadSafe<VersionReader> {
 public:
  explicit VersionReader(
      const base::WeakPtr<VersionUpdaterWin>& version_updater)
      : version_updater_(version_updater) {
  }

  void GetVersionFromFileThread() {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    InstallUtil::GetChromeVersion(dist, false, &installed_version_);
    if (!installed_version_.IsValid()) {
      // User-level Chrome is not installed, check system-level.
      InstallUtil::GetChromeVersion(dist, true, &installed_version_);
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
          &VersionReader::SetVersionInUIThread, this));
  }

  void SetVersionInUIThread() {
    if (version_updater_.get() != NULL)
      version_updater_->GotInstalledVersion(installed_version_);
  }

 private:
  friend class base::RefCountedThreadSafe<VersionReader>;

  // The version updater that must be called back when we are done.
  // We use a weak pointer in case the updater gets destroyed while waiting.
  base::WeakPtr<VersionUpdaterWin> version_updater_;

  // This is the version that gets read in the FILE thread and set on the
  // the updater in the UI thread.
  Version installed_version_;
};

VersionUpdaterWin::VersionUpdaterWin()
    : weak_factory_(this) {
  CreateGoogleUpdater();
}

VersionUpdaterWin::~VersionUpdaterWin() {
  // The Google Updater will hold a pointer to the listener until it reports
  // status, so that pointer must be cleared when the listener is destoyed.
  ClearGoogleUpdater();
}

void VersionUpdaterWin::CheckForUpdate(const StatusCallback& callback) {
  callback_ = callback;

  // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
  // off. So, in this case, the version updater must not mention
  // on-demand updates. Silent updates (in the background) should still
  // work as before - enabling UAC or installing the latest service pack
  // for Vista is another option.
  if (!(base::win::GetVersion() == base::win::VERSION_VISTA &&
        (base::win::OSInfo::GetInstance()->service_pack().major == 0) &&
        !base::win::UserAccountControlIsEnabled())) {
    // This could happen if the page got refreshed after results were returned.
    if (!google_updater_)
      CreateGoogleUpdater();
    UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR,
                 base::string16());
    // Specify false to not upgrade yet.
    google_updater_->CheckForUpdate(false, GetElevationParent());
  }
}

void VersionUpdaterWin::RelaunchBrowser() const {
  chrome::AttemptRestart();
}

void VersionUpdaterWin::OnReportResults(
    GoogleUpdateUpgradeResult result, GoogleUpdateErrorCode error_code,
    const base::string16& error_message, const base::string16& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  ClearGoogleUpdater();
  UpdateStatus(result, error_code, error_message);
}

void VersionUpdaterWin::UpdateStatus(GoogleUpdateUpgradeResult result,
                                     GoogleUpdateErrorCode error_code,
                                     const base::string16& error_message) {
  // For Chromium builds it would show an error message.
  // But it looks weird because in fact there is no error,
  // just the update server is not available for non-official builds.
#if defined(GOOGLE_CHROME_BUILD)
  Status status = UPDATED;
  base::string16 message;

  switch (result) {
    case UPGRADE_CHECK_STARTED: {
      content::RecordAction(UserMetricsAction("UpgradeCheck_Started"));
      status = CHECKING;
      break;
    }
    case UPGRADE_STARTED: {
      content::RecordAction(UserMetricsAction("Upgrade_Started"));
      status = UPDATING;
      break;
    }
    case UPGRADE_IS_AVAILABLE: {
      content::RecordAction(
          UserMetricsAction("UpgradeCheck_UpgradeIsAvailable"));
      DCHECK(!google_updater_);  // Should have been nulled out already.
      CreateGoogleUpdater();
      UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR, base::string16());
      // Specify true to upgrade now.
      google_updater_->CheckForUpdate(true, GetElevationParent());
      return;
    }
    case UPGRADE_ALREADY_UP_TO_DATE: {
      // Google Update reported that Chrome is up-to-date.
      // To confirm the updated version is running, the reading
      // must be done on the file thread. The rest of this case
      // will be handled within GotInstalledVersion.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
            &VersionReader::GetVersionFromFileThread,
            new VersionReader(weak_factory_.GetWeakPtr())));
      return;
    }
    case UPGRADE_SUCCESSFUL: {
      content::RecordAction(UserMetricsAction("UpgradeCheck_Upgraded"));
      status = NEARLY_UPDATED;
      break;
    }
    case UPGRADE_ERROR: {
      content::RecordAction(UserMetricsAction("UpgradeCheck_Error"));
      status = FAILED;
      if (error_code == GOOGLE_UPDATE_DISABLED_BY_POLICY) {
        message =
            l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY);
      } else if (error_code == GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY) {
        message =
            l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY_MANUAL);
      } else {
        message =
            l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code);
      }

      if (!error_message.empty()) {
        message +=
            l10n_util::GetStringFUTF16(IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK,
                                       error_message);
      }
      break;
    }
  }

  // TODO(mad): Get proper progress value instead of passing 0.
  // http://crbug.com/136117
  callback_.Run(status, 0, message);
#endif  //  defined(GOOGLE_CHROME_BUILD)
}

void VersionUpdaterWin::GotInstalledVersion(const Version& version) {
  // This must be called on the UI thread so that callback_ can be called.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Make sure that the latest version is running and if not,
  // notify the user by setting the status to NEARLY_UPDATED.
  //
  // The extra version check is necessary on Windows because the application
  // may be already up to date on disk though the running app is still
  // out of date.
  chrome::VersionInfo version_info;
  Version running_version(version_info.Version());
  if (!version.IsValid() || version.CompareTo(running_version) <= 0) {
    content::RecordAction(
        UserMetricsAction("UpgradeCheck_AlreadyUpToDate"));
    callback_.Run(UPDATED, 0, base::string16());
  } else {
    content::RecordAction(UserMetricsAction("UpgradeCheck_AlreadyUpgraded"));
    callback_.Run(NEARLY_UPDATED, 0, base::string16());
  }
}

void VersionUpdaterWin::CreateGoogleUpdater() {
  ClearGoogleUpdater();
  google_updater_ = new GoogleUpdate();
  google_updater_->set_status_listener(this);
}

void VersionUpdaterWin::ClearGoogleUpdater() {
  if (google_updater_) {
    google_updater_->set_status_listener(NULL);
    google_updater_ = NULL;
  }
}

BOOL CALLBACK WindowEnumeration(HWND window, LPARAM param) {
  if (IsWindowVisible(window)) {
    HWND* returned_window = reinterpret_cast<HWND*>(param);
    *returned_window = window;
    return FALSE;
  }
  return TRUE;
}

HWND VersionUpdaterWin::GetElevationParent() {
  // Look for a visible window belonging to the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HWND window = NULL;
  EnumThreadWindows(GetCurrentThreadId(),
                    WindowEnumeration,
                    reinterpret_cast<LPARAM>(&window));
#if !defined(USE_AURA)
  // If using Aura, we might not have a Visible window in this process. In
  // theory Google update can cope with that.
  DCHECK(window != NULL) << "Failed to find a valid window handle on thread: "
                         << GetCurrentThreadId();
#endif
  return window;
}

}  // namespace

VersionUpdater* VersionUpdater::Create() {
  return new VersionUpdaterWin;
}
