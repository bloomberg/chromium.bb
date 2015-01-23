// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_win.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/atl_module.h"

namespace {

OnDemandAppsClassFactory* g_google_update_factory = nullptr;

// Check if the currently running instance can be updated by Google Update.
// Returns GOOGLE_UPDATE_NO_ERROR only if the instance running is a Google
// Chrome distribution installed in a standard location.
GoogleUpdateErrorCode CanUpdateCurrentChrome(
    const base::FilePath& chrome_exe_path,
    bool system_level) {
#if !defined(GOOGLE_CHROME_BUILD)
  return CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY;
#else
  DCHECK_NE(InstallUtil::IsPerUserInstall(chrome_exe_path), system_level);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::FilePath user_exe_path = installer::GetChromeInstallPath(false, dist);
  base::FilePath machine_exe_path = installer::GetChromeInstallPath(true, dist);
  if (!base::FilePath::CompareEqualIgnoreCase(chrome_exe_path.value(),
                                        user_exe_path.value()) &&
      !base::FilePath::CompareEqualIgnoreCase(chrome_exe_path.value(),
                                        machine_exe_path.value())) {
    return CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY;
  }

  base::string16 app_guid = installer::GetAppGuidForUpdates(system_level);
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
HRESULT CoCreateInstanceAsAdmin(REFCLSID class_id,
                                REFIID interface_id,
                                gfx::AcceleratedWidget hwnd,
                                void** interface_ptr) {
  if (!interface_ptr)
    return E_POINTER;

  // For Vista, need to instantiate the COM server via the elevation
  // moniker. This ensures that the UAC dialog shows up.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    wchar_t class_id_as_string[MAX_PATH] = {};
    StringFromGUID2(class_id, class_id_as_string,
                    arraysize(class_id_as_string));

    base::string16 elevation_moniker_name =
        base::StringPrintf(L"Elevation:Administrator!new:%ls",
                           class_id_as_string);

    BIND_OPTS3 bind_opts;
    // An explicit memset is needed rather than relying on value initialization
    // since BIND_OPTS3 is not an aggregate (it is a derived type).
    memset(&bind_opts, 0, sizeof(bind_opts));
    bind_opts.cbStruct = sizeof(bind_opts);
    bind_opts.dwClassContext = CLSCTX_LOCAL_SERVER;
    bind_opts.hwnd = hwnd;

    return CoGetObject(elevation_moniker_name.c_str(), &bind_opts, interface_id,
                       interface_ptr);
  }

  return CoCreateInstance(class_id, NULL, CLSCTX_LOCAL_SERVER, interface_id,
                          interface_ptr);
}

HRESULT CreateOnDemandAppsClass(
    bool system_level,
    bool install_if_newer,
    gfx::AcceleratedWidget elevation_window,
    base::win::ScopedComPtr<IGoogleUpdate>* on_demand) {
  if (g_google_update_factory)
    return g_google_update_factory->Run(on_demand);

  // For a user-level install, update checks and updates can both be done by a
  // normal user with the UserAppsClass.
  if (!system_level)
    return on_demand->CreateInstance(CLSID_OnDemandUserAppsClass);

  // For a system-level install, update checks can be done by a normal user with
  // the MachineAppsClass.
  if (!install_if_newer)
    return on_demand->CreateInstance(CLSID_OnDemandMachineAppsClass);

  // For a system-level install, an update requires Admin privileges for writing
  // to %ProgramFiles%. Elevate while instantiating the MachineAppsClass.
  return CoCreateInstanceAsAdmin(CLSID_OnDemandMachineAppsClass,
                                 IID_IGoogleUpdate, elevation_window,
                                 on_demand->ReceiveVoid());
}


// GoogleUpdateJobObserver -----------------------------------------------------

// The GoogleUpdateJobObserver COM class is responsible for receiving status
// reports from google Update. It keeps track of the progress as Google Update
// notifies this observer and runs a completion callback once Google Update
// reports that it is done.
class GoogleUpdateJobObserver : public CComObjectRootEx<CComSingleThreadModel>,
                                public IJobObserver {
 public:
  BEGIN_COM_MAP(GoogleUpdateJobObserver)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  GoogleUpdateJobObserver();
  virtual ~GoogleUpdateJobObserver();

  // Sets the callback to be invoked when Google Update reports that the job is
  // done.
  void set_on_complete_callback(const base::Closure& on_complete_callback) {
    on_complete_callback_ = on_complete_callback;
  }

  // Returns the results of the update operation.
  GoogleUpdateUpgradeResult result() const {
    // Intermediary steps should never be reported to the client.
    DCHECK_NE(UPGRADE_STARTED, result_);
    DCHECK_NE(UPGRADE_CHECK_STARTED, result_);
    return result_;
  }

  // Returns which version Google Update found on the server (if a more
  // recent version was found). Otherwise, this will be blank.
  base::string16 new_version() const { return new_version_; }

  // Returns the Google Update supplied error string that describes the error
  // that occurred during the update check/upgrade.
  base::string16 error_message() const { return error_message_; }

 private:
  // IJobObserver:
  STDMETHOD(OnShow)();
  STDMETHOD(OnCheckingForUpdate)();
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string);
  STDMETHOD(OnWaitingToDownload)();
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos);
  STDMETHOD(OnWaitingToInstall)();
  STDMETHOD(OnInstalling)();
  STDMETHOD(OnPause)();
  STDMETHOD(OnComplete)(LegacyCompletionCodes code, const TCHAR* text);
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink);

  // The task runner associated with the thread in which the job runs.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // A callback to be run to complete processing;
  base::Closure on_complete_callback_;

  // The status/result of the Google Update operation.
  GoogleUpdateUpgradeResult result_;

  // The version string Google Update found.
  base::string16 new_version_;

  // An error message, if any.
  base::string16 error_message_;

  // Allows us control the upgrade process to a small degree. After OnComplete
  // has been called, this object can not be used.
  base::win::ScopedComPtr<IProgressWndEvents> event_sink_;

  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateJobObserver);
};

GoogleUpdateJobObserver::GoogleUpdateJobObserver()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      result_(UPGRADE_ERROR) {
}

GoogleUpdateJobObserver::~GoogleUpdateJobObserver() {
}

STDMETHODIMP GoogleUpdateJobObserver::OnShow() {
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnCheckingForUpdate() {
  result_ = UPGRADE_CHECK_STARTED;
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnUpdateAvailable(
    const TCHAR* version_string) {
  result_ = UPGRADE_IS_AVAILABLE;
  new_version_ = version_string;
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnWaitingToDownload() {
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnDownloading(int time_remaining_ms,
                                                    int pos) {
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnWaitingToInstall() {
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnInstalling() {
  result_ = UPGRADE_STARTED;
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnPause() {
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::OnComplete(LegacyCompletionCodes code,
                                                 const TCHAR* text) {
  if (code == COMPLETION_CODE_ERROR) {
    error_message_ = text;
    result_ = UPGRADE_ERROR;
  } else {
    // Everything that isn't an error is some form of success. Chrome doesn't
    // support any of the fancy codes (e.g., COMPLETION_CODE_REBOOT), but they
    // shouldn't be generated anyway.
    LOG_IF(DFATAL, (code != COMPLETION_CODE_SUCCESS &&
                    code != COMPLETION_CODE_SUCCESS_CLOSE_UI))
        << "Unexpected LegacyCompletionCode from IGoogleUpdate: " << code;
    if (result_ == UPGRADE_STARTED)
      result_ = UPGRADE_SUCCESSFUL;
    else if (result_ == UPGRADE_CHECK_STARTED)
      result_ = UPGRADE_ALREADY_UP_TO_DATE;
  }

  event_sink_ = NULL;

  task_runner_->PostTask(FROM_HERE, on_complete_callback_);
  return S_OK;
}

STDMETHODIMP GoogleUpdateJobObserver::SetEventSink(
    IProgressWndEvents* event_sink) {
  event_sink_ = event_sink;
  return S_OK;
}


// UpdateCheckDriver -----------------------------------------------------------

// A driver that is created and destroyed on the caller's thread and drives
// Google Update on another.
class UpdateCheckDriver {
 public:
  // Runs an update check on |task_runner|, invoking |callback| on the caller's
  // thread upon completion. |task_runner| must run a TYPE_UI message loop if
  // the default IGoogleUpdate on-demand COM class is used.
  static void RunUpdateCheck(const scoped_refptr<base::TaskRunner>& task_runner,
                             bool install_if_newer,
                             gfx::AcceleratedWidget elevation_window,
                             const UpdateCheckCallback& callback);

 private:
  friend class base::DeleteHelper<UpdateCheckDriver>;

  explicit UpdateCheckDriver(const UpdateCheckCallback& callback);

  // Runs the caller's update check callback with the results of the operation.
  ~UpdateCheckDriver();

  // Starts an update check.
  void BeginUpdateCheck(bool install_if_newer,
                        gfx::AcceleratedWidget elevation_window);

  // Helper function for starting an update check. Returns true if the check was
  // properly started, in which case CompleteUpdateCheck will be invoked upon
  // completion to return results to the caller on its own thread.
  bool BeginUpdateCheckInternal(bool install_if_newer,
                                gfx::AcceleratedWidget elevation_window);

  // Invoked when results are in from Google Update.
  void CompleteUpdateCheck();

  // Prepares |results| to return the upgrade error indicated by |error_code|
  // and |hr|. The string " -- system level" is included in the generated error
  // message when |system_level| is true.
  void OnUpgradeError(GoogleUpdateErrorCode error_code,
                      HRESULT hr,
                      bool system_level);

  // The caller's task runner, on which |result_callback_| will be run.
  scoped_refptr<base::SingleThreadTaskRunner> result_runner_;

  // The caller's callback to be run when the update check is compelte.
  UpdateCheckCallback result_callback_;

  // The results of the update check.
  GoogleUpdateUpgradeResult result_;
  GoogleUpdateErrorCode error_code_;
  base::string16 error_message_;
  base::string16 version_;
  HRESULT hresult_;

  // A direct pointer to the job observer by which the driver is notified of
  // interesting events from Google Update.
  CComObject<GoogleUpdateJobObserver>* job_observer_;

  // A scoped pointer to |job_observer_| that holds a reference to it, keeping
  // it alive.
  base::win::ScopedComPtr<IJobObserver> job_holder_;

  // The on-demand updater that is doing the work.
  base::win::ScopedComPtr<IGoogleUpdate> on_demand_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckDriver);
};

// static
void UpdateCheckDriver::RunUpdateCheck(
    const scoped_refptr<base::TaskRunner>& task_runner,
    bool install_if_newer,
    gfx::AcceleratedWidget elevation_window,
    const UpdateCheckCallback& callback) {
  // The driver is owned by itself, and will self-destruct when its work is
  // done.
  UpdateCheckDriver* driver = new UpdateCheckDriver(callback);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&UpdateCheckDriver::BeginUpdateCheck, base::Unretained(driver),
                 install_if_newer, elevation_window));
}

// Runs on the caller's thread.
UpdateCheckDriver::UpdateCheckDriver(const UpdateCheckCallback& callback)
    : result_runner_(base::ThreadTaskRunnerHandle::Get()),
      result_callback_(callback),
      result_(UPGRADE_ERROR),
      error_code_(GOOGLE_UPDATE_NO_ERROR),
      hresult_(S_OK),
      job_observer_(nullptr) {
}

UpdateCheckDriver::~UpdateCheckDriver() {
  DCHECK(result_runner_->BelongsToCurrentThread());
  // If there is an error, then error_code must not be blank, and vice versa.
  DCHECK_NE(result_ == UPGRADE_ERROR, error_code_ == GOOGLE_UPDATE_NO_ERROR);
  UMA_HISTOGRAM_ENUMERATION("GoogleUpdate.UpgradeResult", result_,
                            NUM_UPGRADE_RESULTS);
  if (result_ == UPGRADE_ERROR) {
    UMA_HISTOGRAM_ENUMERATION("GoogleUpdate.UpdateErrorCode", error_code_,
                              NUM_ERROR_CODES);
    if (hresult_ != S_OK)
      UMA_HISTOGRAM_SPARSE_SLOWLY("GoogleUpdate.ErrorHresult", hresult_);
  }
  result_callback_.Run(result_, error_code_, error_message_, version_);
}

void UpdateCheckDriver::BeginUpdateCheck(
    bool install_if_newer,
    gfx::AcceleratedWidget elevation_window) {
  // Return results immediately if the driver is not waiting for Google Update.
  if (!BeginUpdateCheckInternal(install_if_newer, elevation_window))
    result_runner_->DeleteSoon(FROM_HERE, this);
}

bool UpdateCheckDriver::BeginUpdateCheckInternal(
    bool install_if_newer,
    gfx::AcceleratedWidget elevation_window) {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::DIR_EXE, &chrome_exe))
    NOTREACHED();

  const bool system_level = !InstallUtil::IsPerUserInstall(chrome_exe);

  // The failures handled here are:
  // CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY,
  // GOOGLE_UPDATE_DISABLED_BY_POLICY, and
  // GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY.
  error_code_ = CanUpdateCurrentChrome(chrome_exe, system_level);
  if (error_code_ != GOOGLE_UPDATE_NO_ERROR) {
    // These failures are handled in the UX with custom messages.
    result_ = UPGRADE_ERROR;
    return false;
  }

  // Make sure ATL is initialized in this module.
  ui::win::CreateATLModuleIfNeeded();

  HRESULT hr =
      CComObject<GoogleUpdateJobObserver>::CreateInstance(&job_observer_);
  if (hr != S_OK) {
    // Most of the error messages come straight from Google Update. This one is
    // deemed worthy enough to also warrant its own error.
    OnUpgradeError(GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED, hr, false);
    return false;
  }
  // Hold a reference on the observer for the lifetime of the driver.
  job_holder_ = job_observer_;

  job_observer_->set_on_complete_callback(base::Bind(
      &UpdateCheckDriver::CompleteUpdateCheck, base::Unretained(this)));

  hr = CreateOnDemandAppsClass(system_level, install_if_newer, elevation_window,
                               &on_demand_);
  if (hr != S_OK) {
    OnUpgradeError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, hr, system_level);
    return false;
  }

  base::string16 app_guid = installer::GetAppGuidForUpdates(system_level);
  DCHECK(!app_guid.empty());

  if (install_if_newer)
    hr = on_demand_->Update(app_guid.c_str(), job_observer_);
  else
    hr = on_demand_->CheckForUpdate(app_guid.c_str(), job_observer_);

  if (hr != S_OK) {
    OnUpgradeError(GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR, hr,
                   system_level);
    return false;
  }
  return true;
}

void UpdateCheckDriver::CompleteUpdateCheck() {
  result_ = job_observer_->result();
  if (result_ == UPGRADE_ERROR) {
    error_code_ = GOOGLE_UPDATE_ERROR_UPDATING;
    error_message_ = job_observer_->error_message();
  } else {
    version_ = job_observer_->new_version();
  }

  // Release the reference on the COM objects before bouncing back to the
  // caller's thread.
  on_demand_.Release();
  job_holder_.Release();
  job_observer_ = nullptr;

  result_runner_->DeleteSoon(FROM_HERE, this);
}

void UpdateCheckDriver::OnUpgradeError(GoogleUpdateErrorCode error_code,
                                       HRESULT hr,
                                       bool system_level) {
  result_ = UPGRADE_ERROR;
  error_code_ = error_code;
  hresult_ = hr;
  base::string16 error_msg =
      base::StringPrintf(L"%d: 0x%x", error_code_, hresult_);
  if (system_level)
    error_msg += L" -- system level";
  error_message_ = l10n_util::GetStringFUTF16(
      IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED, error_msg);
}

}  // namespace


// Globals ---------------------------------------------------------------------

void BeginUpdateCheck(const scoped_refptr<base::TaskRunner>& task_runner,
                      bool install_if_newer,
                      gfx::AcceleratedWidget elevation_window,
                      const UpdateCheckCallback& callback) {
  UpdateCheckDriver::RunUpdateCheck(task_runner, install_if_newer,
                                    elevation_window, callback);
}


// Private API exposed for testing. --------------------------------------------

void SetGoogleUpdateFactoryForTesting(
    const OnDemandAppsClassFactory& google_update_factory) {
  if (g_google_update_factory) {
    delete g_google_update_factory;
    g_google_update_factory = nullptr;
  }
  if (!google_update_factory.is_null()) {
    g_google_update_factory =
        new OnDemandAppsClassFactory(google_update_factory);
  }
}
