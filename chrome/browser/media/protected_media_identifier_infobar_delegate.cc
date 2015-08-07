// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_queue_controller.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* ProtectedMediaIdentifierInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  return infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new ProtectedMediaIdentifierInfoBarDelegate(
              controller, id, requesting_frame, display_languages))));
}


ProtectedMediaIdentifierInfoBarDelegate::
    ProtectedMediaIdentifierInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(),
      controller_(controller),
      id_(id),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

ProtectedMediaIdentifierInfoBarDelegate::
    ~ProtectedMediaIdentifierInfoBarDelegate() {
}

bool ProtectedMediaIdentifierInfoBarDelegate::Accept() {
  SetPermission(true, true);
  return true;
}

void ProtectedMediaIdentifierInfoBarDelegate::SetPermission(
    bool update_content_setting,
    bool allowed) {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  controller_->OnPermissionSet(id_, requesting_frame_,
                               web_contents->GetLastCommittedURL().GetOrigin(),
                               update_content_setting, allowed);
}

infobars::InfoBarDelegate::Type
ProtectedMediaIdentifierInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int ProtectedMediaIdentifierInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
}

void ProtectedMediaIdentifierInfoBarDelegate::InfoBarDismissed() {
  SetPermission(false, false);
}

base::string16 ProtectedMediaIdentifierInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_QUESTION,
      url_formatter::FormatUrl(requesting_frame_.GetOrigin(),
                               display_languages_));
}

base::string16 ProtectedMediaIdentifierInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PROTECTED_MEDIA_IDENTIFIER_ALLOW_BUTTON :
      IDS_PROTECTED_MEDIA_IDENTIFIER_DENY_BUTTON);
}

bool ProtectedMediaIdentifierInfoBarDelegate::Cancel() {
  SetPermission(true, false);
  return true;
}

base::string16 ProtectedMediaIdentifierInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ProtectedMediaIdentifierInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())
      ->OpenURL(content::OpenURLParams(
          GURL(chrome::kEnhancedPlaybackNotificationLearnMoreURL),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
  return false; // Do not dismiss the info bar.
}
