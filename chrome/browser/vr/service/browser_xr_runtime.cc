// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/browser_xr_runtime.h"

#include "chrome/browser/vr/service/vr_display_host.h"
#include "device/vr/vr_device.h"

namespace vr {

BrowserXRRuntime::BrowserXRRuntime(device::mojom::XRRuntimePtr runtime,
                                   device::mojom::VRDisplayInfoPtr display_info)
    : runtime_(std::move(runtime)),
      display_info_(std::move(display_info)),
      binding_(this),
      weak_ptr_factory_(this) {
  device::mojom::XRRuntimeEventListenerPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));

  // Unretained is safe because we are calling through an InterfacePtr we own,
  // so we won't be called after runtime_ is destroyed.
  runtime_->ListenToDeviceChanges(
      std::move(listener),
      base::BindOnce(&BrowserXRRuntime::OnInitialDevicePropertiesReceived,
                     base::Unretained(this)));
}

void BrowserXRRuntime::OnInitialDevicePropertiesReceived(
    device::mojom::VRDisplayInfoPtr display_info) {
  OnDisplayInfoChanged(std::move(display_info));
}

BrowserXRRuntime::~BrowserXRRuntime() = default;

void BrowserXRRuntime::OnDisplayInfoChanged(
    device::mojom::VRDisplayInfoPtr vr_device_info) {
  display_info_ = std::move(vr_device_info);
  for (VRDisplayHost* display : displays_) {
    display->OnChanged();
  }
}

void BrowserXRRuntime::StopImmersiveSession() {
  if (immersive_session_controller_) {
    immersive_session_controller_ = nullptr;
    presenting_display_host_ = nullptr;
  }
}

void BrowserXRRuntime::OnExitPresent() {
  if (presenting_display_host_) {
    presenting_display_host_->OnExitPresent();
    presenting_display_host_ = nullptr;
  }
}

void BrowserXRRuntime::OnDeviceActivated(
    device::mojom::VRDisplayEventReason reason,
    base::OnceCallback<void(bool)> on_handled) {
  if (listening_for_activation_display_host_) {
    listening_for_activation_display_host_->OnActivate(reason,
                                                       std::move(on_handled));
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void BrowserXRRuntime::OnDeviceIdle(
    device::mojom::VRDisplayEventReason reason) {
  for (VRDisplayHost* display : displays_) {
    display->OnDeactivate(reason);
  }
}

void BrowserXRRuntime::OnDisplayHostAdded(VRDisplayHost* display) {
  displays_.insert(display);
}

void BrowserXRRuntime::OnDisplayHostRemoved(VRDisplayHost* display) {
  DCHECK(display);
  displays_.erase(display);
  if (display == presenting_display_host_) {
    ExitPresent(display);
    DCHECK(presenting_display_host_ == nullptr);
  }
  if (display == listening_for_activation_display_host_) {
    // Not listening for activation.
    listening_for_activation_display_host_ = nullptr;
    runtime_->SetListeningForActivate(false);
  }
}

void BrowserXRRuntime::ExitPresent(VRDisplayHost* display) {
  if (display == presenting_display_host_) {
    StopImmersiveSession();
  }
}

void BrowserXRRuntime::RequestSession(
    VRDisplayHost* display,
    const device::mojom::XRRuntimeSessionOptionsPtr& options,
    device::mojom::VRDisplayHost::RequestSessionCallback callback) {
  // base::Unretained is safe because we won't be called back after runtime_ is
  // destroyed.
  runtime_->RequestSession(
      options->Clone(),
      base::BindOnce(&BrowserXRRuntime::OnRequestSessionResult,
                     base::Unretained(this), display->GetWeakPtr(),
                     options->Clone(), std::move(callback)));
}

void BrowserXRRuntime::OnRequestSessionResult(
    base::WeakPtr<VRDisplayHost> display,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    device::mojom::VRDisplayHost::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    device::mojom::XRSessionControllerPtr immersive_session_controller) {
  if (session && display) {
    if (options->immersive) {
      presenting_display_host_ = display.get();
      immersive_session_controller_ = std::move(immersive_session_controller);
    }

    std::move(callback).Run(std::move(session));
  } else {
    std::move(callback).Run(nullptr);
    if (session) {
      // The device has been removed, but we still got a session, so make
      // sure to clean up this weird state.
      immersive_session_controller_ = std::move(immersive_session_controller);
      StopImmersiveSession();
    }
  }
}

void BrowserXRRuntime::UpdateListeningForActivate(VRDisplayHost* display) {
  if (display->ListeningForActivate() && display->InFocusedFrame()) {
    bool was_listening = !!listening_for_activation_display_host_;
    listening_for_activation_display_host_ = display;
    if (!was_listening)
      OnListeningForActivate(true);
  } else if (listening_for_activation_display_host_ == display) {
    listening_for_activation_display_host_ = nullptr;
    OnListeningForActivate(false);
  }
}

void BrowserXRRuntime::OnListeningForActivate(bool is_listening) {
  runtime_->SetListeningForActivate(is_listening);
}

}  // namespace vr
