// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_prompt_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/common/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
  // Grouped permission requests are not yet supported in dialogs.
  // TODO(timloh): Handle grouped media permissions (camera + microphone).
  if (delegate_->Requests().size() == 1) {
    bool has_gesture = delegate_->Requests()[0]->GetGestureType() ==
                       PermissionRequestGestureType::GESTURE;
    if (PermissionDialogDelegate::ShouldShowDialog(has_gesture)) {
      PermissionDialogDelegate::Create(web_contents_, this);
      return;
    }
  }

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

void PermissionPromptAndroid::TogglePersist(bool value) {
  if (delegate_)
    delegate_->TogglePersist(value);
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

bool PermissionPromptAndroid::ShouldShowPersistenceToggle() const {
  for (const PermissionRequest* request : delegate_->Requests()) {
    if (!request->ShouldShowPersistenceToggle())
      return false;
  }
  return true;
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

base::string16 PermissionPromptAndroid::GetMessageText(size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  DCHECK_LT(position, requests.size());
  return requests[position]->GetMessageText();
}

base::string16 PermissionPromptAndroid::GetMessageTextFragment(
    size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  DCHECK_LT(position, requests.size());
  return requests[position]->GetMessageTextFragment();
}

base::string16 PermissionPromptAndroid::GetLinkText() const {
  if (GetContentSettingType(0) ==
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
    return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  }
  return base::string16();
}

GURL PermissionPromptAndroid::GetLinkURL() const {
  if (GetContentSettingType(0) ==
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
    return GURL(chrome::kEnhancedPlaybackNotificationLearnMoreURL);
  }
  return GURL();
}

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents) {
  return base::MakeUnique<PermissionPromptAndroid>(web_contents);
}
