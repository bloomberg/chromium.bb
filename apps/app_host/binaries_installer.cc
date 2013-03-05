// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_host/binaries_installer.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "google_update/google_update_idl.h"


namespace app_host {

// Helpers --------------------------------------------------------------------

namespace {

const wchar_t kAppHostAppId[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";
const wchar_t kBinariesAppId[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
const int kInstallationPollingIntervalMs = 50;

HRESULT CreateInstalledApp(IAppBundle* app_bundle,
                           const wchar_t* app_guid,
                           IApp** app) {
  base::win::ScopedComPtr<IDispatch> idispatch;
  HRESULT hr = app_bundle->createInstalledApp(base::win::ScopedBstr(app_guid),
                                              idispatch.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to configure App Bundle: " << hr;
    return hr;
  }

  base::win::ScopedComPtr<IApp> temp_app;
  hr = temp_app.QueryFrom(idispatch);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unexpected error querying IApp from "
               << "IAppBundle->createInstalledApp return value: " << hr;
  } else {
    *app = temp_app.Detach();
  }
  return hr;
}

HRESULT GetAppHostApValue(IGoogleUpdate3* update3,
                          IAppBundle* app_bundle,
                          BSTR* ap_value) {
  base::win::ScopedComPtr<IApp> app;
  HRESULT hr = CreateInstalledApp(app_bundle, kAppHostAppId, app.Receive());
  if (FAILED(hr))
    return hr;

  hr = app->get_ap(ap_value);
  if (FAILED(hr))
    LOG(ERROR) << "Failed to get the App Launcher AP value.";
  return hr;
}

HRESULT GetCurrentState(IApp* app,
                        ICurrentState** current_state,
                        CurrentState* state_value) {
  base::win::ScopedComPtr<IDispatch> idispatch;
  HRESULT hr = app->get_currentState(idispatch.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get App Bundle state: " << hr;
    return hr;
  }

  base::win::ScopedComPtr<ICurrentState> temp_current_state;
  hr = temp_current_state.QueryFrom(idispatch);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unexpected error querying ICurrentState from "
               << "IApp::get_currentState return value: " << hr;
    return hr;
  }

  LONG long_state_value;
  hr = temp_current_state->get_stateValue(&long_state_value);
  if (SUCCEEDED(hr)) {
    *state_value = static_cast<CurrentState>(long_state_value);
    *current_state = temp_current_state.Detach();
  } else {
    LOG(ERROR) << "Failed to get App Bundle state value: " << hr;
  }
  return hr;
}

bool CheckIsBusy(IAppBundle* app_bundle, HRESULT* hr) {
  VARIANT_BOOL variant_is_busy = VARIANT_TRUE;
  *hr = app_bundle->isBusy(&variant_is_busy);
  if (FAILED(*hr))
    LOG(ERROR) << "Failed to check app_bundle->isBusy: " << *hr;
  return (variant_is_busy == VARIANT_TRUE);
}

void OnUpdateAvailable(IAppBundle* app_bundle, HRESULT* hr) {
  // If the app bundle is busy we will just wait some more.
  if (CheckIsBusy(app_bundle, hr) || FAILED(*hr))
    return;
  *hr = app_bundle->download();
  if (FAILED(*hr))
    LOG(ERROR) << "Failed to initiate bundle download: " << *hr;
}

void OnReadyToInstall(IAppBundle* app_bundle, HRESULT* hr) {
  // If the app bundle is busy we will just wait some more.
  if (CheckIsBusy(app_bundle, hr) || FAILED(*hr))
    return;
  *hr = app_bundle->install();
  if (FAILED(*hr))
    LOG(ERROR) << "Failed to initiate bundle install: " << *hr;
}

HRESULT OnError(ICurrentState* current_state) {
  LONG error_code;
  HRESULT hr = current_state->get_errorCode(&error_code);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve bundle error code: " << hr;
    return hr;
  }

  base::win::ScopedBstr completion_message;
  HRESULT completion_message_hr =
    current_state->get_completionMessage(completion_message.Receive());
  if (FAILED(completion_message_hr)) {
    LOG(ERROR) << "Bundle installation failed with error " << error_code
               << ". Error message retrieval failed with error: "
               << completion_message_hr;
  } else {
    LOG(ERROR) << "Bundle installation failed with error " << error_code << ": "
               << completion_message;
  }
  return error_code;
}

HRESULT CreateGoogleUpdate3(IGoogleUpdate3** update3) {
  base::win::ScopedComPtr<IGoogleUpdate3> temp_update3;
  HRESULT hr = temp_update3.CreateInstance(CLSID_GoogleUpdate3UserClass);
  if (SUCCEEDED(hr)) {
    *update3 = temp_update3.Detach();
  } else {
    // TODO(erikwright): Try in-proc to support running elevated? According
    // to update3_utils.cc (CreateGoogleUpdate3UserClass):
    // The primary reason for the LocalServer activation failing on Vista/Win7
    // is that COM does not look at HKCU registration when the code is running
    // elevated. We fall back to an in-proc mode. The in-proc mode is limited to
    // one install at a time, so we use it only as a backup mechanism.
    LOG(ERROR) << "Failed to instantiate GoogleUpdate3: " << hr;
  }
  return hr;
}

HRESULT CreateAppBundle(IGoogleUpdate3* update3, IAppBundle** app_bundle) {
  base::win::ScopedComPtr<IDispatch> idispatch;
  HRESULT hr = update3->createAppBundle(idispatch.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to createAppBundle: " << hr;
    return hr;
  }

  base::win::ScopedComPtr<IAppBundle> temp_app_bundle;
  hr = temp_app_bundle.QueryFrom(idispatch);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unexpected error querying IAppBundle from "
               << "IGoogleUpdate3->createAppBundle return value: " << hr;
    return hr;
  }

  hr = temp_app_bundle->initialize();
  if (FAILED(hr))
    LOG(ERROR) << "Failed to initialize App Bundle: " << hr;
  else
    *app_bundle = temp_app_bundle.Detach();
  return hr;
}

HRESULT SelectBinariesApValue(IGoogleUpdate3* update3,
                              BSTR* ap_value) {
  // TODO(erikwright): Uncomment this when we correctly propagate the AP value
  // from the system-level binaries when quick-enabling the app host at
  // user-level (http://crbug.com/178479).
  // base::win::ScopedComPtr<IAppBundle> app_bundle;
  // HRESULT hr = CreateAppBundle(update3, app_bundle.Receive());
  // if (FAILED(hr))
  //   return hr;

  // hr = GetAppHostApValue(update3, app_bundle, ap_value);
  // if (SUCCEEDED(hr))
  //   return hr;

  // TODO(erikwright): distinguish between AppHost not installed and an
  // error in GetAppHostApValue.
  // TODO(erikwright): Use stable by default when App Host support is in
  // stable.
  base::win::ScopedBstr temp_ap_value;
  if (temp_ap_value.Allocate(L"2.0-dev-multi-apphost") == NULL) {
    LOG(ERROR) << "Unexpected error in ScopedBstr::Allocate.";
    return E_FAIL;
  }
  *ap_value = temp_ap_value.Release();
  return S_OK;
}

HRESULT CreateBinariesIApp(IAppBundle* app_bundle, BSTR ap, IApp** app) {
  base::win::ScopedComPtr<IDispatch> idispatch;
  HRESULT hr = app_bundle->createApp(base::win::ScopedBstr(kBinariesAppId),
                                     idispatch.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to configure App Bundle: " << hr;
    return hr;
  }

  base::win::ScopedComPtr<IApp> temp_app;
  hr = temp_app.QueryFrom(idispatch);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unexpected error querying IApp from "
               << "IAppBundle->createApp return value: " << hr;
    return hr;
  }

  hr = temp_app->put_isEulaAccepted(VARIANT_TRUE);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set 'EULA Accepted': " << hr;
    return hr;
  }

  hr = temp_app->put_ap(ap);
  if (FAILED(hr))
    LOG(ERROR) << "Failed to set AP value: " << hr;
  else
    *app = temp_app.Detach();
  return hr;
}

bool CheckIfDone(IAppBundle* app_bundle, IApp* app, HRESULT* hr) {
  base::win::ScopedComPtr<ICurrentState> current_state;
  CurrentState state_value;
  *hr = GetCurrentState(app, current_state.Receive(), &state_value);
  if (FAILED(*hr))
    return true;

  switch (state_value) {
    case STATE_WAITING_TO_CHECK_FOR_UPDATE:
    case STATE_CHECKING_FOR_UPDATE:
    case STATE_WAITING_TO_DOWNLOAD:
    case STATE_RETRYING_DOWNLOAD:
    case STATE_DOWNLOADING:
    case STATE_WAITING_TO_INSTALL:
    case STATE_INSTALLING:
    case STATE_DOWNLOAD_COMPLETE:
    case STATE_EXTRACTING:
    case STATE_APPLYING_DIFFERENTIAL_PATCH:
      // These states will all transition on their own.
      return false;

    case STATE_UPDATE_AVAILABLE:
      OnUpdateAvailable(app_bundle, hr);
      return FAILED(*hr);

    case STATE_READY_TO_INSTALL:
      OnReadyToInstall(app_bundle, hr);
      return FAILED(*hr);

    case STATE_NO_UPDATE:
      LOG(INFO) << "Google Update reports that the binaries are already "
                << "installed and up-to-date.";
      return true;

    case STATE_INSTALL_COMPLETE:
      return true;

    case STATE_ERROR:
      *hr = OnError(current_state);
      return FAILED(*hr);

    case STATE_INIT:
    case STATE_PAUSED:
    default:
      LOG(ERROR) << "Unexpected bundle state: " << state_value << ".";
      *hr = E_FAIL;
      return true;
  }
}

}  // namespace


// Globals --------------------------------------------------------------------

HRESULT InstallBinaries() {
  base::win::ScopedCOMInitializer initialize_com;
  if (!initialize_com.succeeded()) {
    LOG(ERROR) << "COM initialization failed";
    return E_FAIL;
  }

  base::win::ScopedComPtr<IGoogleUpdate3> update3;
  HRESULT hr = CreateGoogleUpdate3(update3.Receive());
  if (FAILED(hr))
    return hr;

  base::win::ScopedBstr ap_value;
  hr = SelectBinariesApValue(update3, ap_value.Receive());
  if (FAILED(hr))
    return hr;

  base::win::ScopedComPtr<IAppBundle> app_bundle;
  hr = CreateAppBundle(update3, app_bundle.Receive());
  if (FAILED(hr))
    return hr;

  base::win::ScopedComPtr<IApp> app;
  hr = CreateBinariesIApp(app_bundle, ap_value, app.Receive());
  if (FAILED(hr))
    return hr;

  hr = app_bundle->checkForUpdate();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to initiate update check: " << hr;
    return hr;
  }

  // We rely upon Omaha to eventually time out and transition to a failure
  // state.
  while (!CheckIfDone(app_bundle, app, &hr)) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
        kInstallationPollingIntervalMs));
  }
  return hr;
}

}  // namespace app_host
