// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"
typedef GeolocationInfoBarDelegateAndroid DelegateType;
#else
typedef GeolocationInfoBarDelegate DelegateType;
#endif


// static
InfoBarDelegate* GeolocationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  return infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new DelegateType(infobar_service, controller, id, requesting_frame,
                       committed_entry ? committed_entry->GetUniqueID() : 0,
                       display_languages)));
}

GeolocationInfoBarDelegate::GeolocationInfoBarDelegate(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(infobar_service),
      controller_(controller),
      id_(id),
      requesting_frame_(requesting_frame.GetOrigin()),
      contents_unique_id_(contents_unique_id),
      display_languages_(display_languages) {
}

GeolocationInfoBarDelegate::~GeolocationInfoBarDelegate() {
}

bool GeolocationInfoBarDelegate::Accept() {
  SetPermission(true, true);
  return true;
}

void GeolocationInfoBarDelegate::SetPermission(bool update_content_setting,
                                               bool allowed) {
  if (web_contents()) {
    GURL embedder = web_contents()->GetLastCommittedURL().GetOrigin();
    controller_->OnPermissionSet(id_, requesting_frame_, embedder,
                                 update_content_setting, allowed);
  }
}

void GeolocationInfoBarDelegate::InfoBarDismissed() {
  SetPermission(false, false);
}

int GeolocationInfoBarDelegate::GetIconID() const {
  return IDR_GEOLOCATION_INFOBAR_ICON;
}

InfoBarDelegate::Type GeolocationInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool GeolocationInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  // This implementation matches InfoBarDelegate::ShouldExpireInternal(), but
  // uses the unique ID we set in the constructor instead of that stored in the
  // base class.
  return (contents_unique_id_ != details.entry->GetUniqueID()) ||
      (content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD);
}

string16 GeolocationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_, display_languages_));
}

string16 GeolocationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}

bool GeolocationInfoBarDelegate::Cancel() {
  SetPermission(true, false);
  return true;
}

string16 GeolocationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GeolocationInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  const char kGeolocationLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
      "https://www.google.com/support/chromeos/bin/answer.py?answer=142065";
#elif defined(OS_ANDROID)
      "https://support.google.com/chrome/?p=mobile_location";
#else
      "https://www.google.com/support/chrome/bin/answer.py?answer=142065";
#endif

  web_contents()->OpenURL(content::OpenURLParams(
      google_util::AppendGoogleLocaleParam(GURL(kGeolocationLearnMoreUrl)),
      content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false));
  return false;  // Do not dismiss the info bar.
}
