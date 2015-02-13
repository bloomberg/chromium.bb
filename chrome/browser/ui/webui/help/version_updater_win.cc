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
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace {

// Windows implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterWin : public VersionUpdater {
 private:
  friend class VersionReader;
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterWin();
  virtual ~VersionUpdaterWin();

  // VersionUpdater implementation.
  virtual void CheckForUpdate(const StatusCallback& callback) override;
  virtual void RelaunchBrowser() const override;

  // chrome::UpdateCheckCallback.
  void OnUpdateCheckResults(GoogleUpdateUpgradeResult result,
                            GoogleUpdateErrorCode error_code,
                            const base::string16& error_message,
                            const base::string16& version);

  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code,
                    const base::string16& error_message);

  // Got the intalled version so the handling of the UPGRADE_ALREADY_UP_TO_DATE
  // result case can now be completeb on the UI thread.
  void GotInstalledVersion(const Version& version);

  // Returns a window that can be used for elevation.
  gfx::AcceleratedWidget GetElevationParent();

  void BeginUpdateCheckOnFileThread(bool install_if_newer);

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
}

VersionUpdaterWin::~VersionUpdaterWin() {
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
    UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR,
                 base::string16());
    // Specify false to not upgrade yet.
    BeginUpdateCheckOnFileThread(false);
  }
}

void VersionUpdaterWin::RelaunchBrowser() const {
  chrome::AttemptRestart();
}

void VersionUpdaterWin::OnUpdateCheckResults(
    GoogleUpdateUpgradeResult result,
    GoogleUpdateErrorCode error_code,
    const base::string16& error_message,
    const base::string16& version) {
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
      status = CHECKING;
      break;
    }
    case UPGRADE_STARTED: {
      status = UPDATING;
      break;
    }
    case UPGRADE_IS_AVAILABLE: {
      UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR, base::string16());
      // Specify true to upgrade now.
      BeginUpdateCheckOnFileThread(true);
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
      status = NEARLY_UPDATED;
      break;
    }
    case UPGRADE_ERROR: {
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
  callback_.Run((version.IsValid() && version.CompareTo(running_version) > 0)
                    ? NEARLY_UPDATED
                    : UPDATED,
                0,
                base::string16());
}

BOOL CALLBACK WindowEnumeration(HWND window, LPARAM param) {
  if (IsWindowVisible(window)) {
    HWND* returned_window = reinterpret_cast<HWND*>(param);
    *returned_window = window;
    return FALSE;
  }
  return TRUE;
}

gfx::AcceleratedWidget VersionUpdaterWin::GetElevationParent() {
  // Look for a visible window belonging to the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HWND window = NULL;
  EnumThreadWindows(GetCurrentThreadId(),
                    WindowEnumeration,
                    reinterpret_cast<LPARAM>(&window));
  return window;
}

void VersionUpdaterWin::BeginUpdateCheckOnFileThread(bool install_if_newer) {
  scoped_refptr<base::TaskRunner> task_runner(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  BeginUpdateCheck(task_runner, install_if_newer, GetElevationParent(),
                   base::Bind(&VersionUpdaterWin::OnUpdateCheckResults,
                              weak_factory_.GetWeakPtr()));
}

}  // namespace

VersionUpdater* VersionUpdater::Create(content::BrowserContext* /* context */) {
  return new VersionUpdaterWin;
}
