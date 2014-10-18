// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_win.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_update/google_update_idl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/atl_module.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace {

// Check if the currently running instance can be updated by Google Update.
// Returns GOOGLE_UPDATE_NO_ERROR only if the instance running is a Google
// Chrome distribution installed in a standard location.
GoogleUpdateErrorCode CanUpdateCurrentChrome(
    const base::FilePath& chrome_exe_path) {
#if !defined(GOOGLE_CHROME_BUILD)
  return CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY;
#else
  // TODO(tommi): Check if using the default distribution is always the right
  // thing to do.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::FilePath user_exe_path = installer::GetChromeInstallPath(false, dist);
  base::FilePath machine_exe_path = installer::GetChromeInstallPath(true, dist);
  if (!base::FilePath::CompareEqualIgnoreCase(chrome_exe_path.value(),
                                        user_exe_path.value()) &&
      !base::FilePath::CompareEqualIgnoreCase(chrome_exe_path.value(),
                                        machine_exe_path.value())) {
    LOG(ERROR) << L"Google Update cannot update Chrome installed in a "
               << L"non-standard location: " << chrome_exe_path.value().c_str()
               << L". The standard location is: "
                   << user_exe_path.value().c_str()
               << L" or " << machine_exe_path.value().c_str() << L".";
    return CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY;
  }

  base::string16 app_guid = installer::GetAppGuidForUpdates(
      !InstallUtil::IsPerUserInstall(chrome_exe_path.value().c_str()));
  DCHECK(!app_guid.empty());

  GoogleUpdateSettings::UpdatePolicy update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, NULL);

  if (update_policy == GoogleUpdateSettings::UPDATES_DISABLED)
    return GOOGLE_UPDATE_DISABLED_BY_POLICY;

  if (update_policy == GoogleUpdateSettings::AUTO_UPDATES_ONLY)
    return GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY;

  return GOOGLE_UPDATE_NO_ERROR;
#endif
}

// Creates an instance of a COM Local Server class using either plain vanilla
// CoCreateInstance, or using the Elevation moniker if running on Vista.
// hwnd must refer to a foregound window in order to get the UAC prompt
// showing up in the foreground if running on Vista. It can also be NULL if
// background UAC prompts are desired.
HRESULT CoCreateInstanceAsAdmin(REFCLSID class_id, REFIID interface_id,
                                HWND hwnd, void** interface_ptr) {
  if (!interface_ptr)
    return E_POINTER;

  // For Vista, need to instantiate the COM server via the elevation
  // moniker. This ensures that the UAC dialog shows up.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    wchar_t class_id_as_string[MAX_PATH] = {0};
    StringFromGUID2(class_id, class_id_as_string,
                    arraysize(class_id_as_string));

    base::string16 elevation_moniker_name =
        base::StringPrintf(L"Elevation:Administrator!new:%ls",
                           class_id_as_string);

    BIND_OPTS3 bind_opts;
    memset(&bind_opts, 0, sizeof(bind_opts));
    bind_opts.cbStruct = sizeof(bind_opts);
    bind_opts.dwClassContext = CLSCTX_LOCAL_SERVER;
    bind_opts.hwnd = hwnd;

    return CoGetObject(elevation_moniker_name.c_str(), &bind_opts,
                       interface_id, reinterpret_cast<void**>(interface_ptr));
  }

  return CoCreateInstance(class_id, NULL, CLSCTX_LOCAL_SERVER,
                          interface_id,
                          reinterpret_cast<void**>(interface_ptr));
}


}  // namespace

// The GoogleUpdateJobObserver COM class is responsible for receiving status
// reports from google Update. It keeps track of the progress as GoogleUpdate
// notifies this observer and ends the message loop that is spinning in once
// GoogleUpdate reports that it is done.
class GoogleUpdateJobObserver
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IJobObserver {
 public:
  BEGIN_COM_MAP(GoogleUpdateJobObserver)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  GoogleUpdateJobObserver()
    : result_(UPGRADE_ERROR) {
  }
  virtual ~GoogleUpdateJobObserver() {}

  // Notifications from Google Update:
  STDMETHOD(OnShow)() {
    return S_OK;
  }
  STDMETHOD(OnCheckingForUpdate)() {
    result_ = UPGRADE_CHECK_STARTED;
    return S_OK;
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    result_ = UPGRADE_IS_AVAILABLE;
    new_version_ = version_string;
    return S_OK;
  }
  STDMETHOD(OnWaitingToDownload)() {
    return S_OK;
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    return S_OK;
  }
  STDMETHOD(OnWaitingToInstall)() {
    return S_OK;
  }
  STDMETHOD(OnInstalling)() {
    result_ = UPGRADE_STARTED;
    return S_OK;
  }
  STDMETHOD(OnPause)() {
    return S_OK;
  }
  STDMETHOD(OnComplete)(LegacyCompletionCodes code, const TCHAR* text) {
    switch (code) {
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
      case COMPLETION_CODE_SUCCESS: {
        if (result_ == UPGRADE_STARTED)
          result_ = UPGRADE_SUCCESSFUL;
        else if (result_ == UPGRADE_CHECK_STARTED)
          result_ = UPGRADE_ALREADY_UP_TO_DATE;
        break;
      }
      case COMPLETION_CODE_ERROR:
        error_message_ = text;
      default: {
        NOTREACHED();
        result_ = UPGRADE_ERROR;
        break;
      }
    }

    event_sink_ = NULL;

    // No longer need to spin the message loop that started spinning in
    // InitiateGoogleUpdateCheck.
    base::MessageLoop::current()->Quit();
    return S_OK;
  }
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    event_sink_ = event_sink;
    return S_OK;
  }

  // Returns the results of the update operation.
  STDMETHOD(GetResult)(GoogleUpdateUpgradeResult* result) {
    // Intermediary steps should never be reported to the client.
    DCHECK(result_ != UPGRADE_STARTED && result_ != UPGRADE_CHECK_STARTED);

    *result = result_;
    return S_OK;
  }

  // Returns which version Google Update found on the server (if a more
  // recent version was found). Otherwise, this will be blank.
  STDMETHOD(GetVersionInfo)(base::string16* version_string) {
    *version_string = new_version_;
    return S_OK;
  }

  // Returns the Google Update supplied error string that describes the error
  // that occurred during the update check/upgrade.
  STDMETHOD(GetErrorMessage)(base::string16* error_message) {
    *error_message = error_message_;
    return S_OK;
  }

 private:
  // The status/result of the Google Update operation.
  GoogleUpdateUpgradeResult result_;

  // The version string Google Update found.
  base::string16 new_version_;

  // An error message, if any.
  base::string16 error_message_;

  // Allows us control the upgrade process to a small degree. After OnComplete
  // has been called, this object can not be used.
  base::win::ScopedComPtr<IProgressWndEvents> event_sink_;
};

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, public:

GoogleUpdate::GoogleUpdate()
    : listener_(NULL) {
}

GoogleUpdate::~GoogleUpdate() {
}

void GoogleUpdate::CheckForUpdate(bool install_if_newer, HWND window) {
  // Need to shunt this request over to InitiateGoogleUpdateCheck and have
  // it run in the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GoogleUpdate::InitiateGoogleUpdateCheck, this,
                 install_if_newer, window, base::MessageLoop::current()));
}

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, private:

void GoogleUpdate::InitiateGoogleUpdateCheck(bool install_if_newer,
                                             HWND window,
                                             base::MessageLoop* main_loop) {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::DIR_EXE, &chrome_exe))
    NOTREACHED();

  GoogleUpdateErrorCode error_code = CanUpdateCurrentChrome(chrome_exe);
  if (error_code != GOOGLE_UPDATE_NO_ERROR) {
    main_loop->PostTask(
        FROM_HERE,
        base::Bind(&GoogleUpdate::ReportResults, this,
                   UPGRADE_ERROR, error_code, base::string16()));
    return;
  }

  // Make sure ATL is initialized in this module.
  ui::win::CreateATLModuleIfNeeded();

  CComObject<GoogleUpdateJobObserver>* job_observer;
  HRESULT hr =
      CComObject<GoogleUpdateJobObserver>::CreateInstance(&job_observer);
  if (hr != S_OK) {
    // Most of the error messages come straight from Google Update. This one is
    // deemed worthy enough to also warrant its own error.
    GoogleUpdateErrorCode error = GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED;
    base::string16 error_code = base::StringPrintf(L"%d: 0x%x", error, hr);
    ReportFailure(
        hr, error,
        l10n_util::GetStringFUTF16(IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED,
                                   error_code),
        main_loop);
    return;
  }

  base::win::ScopedComPtr<IJobObserver> job_holder(job_observer);

  base::win::ScopedComPtr<IGoogleUpdate> on_demand;

  bool system_level = false;

  if (InstallUtil::IsPerUserInstall(chrome_exe.value().c_str())) {
    hr = on_demand.CreateInstance(CLSID_OnDemandUserAppsClass);
  } else {
    // The Update operation needs Admin privileges for writing
    // to %ProgramFiles%. On Vista, need to elevate before instantiating
    // the updater instance.
    if (!install_if_newer) {
      hr = on_demand.CreateInstance(CLSID_OnDemandMachineAppsClass);
    } else {
      hr = CoCreateInstanceAsAdmin(CLSID_OnDemandMachineAppsClass,
          IID_IGoogleUpdate, window,
          reinterpret_cast<void**>(on_demand.Receive()));
    }
    system_level = true;
  }

  if (hr != S_OK) {
    GoogleUpdateErrorCode error = GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND;
    base::string16 error_code = base::StringPrintf(L"%d: 0x%x", error, hr);
    if (system_level)
      error_code += L" -- system level";
    ReportFailure(hr, error,
                  l10n_util::GetStringFUTF16(
                      IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED,
                      error_code),
                  main_loop);
    return;
  }

  base::string16 app_guid = installer::GetAppGuidForUpdates(system_level);
  DCHECK(!app_guid.empty());

  if (!install_if_newer)
    hr = on_demand->CheckForUpdate(app_guid.c_str(), job_observer);
  else
    hr = on_demand->Update(app_guid.c_str(), job_observer);

  if (hr != S_OK) {
    GoogleUpdateErrorCode error = GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR;
    base::string16 error_code = base::StringPrintf(L"%d: 0x%x", error, hr);
    ReportFailure(hr, error,
                  l10n_util::GetStringFUTF16(
                      IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED,
                      error_code),
                  main_loop);
    return;
  }

  // Need to spin the message loop while Google Update is running so that it
  // can report back to us through GoogleUpdateJobObserver. This message loop
  // will terminate once Google Update sends us the completion status
  // (success/error). See OnComplete().
  base::MessageLoop::current()->Run();

  GoogleUpdateUpgradeResult results;
  hr = job_observer->GetResult(&results);

  if (hr != S_OK) {
    GoogleUpdateErrorCode error = GOOGLE_UPDATE_GET_RESULT_CALL_FAILED;
    base::string16 error_code = base::StringPrintf(L"%d: 0x%x", error, hr);
    ReportFailure(hr, error,
                  l10n_util::GetStringFUTF16(
                      IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED,
                      error_code),
                  main_loop);
    return;
  }

  if (results == UPGRADE_ERROR) {
    base::string16 error_message;
    job_observer->GetErrorMessage(&error_message);
    ReportFailure(hr, GOOGLE_UPDATE_ERROR_UPDATING, error_message, main_loop);
    return;
  }

  hr = job_observer->GetVersionInfo(&version_available_);
  if (hr != S_OK) {
    GoogleUpdateErrorCode error = GOOGLE_UPDATE_GET_VERSION_INFO_FAILED;
    base::string16 error_code = base::StringPrintf(L"%d: 0x%x", error, hr);
    ReportFailure(hr, error,
                  l10n_util::GetStringFUTF16(
                      IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED,
                      error_code),
                  main_loop);
    return;
  }

  main_loop->PostTask(
      FROM_HERE,
      base::Bind(&GoogleUpdate::ReportResults, this,
                 results, GOOGLE_UPDATE_NO_ERROR, base::string16()));
  job_holder = NULL;
  on_demand = NULL;
}

void GoogleUpdate::ReportResults(GoogleUpdateUpgradeResult results,
                                 GoogleUpdateErrorCode error_code,
                                 const base::string16& error_message) {
  // If there is an error, then error code must not be blank, and vice versa.
  DCHECK(results == UPGRADE_ERROR ? error_code != GOOGLE_UPDATE_NO_ERROR :
                                    error_code == GOOGLE_UPDATE_NO_ERROR);
  UMA_HISTOGRAM_ENUMERATION(
      "GoogleUpdate.UpgradeResult", results, NUM_UPGRADE_RESULTS);
  if (results == UPGRADE_ERROR) {
    UMA_HISTOGRAM_ENUMERATION(
        "GoogleUpdate.UpdateErrorCode", error_code, NUM_ERROR_CODES);
  }
  if (listener_) {
    listener_->OnReportResults(
        results, error_code, error_message, version_available_);
  }
}

bool GoogleUpdate::ReportFailure(HRESULT hr,
                                 GoogleUpdateErrorCode error_code,
                                 const base::string16& error_message,
                                 base::MessageLoop* main_loop) {
  DLOG(ERROR) << "Communication with Google Update failed: " << hr
              << " error: " << error_code
              << ", message: " << error_message.c_str();
  UMA_HISTOGRAM_SPARSE_SLOWLY("GoogleUpdate.ErrorHresult", hr);
  main_loop->PostTask(
      FROM_HERE,
      base::Bind(&GoogleUpdate::ReportResults, this,
                 UPGRADE_ERROR, error_code, error_message));
  return false;
}
