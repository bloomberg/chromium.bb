// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_host/binaries_installer_internal.h"

#include "base/logging.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "google_update/google_update_idl.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

namespace app_host {
namespace internal {

namespace {

const wchar_t kBinariesAppId[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
const wchar_t kAppHostAppId[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

HRESULT CheckIsBusy(IAppBundle* app_bundle, bool* is_busy) {
  VARIANT_BOOL variant_is_busy = VARIANT_TRUE;
  HRESULT hr = app_bundle->isBusy(&variant_is_busy);
  if (FAILED(hr))
    LOG(ERROR) << "Failed to check app_bundle->isBusy: " << hr;
  else
    *is_busy = (variant_is_busy == VARIANT_TRUE);
  return hr;
}

HRESULT OnUpdateAvailable(IAppBundle* app_bundle) {
  // If the app bundle is busy we will just wait some more.
  bool is_busy = false;
  HRESULT hr = CheckIsBusy(app_bundle, &is_busy);
  if (SUCCEEDED(hr) && !is_busy) {
    hr = app_bundle->download();
    if (FAILED(hr))
      LOG(ERROR) << "Failed to initiate bundle download: " << hr;
  }
  return hr;
}

HRESULT OnReadyToInstall(IAppBundle* app_bundle) {
  // If the app bundle is busy we will just wait some more.
  bool is_busy = false;
  HRESULT hr = CheckIsBusy(app_bundle, &is_busy);
  if (SUCCEEDED(hr) && !is_busy) {
    hr = app_bundle->install();
    if (FAILED(hr))
      LOG(ERROR) << "Failed to initiate bundle install: " << hr;
  }
  return hr;
}

HRESULT OnError(ICurrentState* current_state) {
  LONG error_code;
  HRESULT hr = current_state->get_errorCode(&error_code);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve bundle error code: " << hr;
  } else {
    hr = error_code;

    ScopedBstr completion_message;
    HRESULT completion_message_hr =
      current_state->get_completionMessage(completion_message.Receive());
    if (FAILED(completion_message_hr)) {
      LOG(ERROR) << "Bundle installation failed with error " << hr
                 << ". Error message retrieval failed with error: "
                 << completion_message_hr;
    } else {
      LOG(ERROR) << "Bundle installation failed with error " << hr << ": "
                 << completion_message;
    }
  }

  return hr;
}

HRESULT GetCurrentState(IApp* app,
                        ICurrentState** current_state,
                        CurrentState* state_value) {
  HRESULT hr = S_OK;

  ScopedComPtr<ICurrentState> temp_current_state;
  {
    ScopedComPtr<IDispatch> idispatch;
    hr = app->get_currentState(idispatch.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get App Bundle state: " << hr;
    } else {
      hr = temp_current_state.QueryFrom(idispatch);
      if (FAILED(hr)) {
        LOG(ERROR) << "Unexpected error querying ICurrentState from "
                   << "IApp::get_currentState return value: " << hr;
      }
    }
  }

  if (SUCCEEDED(hr)) {
    LONG long_state_value;
    hr = temp_current_state->get_stateValue(&long_state_value);
    if (FAILED(hr))
      LOG(ERROR) << "Failed to get App Bundle state value: " << hr;
    *state_value = static_cast<CurrentState>(long_state_value);
    *current_state = temp_current_state.Detach();
  }

  return hr;
}

HRESULT CreateInstalledApp(IAppBundle* app_bundle,
                           const wchar_t* app_guid,
                           IApp** app) {
  ScopedComPtr<IApp> temp_app;
  ScopedComPtr<IDispatch> idispatch;
  HRESULT hr = app_bundle->createInstalledApp(ScopedBstr(app_guid),
                                              idispatch.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to configure App Bundle: " << hr;
  } else {
    hr = temp_app.QueryFrom(idispatch);
    if (FAILED(hr)) {
      LOG(ERROR) << "Unexpected error querying IApp from "
                 << "IAppBundle->createInstalledApp return value: " << hr;
    } else {
      *app = temp_app.Detach();
    }
  }

  return hr;
}

}  // namespace

bool CheckIfDone(IAppBundle* app_bundle, IApp* app, HRESULT* hr) {
  ScopedComPtr<ICurrentState> current_state;
  CurrentState state_value;
  *hr = GetCurrentState(app, current_state.Receive(), &state_value);

  bool complete = false;

  if (SUCCEEDED(hr)) {
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
        break;

      case STATE_UPDATE_AVAILABLE:
        *hr = OnUpdateAvailable(app_bundle);
        break;

      case STATE_READY_TO_INSTALL:
        *hr = OnReadyToInstall(app_bundle);
        break;

      case STATE_NO_UPDATE:
        LOG(INFO) << "Google Update reports that the binaries are already "
                  << "installed and up-to-date.";
        complete = true;
        break;

      case STATE_INSTALL_COMPLETE:
        complete = true;
        break;

      case STATE_ERROR:
        *hr = OnError(current_state);
        break;

      case STATE_INIT:
      case STATE_PAUSED:
      default:
        LOG(ERROR) << "Unexpected bundle state: " << state_value << ".";
        *hr = E_FAIL;
        break;
    }
  }

  return FAILED(*hr) || complete;
}

HRESULT CreateAppBundle(IGoogleUpdate3* update3, IAppBundle** app_bundle) {
  HRESULT hr = S_OK;

  ScopedComPtr<IAppBundle> temp_app_bundle;
  {
    ScopedComPtr<IDispatch> idispatch;
    hr = update3->createAppBundle(idispatch.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to createAppBundle: " << hr;
    } else {
      hr = temp_app_bundle.QueryFrom(idispatch);
      if (FAILED(hr)) {
        LOG(ERROR) << "Unexpected error querying IAppBundle from "
                   << "IGoogleUpdate3->createAppBundle return value: " << hr;
      }
    }
  }

  if (SUCCEEDED(hr)) {
    hr = temp_app_bundle->initialize();
    if (FAILED(hr))
      LOG(ERROR) << "Failed to initialize App Bundle: " << hr;
    else
      *app_bundle = temp_app_bundle.Detach();
  }

  return hr;
}

HRESULT GetAppHostApValue(IGoogleUpdate3* update3, BSTR* ap_value) {
  ScopedComPtr<IAppBundle> app_bundle;
  HRESULT hr = CreateAppBundle(update3, app_bundle.Receive());
  if (SUCCEEDED(hr)) {
    ScopedComPtr<IApp> app;
    hr = CreateInstalledApp(app_bundle, kAppHostAppId, app.Receive());
    if (SUCCEEDED(hr)) {
      hr = app->get_ap(ap_value);
      if (FAILED(hr))
        LOG(ERROR) << "Failed to get the App Host AP value.";
    }
  }

  return hr;
}

HRESULT CreateBinariesIApp(IAppBundle* app_bundle, BSTR ap, IApp** app) {
  HRESULT hr = S_OK;

  ScopedComPtr<IApp> temp_app;
  {
    ScopedComPtr<IDispatch> idispatch;
    hr = app_bundle->createApp(ScopedBstr(kBinariesAppId), idispatch.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to configure App Bundle: " << hr;
    } else {
      hr = temp_app.QueryFrom(idispatch);
      if (FAILED(hr)) {
        LOG(ERROR) << "Unexpected error querying IApp from "
                   << "IAppBundle->createApp return value: " << hr;
      }
    }
  }

  if (SUCCEEDED(hr)) {
    hr = temp_app->put_isEulaAccepted(VARIANT_TRUE);
    if (FAILED(hr))
      LOG(ERROR) << "Failed to set 'EULA Accepted': " << hr;
  }

  if (SUCCEEDED(hr)) {
    hr = temp_app->put_ap(ap);
    if (FAILED(hr))
      LOG(ERROR) << "Failed to set AP value: " << hr;
  }

  if (SUCCEEDED(hr))
    *app = temp_app.Detach();

  return hr;
}

HRESULT CreateGoogleUpdate3(IGoogleUpdate3** update3) {
  ScopedComPtr<IGoogleUpdate3> temp_update3;
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

HRESULT SelectBinariesApValue(IGoogleUpdate3* update3, BSTR* ap_value) {
  HRESULT hr = GetAppHostApValue(update3, ap_value);
  if (FAILED(hr)) {
    // TODO(erikwright): distinguish between AppHost not installed and an
    // error in GetAppHostApValue.
    // TODO(erikwright): Use stable by default when App Host support is in
    // stable.
    ScopedBstr temp_ap_value;
    if (NULL == temp_ap_value.Allocate(L"2.0-dev-multi-apphost")) {
      LOG(ERROR) << "Unexpected error in ScopedBstr::Allocate.";
      hr = E_FAIL;
    } else {
      *ap_value = temp_ap_value.Release();
      hr = S_OK;
    }
  }

  return hr;
}

}  // namespace internal
}  // namespace app_host
