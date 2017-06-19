// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_prompt_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents), delegate_(nullptr), persist_(true) {
  DCHECK(web_contents);
}

PermissionPromptAndroid::~PermissionPromptAndroid() {}

void PermissionPromptAndroid::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionPromptAndroid::Show() {
  bool has_gesture = true;
  for (const PermissionRequest* request : delegate_->Requests()) {
    has_gesture &=
        request->GetGestureType() == PermissionRequestGestureType::GESTURE;
  }
  if (PermissionDialogDelegate::ShouldShowDialog(has_gesture)) {
    PermissionDialogDelegate::Create(web_contents_, this);
    return;
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
  persist_ = value;
  if (delegate_)
    delegate_->TogglePersist(value);
}

void PermissionPromptAndroid::Accept() {
  if (delegate_) {
    if (ShouldShowPersistenceToggle()) {
      for (const PermissionRequest* request : delegate_->Requests()) {
        PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
            request->GetContentSettingsType(), persist_);
      }
    }
    delegate_->Accept();
  }
}

void PermissionPromptAndroid::Deny() {
  if (delegate_) {
    if (ShouldShowPersistenceToggle()) {
      for (const PermissionRequest* request : delegate_->Requests()) {
        PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
            request->GetContentSettingsType(), persist_);
      }
    }
    delegate_->Deny();
  }
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

// Grouped permission requests can only be Mic+Camera or Camera+Mic
static void CheckValidRequestGroup(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK_EQ(static_cast<size_t>(2), requests.size());
  DCHECK_EQ(requests[0]->GetOrigin(), requests[1]->GetOrigin());
  DCHECK((requests[0]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_MIC &&
          requests[1]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA) ||
         (requests[0]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA &&
          requests[1]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
}

int PermissionPromptAndroid::GetIconId() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconId();
  CheckValidRequestGroup(requests);
  return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 PermissionPromptAndroid::GetMessageText() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetMessageText();
  CheckValidRequestGroup(requests);
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO,
      url_formatter::FormatUrlForSecurityDisplay(
          requests[0]->GetOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
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
