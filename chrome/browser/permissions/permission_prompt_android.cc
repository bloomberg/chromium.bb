// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_prompt_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents), delegate_(nullptr), infobar_(nullptr) {
  DCHECK(web_contents);
}

PermissionPromptAndroid::~PermissionPromptAndroid() {
  GroupedPermissionInfoBarDelegate* infobar_delegate =
      static_cast<GroupedPermissionInfoBarDelegate*>(infobar_->delegate());
  infobar_delegate->PermissionPromptDestroyed();
}

void PermissionPromptAndroid::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionPromptAndroid::Show(
    const std::vector<PermissionRequest*>& requests,
    const std::vector<bool>& values) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (!infobar_service)
    return;

  infobar_ = GroupedPermissionInfoBarDelegate::Create(
      this, infobar_service, requests[0]->GetOrigin(), requests);
}

bool PermissionPromptAndroid::CanAcceptRequestUpdate() {
  return false;
}

void PermissionPromptAndroid::Hide() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (infobar_ && infobar_service) {
    infobar_service->RemoveInfoBar(infobar_);
  }
  infobar_ = nullptr;
}

bool PermissionPromptAndroid::IsVisible() {
  return infobar_ != nullptr;
}

void PermissionPromptAndroid::UpdateAnchorPosition() {
  NOTREACHED() << "UpdateAnchorPosition is not implemented";
}

gfx::NativeWindow PermissionPromptAndroid::GetNativeWindow() {
  NOTREACHED() << "GetNativeWindow is not implemented";
  return nullptr;
}

void PermissionPromptAndroid::Closing() {
  if (delegate_)
    delegate_->Closing();
}

void PermissionPromptAndroid::ToggleAccept(int index, bool value) {
  if (delegate_)
    delegate_->ToggleAccept(index, value);
}

void PermissionPromptAndroid::Accept() {
  if (delegate_)
    delegate_->Accept();
}

void PermissionPromptAndroid::Deny() {
  if (delegate_)
    delegate_->Deny();
}

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents) {
  return base::MakeUnique<PermissionPromptAndroid>(web_contents);
}
