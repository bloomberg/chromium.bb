// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_notifier.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/page_zoom.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

SkBitmap GetGAIAPictureForNTP(const gfx::Image& image) {
  // This value must match the width and height value of login-status-icon
  // in new_tab.css.
  const int kLength = 27;
  SkBitmap bmp = skia::ImageOperations::Resize(*image.ToSkBitmap(),
      skia::ImageOperations::RESIZE_BEST, kLength, kLength);

  gfx::Canvas canvas(gfx::Size(kLength, kLength), 1.0f, false);
  canvas.DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bmp), 0, 0);

  // Draw a gray border on the inside of the icon.
  SkColor color = SkColorSetARGB(83, 0, 0, 0);
  canvas.DrawRect(gfx::Rect(0, 0, kLength - 1, kLength - 1), color);

  return canvas.ExtractImageRep().sk_bitmap();
}

// Puts the |content| into a span with the given CSS class.
base::string16 CreateSpanWithClass(const base::string16& content,
                                   const std::string& css_class) {
  return base::ASCIIToUTF16("<span class='" + css_class + "'>") +
      net::EscapeForHTML(content) + base::ASCIIToUTF16("</span>");
}

}  // namespace

NTPLoginHandler::NTPLoginHandler() {
}

NTPLoginHandler::~NTPLoginHandler() {
}

void NTPLoginHandler::RegisterMessages() {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  username_pref_.Init(prefs::kGoogleServicesUsername,
                      pref_service,
                      base::Bind(&NTPLoginHandler::UpdateLogin,
                                 base::Unretained(this)));
  signin_allowed_pref_.Init(prefs::kSigninAllowed,
                            pref_service,
                            base::Bind(&NTPLoginHandler::UpdateLogin,
                                       base::Unretained(this)));

  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());

  web_ui()->RegisterMessageCallback("initializeSyncLogin",
      base::Bind(&NTPLoginHandler::HandleInitializeSyncLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showSyncLoginUI",
      base::Bind(&NTPLoginHandler::HandleShowSyncLoginUI,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginMessageSeen",
      base::Bind(&NTPLoginHandler::HandleLoginMessageSeen,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showAdvancedLoginUI",
      base::Bind(&NTPLoginHandler::HandleShowAdvancedLoginUI,
                 base::Unretained(this)));
}

void NTPLoginHandler::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED, type);
  UpdateLogin();
}

void NTPLoginHandler::HandleInitializeSyncLogin(const base::ListValue* args) {
  UpdateLogin();
}

void NTPLoginHandler::HandleShowSyncLoginUI(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  content::WebContents* web_contents = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  if (username.empty()) {
    // The user isn't signed in, show the sign in promo.
    if (signin::ShouldShowPromo(profile)) {
      signin::Source source =
          (web_contents->GetURL().spec() == chrome::kChromeUIAppsURL) ?
              signin::SOURCE_APPS_PAGE_LINK :
              signin::SOURCE_NTP_LINK;
      chrome::ShowBrowserSignin(browser, source);
      RecordInHistogram(NTP_SIGN_IN_PROMO_CLICKED);
    }
  } else if (args->GetSize() == 4) {
    // The user is signed in, show the profiles menu.
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    bool success = args->GetDouble(0, &x);
    DCHECK(success);
    success = args->GetDouble(1, &y);
    DCHECK(success);
    success = args->GetDouble(2, &width);
    DCHECK(success);
    success = args->GetDouble(3, &height);
    DCHECK(success);

    double zoom = content::ZoomLevelToZoomFactor(
        ZoomController::FromWebContents(web_contents)->GetZoomLevel());
    gfx::Rect rect(x * zoom, y * zoom, width * zoom, height * zoom);

    browser->window()->ShowAvatarBubble(web_ui()->GetWebContents(), rect);
    ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::NTP_AVATAR_BUBBLE);
  }
}

void NTPLoginHandler::RecordInHistogram(int type) {
  // Invalid type to record.
  if (type < NTP_SIGN_IN_PROMO_VIEWED ||
      type > NTP_SIGN_IN_PROMO_CLICKED) {
    NOTREACHED();
  } else {
    UMA_HISTOGRAM_ENUMERATION("SyncPromo.NTPPromo", type,
                              NTP_SIGN_IN_PROMO_BUCKET_BOUNDARY);
  }
}

void NTPLoginHandler::HandleLoginMessageSeen(const base::ListValue* args) {
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowNTPBubble, false);
  NewTabUI* ntp_ui = NewTabUI::FromWebUIController(web_ui()->GetController());
  // When instant extended is enabled, there may not be a NewTabUI object.
  if (ntp_ui)
    ntp_ui->set_showing_sync_bubble(true);
}

void NTPLoginHandler::HandleShowAdvancedLoginUI(const base::ListValue* args) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (browser)
    chrome::ShowBrowserSignin(browser, signin::SOURCE_NTP_LINK);
}

void NTPLoginHandler::UpdateLogin() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  base::string16 header, sub_header;
  std::string icon_url;
  if (!username.empty()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (profile_index != std::string::npos) {
      // Only show the profile picture and full name for the single profile
      // case. In the multi-profile case the profile picture is visible in the
      // title bar and the full name can be ambiguous.
      if (cache.GetNumberOfProfiles() == 1) {
        base::string16 name = cache.GetGAIANameOfProfileAtIndex(profile_index);
        if (!name.empty())
          header = CreateSpanWithClass(name, "profile-name");
        const gfx::Image* image =
            cache.GetGAIAPictureOfProfileAtIndex(profile_index);
        if (image)
          icon_url = webui::GetBitmapDataUrl(GetGAIAPictureForNTP(*image));
      }
      if (header.empty()) {
        header = CreateSpanWithClass(base::UTF8ToUTF16(username),
                                     "profile-name");
      }
    }
  } else {
#if !defined(OS_CHROMEOS)
    // Chromeos does not show this status header.
    SigninManager* signin = SigninManagerFactory::GetForProfile(
        profile->GetOriginalProfile());
    if (!profile->IsSupervised() && signin->IsSigninAllowed()) {
      base::string16 signed_in_link = l10n_util::GetStringUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_LINK);
      signed_in_link = CreateSpanWithClass(signed_in_link, "link-span");
      header = l10n_util::GetStringFUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_HEADER,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
      sub_header = l10n_util::GetStringFUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_SUB_HEADER, signed_in_link);
      // Record that the user was shown the promo.
      RecordInHistogram(NTP_SIGN_IN_PROMO_VIEWED);
    }
#endif
  }

  base::StringValue header_value(header);
  base::StringValue sub_header_value(sub_header);
  base::StringValue icon_url_value(icon_url);
  base::FundamentalValue is_user_signed_in(!username.empty());
  web_ui()->CallJavascriptFunction("ntp.updateLogin",
      header_value, sub_header_value, icon_url_value, is_user_signed_in);
}

// static
bool NTPLoginHandler::ShouldShow(Profile* profile) {
#if defined(OS_CHROMEOS)
  // For now we don't care about showing sync status on Chrome OS. The promo
  // UI and the avatar menu don't exist on that platform.
  return false;
#else
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
  return !profile->IsOffTheRecord() && signin && signin->IsSigninAllowed();
#endif
}

// static
void NTPLoginHandler::GetLocalizedValues(Profile* profile,
                                         base::DictionaryValue* values) {
  PrefService* prefs = profile->GetPrefs();
  bool hide_sync = !prefs->GetBoolean(prefs::kSignInPromoShowNTPBubble);

  base::string16 message = hide_sync ? base::string16() :
      l10n_util::GetStringFUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_MESSAGE,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  values->SetString("login_status_message", message);
  values->SetString("login_status_url",
      hide_sync ? std::string() : chrome::kSyncLearnMoreURL);
  values->SetString("login_status_advanced",
      hide_sync ? base::string16() :
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  values->SetString("login_status_dismiss",
      hide_sync ? base::string16() :
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_OK));
}
