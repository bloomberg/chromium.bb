// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_prompt_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_request.h"

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents), delegate_(nullptr) {
  DCHECK(web_contents);
}

PermissionPromptAndroid::~PermissionPromptAndroid() {}

void PermissionPromptAndroid::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionPromptAndroid::Show() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (!infobar_service)
    return;

  GroupedPermissionInfoBarDelegate::Create(
      this, infobar_service, delegate_->Requests()[0]->GetOrigin());
}

bool PermissionPromptAndroid::CanAcceptRequestUpdate() {
  return false;
}

bool PermissionPromptAndroid::HidesAutomatically() {
  return true;
}

void PermissionPromptAndroid::Hide() {
  // Hide() is only called if HidesAutomatically() returns false or
  // CanAcceptRequestUpdate() return true.
  NOTREACHED();
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

size_t PermissionPromptAndroid::PermissionCount() const {
  return delegate_->Requests().size();
}

ContentSettingsType PermissionPromptAndroid::GetContentSettingType(
    size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  DCHECK_LT(position, requests.size());
  return requests[position]->GetContentSettingsType();
}

int PermissionPromptAndroid::GetIconIdForPermission(size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  DCHECK_LT(position, requests.size());
  return requests[position]->GetIconId();
}

base::string16 PermissionPromptAndroid::GetMessageTextFragment(
    size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  DCHECK_LT(position, requests.size());
  return requests[position]->GetMessageTextFragment();
}

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents) {
  return base::MakeUnique<PermissionPromptAndroid>(web_contents);
}
