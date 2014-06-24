// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
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

namespace {

enum GeolocationInfoBarDelegateEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // The bar was created.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_CREATE = 0,

  // User allowed use of geolocation.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_ALLOW = 1,

  // User denied use of geolocation.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_DENY = 2,

  // User dismissed the bar.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_DISMISS = 3,

  // User clicked on link.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_LINK_CLICK = 4,

  // User ignored the bar.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_IGNORED = 5,

  // NOTE: Add entries only immediately above this line.
  GEOLOCATION_INFO_BAR_DELEGATE_EVENT_COUNT = 6
};

void RecordUmaEvent(GeolocationInfoBarDelegateEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Geolocation.InfoBarDelegate.Event",
      event, GEOLOCATION_INFO_BAR_DELEGATE_EVENT_COUNT);
}

}  // namespace

// static
infobars::InfoBar* GeolocationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages,
    const std::string& accept_button_label) {
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_CREATE);
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  GeolocationInfoBarDelegate* const delegate = new DelegateType(
          controller, id, requesting_frame,
          committed_entry ? committed_entry->GetUniqueID() : 0,
          display_languages, accept_button_label);

  infobars::InfoBar* infobar = ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(delegate)).release();
  return infobar_service->AddInfoBar(scoped_ptr<infobars::InfoBar>(infobar));
}

GeolocationInfoBarDelegate::GeolocationInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
    const std::string& display_languages,
    const std::string& accept_button_label)
    : ConfirmInfoBarDelegate(),
      controller_(controller),
      id_(id),
      requesting_frame_(requesting_frame.GetOrigin()),
      contents_unique_id_(contents_unique_id),
      display_languages_(display_languages),
      user_has_interacted_(false) {
}

GeolocationInfoBarDelegate::~GeolocationInfoBarDelegate() {
  if (!user_has_interacted_)
    RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_IGNORED);
}

bool GeolocationInfoBarDelegate::Accept() {
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_ALLOW);
  set_user_has_interacted();
  SetPermission(true, true);
  return true;
}

void GeolocationInfoBarDelegate::SetPermission(bool update_content_setting,
                                               bool allowed) {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  controller_->OnPermissionSet(
        id_, requesting_frame_,
        web_contents->GetLastCommittedURL().GetOrigin(),
        update_content_setting, allowed);
}

void GeolocationInfoBarDelegate::InfoBarDismissed() {
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_DISMISS);
  set_user_has_interacted();
  SetPermission(false, false);
}

int GeolocationInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_GEOLOCATION;
}

infobars::InfoBarDelegate::Type GeolocationInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

bool GeolocationInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  // This implementation matches InfoBarDelegate::ShouldExpireInternal(), but
  // uses the unique ID we set in the constructor instead of that stored in the
  // base class.
  return (contents_unique_id_ != details.entry_id) || details.is_reload;
}

base::string16 GeolocationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_, display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
}

base::string16 GeolocationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}

bool GeolocationInfoBarDelegate::Cancel() {
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_DENY);
  set_user_has_interacted();
  SetPermission(true, false);
  return true;
}

base::string16 GeolocationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GeolocationInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_EVENT_LINK_CLICK);
  const char kGeolocationLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
      "https://www.google.com/support/chromeos/bin/answer.py?answer=142065";
#elif defined(OS_ANDROID)
      "https://support.google.com/chrome/?p=mobile_location";
#else
      "https://www.google.com/support/chrome/bin/answer.py?answer=142065";
#endif

  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL(kGeolocationLearnMoreUrl),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;  // Do not dismiss the info bar.
}
