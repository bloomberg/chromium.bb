// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_notifier.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/webui/web_ui_util.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

SkBitmap GetGAIAPictureForNTP(const gfx::Image& image) {
  // This value must match the width and height value of login-status-icon
  // in new_tab.css.
  const int kLength = 27;
  SkBitmap bmp = skia::ImageOperations::Resize(*image.ToSkBitmap(),
      skia::ImageOperations::RESIZE_BEST, kLength, kLength);

  gfx::Canvas canvas(gfx::Size(kLength, kLength), ui::SCALE_FACTOR_100P,
      false);
  canvas.DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bmp), 0, 0);

  // Draw a gray border on the inside of the icon.
  SkColor color = SkColorSetARGB(83, 0, 0, 0);
  canvas.DrawRect(gfx::Rect(0, 0, kLength - 1, kLength - 1), color);

  return canvas.ExtractImageRep().sk_bitmap();
}

// Puts the |content| into a span with the given CSS class.
string16 CreateSpanWithClass(const string16& content,
                             const std::string& css_class) {
  return ASCIIToUTF16("<span class='" + css_class + "'>") +
      net::EscapeForHTML(content) + ASCIIToUTF16("</span>");
}

} // namespace

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
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED) {
    UpdateLogin();
  } else {
    NOTREACHED();
  }
}

void NTPLoginHandler::HandleInitializeSyncLogin(const ListValue* args) {
  UpdateLogin();
}

void NTPLoginHandler::HandleShowSyncLoginUI(const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  content::WebContents* web_contents = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  if (username.empty()) {
#if !defined(OS_ANDROID)
    // The user isn't signed in, show the sync promo.
    if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
      chrome::ShowSyncSetup(browser, SyncPromoUI::SOURCE_NTP_LINK);
      RecordInHistogram(NTP_SIGN_IN_PROMO_CLICKED);
    }
#endif
  } else if (args->GetSize() == 4 &&
             chrome::IsCommandEnabled(browser, IDC_SHOW_AVATAR_MENU)) {
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

    double zoom =
        WebKit::WebView::zoomLevelToZoomFactor(web_contents->GetZoomLevel());
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

void NTPLoginHandler::HandleLoginMessageSeen(const ListValue* args) {
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kSyncPromoShowNTPBubble, false);
  NewTabUI* ntp_ui = NewTabUI::FromWebUIController(web_ui()->GetController());
  ntp_ui->set_showing_sync_bubble(true);
}

void NTPLoginHandler::HandleShowAdvancedLoginUI(const ListValue* args) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (browser)
    chrome::ShowSyncSetup(browser, SyncPromoUI::SOURCE_NTP_LINK);
}

void NTPLoginHandler::UpdateLogin() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  string16 header, sub_header;
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
        string16 name = cache.GetGAIANameOfProfileAtIndex(profile_index);
        if (!name.empty())
          header = CreateSpanWithClass(name, "profile-name");
        const gfx::Image* image =
            cache.GetGAIAPictureOfProfileAtIndex(profile_index);
        if (image)
          icon_url = webui::GetBitmapDataUrl(GetGAIAPictureForNTP(*image));
      }
      if (header.empty())
        header = CreateSpanWithClass(UTF8ToUTF16(username), "profile-name");
    }
  } else {
#if !defined(OS_ANDROID)
    // Android uses a custom sync promo
    if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
      string16 signed_in_link = l10n_util::GetStringUTF16(
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

  StringValue header_value(header);
  StringValue sub_header_value(sub_header);
  StringValue icon_url_value(icon_url);
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
  if (profile->IsOffTheRecord())
    return false;

  return profile->GetOriginalProfile()->IsSyncAccessible();
#endif
}

// static
void NTPLoginHandler::GetLocalizedValues(Profile* profile,
                                         DictionaryValue* values) {
  PrefService* prefs = profile->GetPrefs();
  std::string error_message = prefs->GetString(prefs::kSyncPromoErrorMessage);
  bool hide_sync = !prefs->GetBoolean(prefs::kSyncPromoShowNTPBubble);

  string16 message =
      hide_sync ? string16() :
          !error_message.empty() ? UTF8ToUTF16(error_message) :
              l10n_util::GetStringFUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_MESSAGE,
                  l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  values->SetString("login_status_message", message);
  values->SetString("login_status_url",
      hide_sync ? std::string() : chrome::kSyncLearnMoreURL);
  values->SetString("login_status_advanced",
      hide_sync || !error_message.empty() ? string16() :
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  values->SetString("login_status_dismiss",
      hide_sync ? string16() :
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_OK));
}
