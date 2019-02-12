// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"

#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace vr {

namespace {
static constexpr base::TimeDelta kPermissionPromptTimeout =
    base::TimeDelta::FromSeconds(5);
}  // namespace

VRUiHostImpl::VRUiHostImpl(device::mojom::XRDeviceId device_id,
                           device::mojom::XRCompositorHostPtr compositor)
    : compositor_(std::move(compositor)), weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  BrowserXRRuntime* runtime =
      XRRuntimeManager::GetInstance()->GetRuntime(device_id);
  if (runtime) {
    runtime->AddObserver(this);
  }
}

VRUiHostImpl::~VRUiHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  // We don't call BrowserXRRuntime::RemoveObserver, because if we are being
  // destroyed, it means the corresponding device has been removed from
  // XRRuntimeManager, and the BrowserXRRuntime has been destroyed.

  StopUiRendering();

  // Clean up permission observer.
  if (permission_request_manager_) {
    permission_request_manager_->RemoveObserver(this);
  }
}

// static
std::unique_ptr<VRUiHost> VRUiHostImpl::Create(
    device::mojom::XRDeviceId device_id,
    device::mojom::XRCompositorHostPtr compositor) {
  DVLOG(1) << __func__;
  return std::make_unique<VRUiHostImpl>(device_id, std::move(compositor));
}

void VRUiHostImpl::SetWebXRWebContents(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Eventually the contents will be used to poll for permissions, or determine
  // what overlays should show.

  // permission_request_manager_ is an unowned pointer; it's owned by
  // WebContents. If the WebContents change, make sure we unregister any
  // pre-existing observers. We only have a non-null permission_request_manager_
  // if we successfully added an observer.
  if (permission_request_manager_) {
    permission_request_manager_->RemoveObserver(this);
    permission_request_manager_ = nullptr;
  }

  if (web_contents_)
    VrTabHelper::SetIsContentDisplayedInHeadset(web_contents_, false);
  if (contents)
    VrTabHelper::SetIsContentDisplayedInHeadset(contents, true);

  web_contents_ = contents;
  if (contents) {
    StartUiRendering();

    PermissionRequestManager::CreateForWebContents(contents);
    permission_request_manager_ =
        PermissionRequestManager::FromWebContents(contents);
    // Attaching a permission request manager to WebContents can fail, so a
    // DCHECK would be inappropriate here. If it fails, the user won't get
    // notified about permission prompts, but other than that the session would
    // work normally.
    if (permission_request_manager_) {
      permission_request_manager_->AddObserver(this);

      // There might already be a visible permission bubble from before
      // we registered the observer, show the HMD message now in that case.
      if (permission_request_manager_->IsBubbleVisible())
        OnBubbleAdded();
    } else {
      DVLOG(1) << __func__ << ": No PermissionRequestManager";
    }
  } else {
    StopUiRendering();
  }
}

void VRUiHostImpl::SetVRDisplayInfo(
    device::mojom::VRDisplayInfoPtr display_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  info_ = std::move(display_info);
  if (ui_rendering_thread_) {
    ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
  }
}

void VRUiHostImpl::StartUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  DCHECK(info_);
  ui_rendering_thread_ = std::make_unique<VRBrowserRendererThreadWin>();
  ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
}

void VRUiHostImpl::StopUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  ui_rendering_thread_ = nullptr;
}

void VRUiHostImpl::OnBubbleAdded() {
  if (!ui_rendering_thread_) {
    DVLOG(1) << __func__ << ": no ui_rendering_thread_";
    return;
  }

  ui_rendering_thread_->StartOverlay(compositor_.get());

  if (web_contents_) {
    content::NavigationEntry* entry =
        web_contents_->GetController().GetVisibleEntry();
    if (entry) {
      GURL gurl = entry->GetVirtualURL();
      ui_rendering_thread_->SetLocationInfo(gurl);
    }
  }

  ui_rendering_thread_->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptGenericPermission);

  is_prompt_showing_in_headset_ = true;
  current_prompt_sequence_num_++;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VRUiHostImpl::RemoveHeadsetNotificationPrompt,
                     weak_ptr_factory_.GetWeakPtr(),
                     current_prompt_sequence_num_),
      kPermissionPromptTimeout);
}

void VRUiHostImpl::OnBubbleRemoved() {
  RemoveHeadsetNotificationPrompt(current_prompt_sequence_num_);
}

void VRUiHostImpl::RemoveHeadsetNotificationPrompt(int prompt_sequence_num) {
  if (!is_prompt_showing_in_headset_)
    return;
  if (prompt_sequence_num != current_prompt_sequence_num_)
    return;
  is_prompt_showing_in_headset_ = false;
  ui_rendering_thread_->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptNone);
  ui_rendering_thread_->StopOverlay();
}
}  // namespace vr
