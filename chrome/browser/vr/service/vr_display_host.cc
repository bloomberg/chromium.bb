// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_display_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/mode.h"
#include "chrome/browser/vr/service/browser_xr_device.h"
#include "chrome/browser/vr/service/vr_device_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "device/vr/vr_display_impl.h"

namespace vr {

namespace {

// TODO(mthiesse): When we unship WebVR 1.1, set this to false.
static constexpr bool kAllowHTTPWebVRWithFlag = true;

bool IsSecureContext(content::RenderFrameHost* host) {
  if (!host)
    return false;
  while (host != nullptr) {
    if (!content::IsOriginSecure(host->GetLastCommittedURL()))
      return false;
    host = host->GetParent();
  }
  return true;
}

device::mojom::XRDeviceRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRDeviceRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRDeviceRuntimeSessionOptions::New();
  runtime_options->immersive = options->immersive;
  runtime_options->has_user_activation = options->has_user_activation;
  runtime_options->provide_passthrough_camera =
      options->provide_passthrough_camera;
  runtime_options->use_legacy_webvr_render_path =
      options->use_legacy_webvr_render_path;
  return runtime_options;
}

}  // namespace

device::mojom::VRDisplayInfoPtr VRDisplayHost::GetCurrentVRDisplayInfo() {
  // Get an immersive_device_ device if there is one.
  if (!immersive_device_) {
    immersive_device_ = VRDeviceManager::GetInstance()->GetImmersiveDevice();
    if (immersive_device_) {
      // Listen to changes for this device.
      immersive_device_->OnDisplayHostAdded(this);
    }
  }

  // Get an AR device if there is one.
  if (!ar_device_) {
    device::mojom::XRSessionOptions options = {};
    options.provide_passthrough_camera = true;
    ar_device_ = VRDeviceManager::GetInstance()->GetDeviceForOptions(&options);
    if (ar_device_) {
      // Listen to  changes for this device.
      ar_device_->OnDisplayHostAdded(this);
    }
  }

  // If there is neither, use the magic window device.
  if (!ar_device_ && !immersive_device_) {
    if (!magic_window_device_) {
      device::mojom::XRSessionOptions options = {};
      magic_window_device_ =
          VRDeviceManager::GetInstance()->GetDeviceForOptions(&options);
      if (magic_window_device_) {
        // Listen to changes for this device.
        magic_window_device_->OnDisplayHostAdded(this);
      }
    }

    // If we don't have an AR or immersive device, return the magic-window's
    // DisplayInfo if we have it.
    return magic_window_device_ ? magic_window_device_->GetVRDisplayInfo()
                                : nullptr;
  }

  // Use the immersive or AR device.  However, if we are using the immersive
  // device's info, and AR is supported, reflect that in capabilities.
  device::mojom::VRDisplayInfoPtr device_info =
      immersive_device_ ? immersive_device_->GetVRDisplayInfo()
                        : ar_device_->GetVRDisplayInfo();
  device_info->capabilities->can_provide_pass_through_images = !!ar_device_;

  return device_info;
}

VRDisplayHost::VRDisplayHost(content::RenderFrameHost* render_frame_host,
                             device::mojom::VRServiceClient* service_client)
    :  // TODO(https://crbug.com/846392): render_frame_host can be null because
       // of a test, not because a VRDisplayHost can be created without it.
      in_focused_frame_(
          render_frame_host ? render_frame_host->GetView()->HasFocus() : false),
      render_frame_host_(render_frame_host),
      binding_(this),
      weak_ptr_factory_(this) {
  device::mojom::VRDisplayInfoPtr display_info = GetCurrentVRDisplayInfo();
  if (!display_info) {
    return;
  }

  // Tell blink that we are available.
  device::mojom::VRDisplayHostPtr display_host;
  binding_.Bind(mojo::MakeRequest(&display_host));
  service_client->OnDisplayConnected(std::move(display_host),
                                     mojo::MakeRequest(&client_),
                                     std::move(display_info));
}

void VRDisplayHost::OnARSessionCreated(
    vr::BrowserXrDevice* device,
    device::mojom::VRDisplayHost::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session) {
  if (!session) {
    std::move(callback).Run(nullptr);
    return;
  }

  device->GetRuntime()->RequestMagicWindowSession(
      base::BindOnce(&VRDisplayHost::OnMagicWindowSessionCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void VRDisplayHost::OnMagicWindowSessionCreated(
    device::mojom::VRDisplayHost::RequestSessionCallback callback,
    device::mojom::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::XRSessionControllerPtr controller) {
  if (!magic_window_provider) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Start giving out magic window data if we are focused.
  controller->SetFrameDataRestricted(!in_focused_frame_);

  magic_window_controllers_.AddPtr(std::move(controller));

  device::mojom::XRSessionPtr xr_session = device::mojom::XRSession::New();
  xr_session->magic_window_provider = magic_window_provider.PassInterface();

  std::move(callback).Run(std::move(xr_session));
}

VRDisplayHost::~VRDisplayHost() {
  if (immersive_device_)
    immersive_device_->OnDisplayHostRemoved(this);
  if (magic_window_device_)
    magic_window_device_->OnDisplayHostRemoved(this);
  if (ar_device_)
    ar_device_->OnDisplayHostRemoved(this);
}

void VRDisplayHost::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    bool triggered_by_displayactive,
    device::mojom::VRDisplayHost::RequestSessionCallback callback) {
  DCHECK(options);

  // Check that the request satisifies secure context requirements.
  if (!IsSecureContextRequirementSatisfied()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Check that the request is coming from a focused page if required.
  if (!in_focused_frame_ && options->immersive) {
    std::move(callback).Run(nullptr);
    return;
  }

  BrowserXrDevice* presenting_device =
      VRDeviceManager::GetInstance()->GetImmersiveDevice();
  if (presenting_device &&
      presenting_device->GetPresentingDisplayHost() != this &&
      presenting_device->GetPresentingDisplayHost() != nullptr) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(nullptr);
    return;
  }

  // Get the runtime we'll be creating a session with.
  BrowserXrDevice* device =
      VRDeviceManager::GetInstance()->GetDeviceForOptions(options.get());
  if (!device) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto runtime_options = GetRuntimeOptions(options.get());
  runtime_options->render_process_id =
      render_frame_host_ ? render_frame_host_->GetProcess()->GetID() : -1;
  runtime_options->render_frame_id =
      render_frame_host_ ? render_frame_host_->GetRoutingID() : -1;

  if (runtime_options->immersive) {
    if (!triggered_by_displayactive) {
      ReportRequestPresent();
    }

    // Ensure we will notify the immersive device when we are destroyed, so
    // lingering presentation state will be cleaned up.
    DCHECK(device == immersive_device_);

    device->RequestSession(this, std::move(runtime_options),
                           std::move(callback));
  } else if (runtime_options->provide_passthrough_camera) {
    // WebXrHitTest enabled means we are requesting an AR session.  This means
    // we make two requests to the device - one to request permissions and start
    // the runtime, then a followup to actually get the magic window provider.
    // TODO(offenwanger): Clean this up and make only one request.
    // base::Unretained(device) is safe because either device is alive when we
    // are called back, or we are called back with failure.
    device->RequestSession(
        this, std::move(runtime_options),
        base::BindOnce(&VRDisplayHost::OnARSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), base::Unretained(device),
                       std::move(callback)));
  } else {
    device->GetRuntime()->RequestMagicWindowSession(
        base::BindOnce(&VRDisplayHost::OnMagicWindowSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void VRDisplayHost::SupportsSession(device::mojom::XRSessionOptionsPtr options,
                                    SupportsSessionCallback callback) {
  std::move(callback).Run(InternalSupportsSession(options.get()));
}

bool VRDisplayHost::InternalSupportsSession(
    device::mojom::XRSessionOptions* options) {
  // Return whether we can get a device that supports the specified options.
  return VRDeviceManager::GetInstance()->GetDeviceForOptions(options) !=
         nullptr;
}

void VRDisplayHost::ReportRequestPresent() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents);
  if (!metrics_helper) {
    // This will only happen if we are not already in VR, set start params
    // accordingly.
    metrics_helper = SessionMetricsHelper::CreateForWebContents(
        web_contents, Mode::kNoVr, false);
  }
  metrics_helper->ReportRequestPresent();
}

void VRDisplayHost::ExitPresent() {
  if (immersive_device_)
    immersive_device_->ExitPresent(this);
}

void VRDisplayHost::SetListeningForActivate(bool listening) {
  listening_for_activate_ = listening;
  if (!immersive_device_) {
    return;
  }
  immersive_device_->UpdateListeningForActivate(this);
}

void VRDisplayHost::SetInFocusedFrame(bool in_focused_frame) {
  in_focused_frame_ = in_focused_frame;
  SetListeningForActivate(listening_for_activate_);  // No change, except focus.

  magic_window_controllers_.ForAllPtrs(
      [&in_focused_frame](device::mojom::XRSessionController* controller) {
        controller->SetFrameDataRestricted(!in_focused_frame);
      });
}

void VRDisplayHost::OnChanged() {
  device::mojom::VRDisplayInfoPtr display_info = GetCurrentVRDisplayInfo();
  if (display_info && client_)
    client_->OnChanged(std::move(display_info));
}

void VRDisplayHost::OnDeviceRemoved(BrowserXrDevice* device) {
  if (device == immersive_device_) {
    immersive_device_ = nullptr;
  }

  if (device == magic_window_device_) {
    magic_window_device_ = nullptr;
  }

  if (device == ar_device_) {
    ar_device_ = nullptr;
  }

  // Try to update our VRDisplayInfo.
  OnChanged();
}

void VRDisplayHost::OnDeviceAdded(BrowserXrDevice* device) {
  // Try to update our VRDisplayInfo.  That may use the new device.
  OnChanged();
}

void VRDisplayHost::OnExitPresent() {
  client_->OnExitPresent();
}

void VRDisplayHost::OnBlur() {
  client_->OnBlur();
}

void VRDisplayHost::OnFocus() {
  client_->OnFocus();
}

void VRDisplayHost::OnActivate(device::mojom::VRDisplayEventReason reason,
                               base::OnceCallback<void(bool)> on_handled) {
  client_->OnActivate(reason, std::move(on_handled));
}

void VRDisplayHost::OnDeactivate(device::mojom::VRDisplayEventReason reason) {
  client_->OnDeactivate(reason);
}

bool VRDisplayHost::IsSecureContextRequirementSatisfied() {
  // We require secure connections unless both the webvr flag and the
  // http flag are enabled.
  bool requires_secure_context =
      !kAllowHTTPWebVRWithFlag ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebVR);
  if (!requires_secure_context)
    return true;
  return IsSecureContext(render_frame_host_);
}

}  // namespace vr
