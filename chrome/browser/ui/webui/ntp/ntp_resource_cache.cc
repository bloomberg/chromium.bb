// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"
#include "chrome/browser/ui/webui/sync_promo_ui.h"
#include "chrome/browser/ui/webui/sync_setup_handler.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#elif defined(OS_POSIX)
#include "chrome/browser/ui/gtk/bookmarks/bookmark_bar_gtk.h"
#endif

using base::Time;
using content::BrowserThread;

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char kLearnMoreIncognitoUrl[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=95464";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=95464";
#endif

// The URL for the Learn More page shown on guest session new tab.
const char kLearnMoreGuestSessionUrl[] =
    "https://www.google.com/support/chromeos/bin/answer.py?answer=1057090";

// The URL for bookmark sync service help.
const char kSyncServiceHelpUrl[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=165139";

// The URL to be loaded to display Help.
const char kHelpContentUrl[] =
    "https://www.google.com/support/chrome/";

string16 GetUrlWithLang(const GURL& url) {
  return ASCIIToUTF16(google_util::AppendGoogleLocaleParam(url).spec());
}

std::string SkColorToRGBAString(SkColor color) {
  // We convert the alpha using DoubleToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return base::StringPrintf(
      "rgba(%d,%d,%d,%s)",
      SkColorGetR(color),
      SkColorGetG(color),
      SkColorGetB(color),
      base::DoubleToString(SkColorGetA(color) / 255.0).c_str());
}

// Creates an rgb string for an SkColor, but leaves the alpha blank so that the
// css can fill it in.
std::string SkColorToRGBComponents(SkColor color) {
  return base::StringPrintf(
      "%d,%d,%d",
      SkColorGetR(color),
      SkColorGetG(color),
      SkColorGetB(color));
}

// Get the CSS string for the background position on the new tab page for the
// states when the bar is attached or detached.
std::string GetNewTabBackgroundCSS(const ui::ThemeProvider* theme_provider,
                                   bool bar_attached) {
  int alignment;
  theme_provider->GetDisplayProperty(
      ThemeService::NTP_BACKGROUND_ALIGNMENT, &alignment);

  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  if (bar_attached)
    return ThemeService::AlignmentToString(alignment);

  // The bar is detached, so we must offset the background by the bar size
  // if it's a top-aligned bar.
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
  int offset = browser_defaults::kNewtabBookmarkBarHeight;
#elif defined(OS_MACOSX)
  int offset = bookmarks::kNTPBookmarkBarHeight;
#elif defined(OS_POSIX)
  int offset = BookmarkBarGtk::kBookmarkBarNTPHeight;
#else
  int offset = 0;
#endif

  if (alignment & ThemeService::ALIGN_TOP) {
    if (alignment & ThemeService::ALIGN_LEFT)
      return "0% " + base::IntToString(-offset) + "px";
    else if (alignment & ThemeService::ALIGN_RIGHT)
      return "100% " + base::IntToString(-offset) + "px";
    return "center " + base::IntToString(-offset) + "px";
  }
  return ThemeService::AlignmentToString(alignment);
}

// How the background image on the new tab page should be tiled (see tiling
// masks in theme_service.h).
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider* theme_provider) {
  int repeat_mode;
  theme_provider->GetDisplayProperty(
      ThemeService::NTP_BACKGROUND_TILING, &repeat_mode);
  return ThemeService::TilingToString(repeat_mode);
}

// Is the current time within a given date range?
bool InDateRange(double begin, double end) {
  Time start_time = Time::FromDoubleT(begin);
  Time end_time = Time::FromDoubleT(end);
  return start_time < Time::Now() && end_time > Time::Now();
}

}  // namespace

NTPResourceCache::NTPResourceCache(Profile* profile) : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));
  registrar_.Add(this, chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NTP4_INTRO_PREF_CHANGED,
                 content::NotificationService::AllSources());

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kSyncAcknowledgedSyncTypes, this);
  pref_change_registrar_.Add(prefs::kShowBookmarkBar, this);
  pref_change_registrar_.Add(prefs::kHomePageIsNewTabPage, this);
  pref_change_registrar_.Add(prefs::kNTPShownPage, this);
  pref_change_registrar_.Add(prefs::kSyncPromoShowNTPBubble, this);
}

NTPResourceCache::~NTPResourceCache() {}

RefCountedMemory* NTPResourceCache::GetNewTabHTML(bool is_incognito) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_incognito) {
    if (!new_tab_incognito_html_.get())
      CreateNewTabIncognitoHTML();
  } else {
    if (!new_tab_html_.get())
      CreateNewTabHTML();
  }
  return is_incognito ? new_tab_incognito_html_.get()
                      : new_tab_html_.get();
}

RefCountedMemory* NTPResourceCache::GetNewTabCSS(bool is_incognito) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_incognito) {
    if (!new_tab_incognito_css_.get())
      CreateNewTabIncognitoCSS();
  } else {
    if (!new_tab_css_.get())
      CreateNewTabCSS();
  }
  return is_incognito ? new_tab_incognito_css_.get()
                      : new_tab_css_.get();
}

void NTPResourceCache::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Invalidate the cache.
  if (chrome::NOTIFICATION_BROWSER_THEME_CHANGED == type ||
      chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED == type) {
    new_tab_incognito_html_ = NULL;
    new_tab_html_ = NULL;
    new_tab_incognito_css_ = NULL;
    new_tab_css_ = NULL;
  } else if (chrome::NOTIFICATION_PREF_CHANGED == type ||
              chrome::NTP4_INTRO_PREF_CHANGED) {
    // A change occurred to one of the preferences we care about, so flush the
    // cache.
    new_tab_incognito_html_ = NULL;
    new_tab_html_ = NULL;
  } else {
    NOTREACHED();
  }
}

void NTPResourceCache::CreateNewTabIncognitoHTML() {
  DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  int new_tab_message_ids = IDS_NEW_TAB_OTR_MESSAGE;
  int new_tab_html_idr = IDR_INCOGNITO_TAB_HTML;
  const char* new_tab_link = kLearnMoreIncognitoUrl;
  // TODO(altimofeev): consider implementation without 'if def' usage.
#if defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    new_tab_message_ids = IDS_NEW_TAB_GUEST_SESSION_MESSAGE;
    new_tab_html_idr = IDR_GUEST_SESSION_TAB_HTML;
    new_tab_link = kLearnMoreGuestSessionUrl;
  }
#endif
  localized_strings.SetString("content",
      l10n_util::GetStringFUTF16(new_tab_message_ids,
                                 GetUrlWithLang(GURL(new_tab_link))));
  localized_strings.SetString("extensionsmessage",
      l10n_util::GetStringFUTF16(
          IDS_NEW_TAB_OTR_EXTENSIONS_MESSAGE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          ASCIIToUTF16(std::string(chrome::kChromeUISettingsURL) +
                       chrome::kExtensionsSubPage)));
  bool bookmark_bar_attached = profile_->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar);
  localized_strings.SetString("bookmarkbarattached",
      bookmark_bar_attached ? "true" : "false");

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          new_tab_html_idr));

  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      incognito_tab_html, &localized_strings);

  new_tab_incognito_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabHTML() {
  // TODO(estade): these strings should be defined in their relevant handlers
  // (in GetLocalizedValues) and should have more legible names.
  // Show the profile name in the title and most visited labels if the current
  // profile is not the default.
  string16 apps = l10n_util::GetStringUTF16(IDS_NEW_TAB_APPS);
  string16 title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  DictionaryValue localized_strings;
  localized_strings.SetString("bookmarkbarattached",
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      "true" : "false");
  localized_strings.SetString("hasattribution",
      ThemeServiceFactory::GetForProfile(profile_)->HasCustomImage(
          IDR_THEME_NTP_ATTRIBUTION) ?
      "true" : "false");
  localized_strings.SetString("apps", apps);
  localized_strings.SetString("title", title);
  localized_strings.SetString("mostvisited",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_MOST_VISITED));
  localized_strings.SetString("restoreThumbnailsShort",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK));
  localized_strings.SetString("recentlyclosed",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED));
  localized_strings.SetString("closedwindowsingle",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE));
  localized_strings.SetString("closedwindowmultiple",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE));
  localized_strings.SetString("attributionintro",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  localized_strings.SetString("thumbnailremovednotification",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION));
  localized_strings.SetString("undothumbnailremove",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE));
  localized_strings.SetString("removethumbnailtooltip",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP));
  localized_strings.SetString("appuninstall",
      l10n_util::GetStringFUTF16(
          IDS_NEW_TAB_APP_UNINSTALL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  localized_strings.SetString("appoptions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_OPTIONS));
  localized_strings.SetString("appcreateshortcut",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_CREATE_SHORTCUT));
  localized_strings.SetString("appDefaultPageName",
      l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME));
  localized_strings.SetString("applaunchtypepinned",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_PINNED));
  localized_strings.SetString("applaunchtyperegular",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_REGULAR));
  localized_strings.SetString("applaunchtypewindow",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));
  localized_strings.SetString("applaunchtypefullscreen",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN));
  localized_strings.SetString("syncpromotext",
      l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL));
  localized_strings.SetString("syncLinkText",
      l10n_util::GetStringUTF16(IDS_SYNC_ADVANCED_OPTIONS));
#if defined(OS_CHROMEOS)
  localized_strings.SetString("expandMenu",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_CLOSE_MENU_EXPAND));
#endif

  NewTabPageHandler::GetLocalizedValues(profile_, &localized_strings);
  NTPLoginHandler::GetLocalizedValues(profile_, &localized_strings);

  // Don't initiate the sync related message passing with the page if the sync
  // code is not present.
  if (profile_->GetProfileSyncService())
    localized_strings.SetString("syncispresent", "true");
  else
    localized_strings.SetString("syncispresent", "false");

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  // Control fade and resize animations.
  std::string anim =
      ui::Animation::ShouldRenderRichAnimation() ? "true" : "false";
  localized_strings.SetString("anim", anim);

  int alignment;
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  tp->GetDisplayProperty(ThemeService::NTP_BACKGROUND_ALIGNMENT, &alignment);
  localized_strings.SetString("themegravity",
      (alignment & ThemeService::ALIGN_RIGHT) ? "right" : "");

  // If the user has preferences for a start and end time for a custom logo,
  // and the time now is between these two times, show the custom logo.
  if (profile_->GetPrefs()->FindPreference(prefs::kNTPCustomLogoStart) &&
      profile_->GetPrefs()->FindPreference(prefs::kNTPCustomLogoEnd)) {
    localized_strings.SetString("customlogo",
        InDateRange(profile_->GetPrefs()->GetDouble(prefs::kNTPCustomLogoStart),
                    profile_->GetPrefs()->GetDouble(prefs::kNTPCustomLogoEnd)) ?
        "true" : "false");
  } else {
    localized_strings.SetString("customlogo", "false");
  }

  // If the user has preferences for a start and end time for a promo from
  // the server, and this promo string exists, set the localized string.
  if (PromoResourceService::CanShowNotificationPromo(profile_)) {
    localized_strings.SetString("serverpromo",
        profile_->GetPrefs()->GetString(prefs::kNTPPromoLine));
    UserMetrics::RecordAction(UserMetricsAction("NTPPromoShown"));
  }

  // Load the new tab page appropriate for this build
  std::string full_html;
  base::StringPiece new_tab_html(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_NEW_TAB_4_HTML));
  full_html = jstemplate_builder::GetI18nTemplateHtml(new_tab_html,
                                                      &localized_strings);
  new_tab_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabIncognitoCSS() {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      tp->GetColor(ThemeService::COLOR_NTP_BACKGROUND);

  // Generate the replacements.
  std::vector<std::string> subst;

  // Cache-buster for background.
  subst.push_back(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID));  // $1

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));  // $2
  subst.push_back(GetNewTabBackgroundCSS(tp, false));  // $3
  subst.push_back(GetNewTabBackgroundCSS(tp, true));  // $4
  subst.push_back(GetNewTabBackgroundTilingCSS(tp));  // $5

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_NEW_INCOGNITO_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string full_css = ReplaceStringPlaceholders(
      new_tab_theme_css, subst, NULL);

  new_tab_incognito_css_ = base::RefCountedString::TakeString(&full_css);
}

void NTPResourceCache::CreateNewTabCSS() {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      tp->GetColor(ThemeService::COLOR_NTP_BACKGROUND);
  SkColor color_text = tp->GetColor(ThemeService::COLOR_NTP_TEXT);
  SkColor color_link = tp->GetColor(ThemeService::COLOR_NTP_LINK);
  SkColor color_link_underline =
      tp->GetColor(ThemeService::COLOR_NTP_LINK_UNDERLINE);

  SkColor color_section =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION);
  SkColor color_section_text =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_TEXT);
  SkColor color_section_link =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_LINK);
  SkColor color_section_link_underline =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_LINK_UNDERLINE);
  SkColor color_section_header_text =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_HEADER_TEXT);
  SkColor color_section_header_text_hover =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_HEADER_TEXT_HOVER);
  SkColor color_section_header_rule =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_HEADER_RULE);
  SkColor color_section_header_rule_light =
      tp->GetColor(ThemeService::COLOR_NTP_SECTION_HEADER_RULE_LIGHT);
  SkColor color_text_light =
      tp->GetColor(ThemeService::COLOR_NTP_TEXT_LIGHT);

  SkColor color_header =
      tp->GetColor(ThemeService::COLOR_NTP_HEADER);
  // Generate a lighter color for the header gradients.
  color_utils::HSL header_lighter;
  color_utils::SkColorToHSL(color_header, &header_lighter);
  header_lighter.l += (1 - header_lighter.l) * 0.33;
  SkColor color_header_gradient_light =
      color_utils::HSLToSkColor(header_lighter, SkColorGetA(color_header));

  // Generate section border color from the header color. See
  // BookmarkBarView::Paint for how we do this for the bookmark bar
  // borders.
  SkColor color_section_border =
      SkColorSetARGB(80,
                     SkColorGetR(color_header),
                     SkColorGetG(color_header),
                     SkColorGetB(color_header));

  // Generate the replacements.
  std::vector<std::string> subst;

  // Cache-buster for background.
  subst.push_back(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID));  // $1

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));  // $2
  subst.push_back(GetNewTabBackgroundCSS(tp, false));  // $3
  subst.push_back(GetNewTabBackgroundCSS(tp, true));  // $4
  subst.push_back(GetNewTabBackgroundTilingCSS(tp));  // $5
  subst.push_back(SkColorToRGBAString(color_header));  // $6
  subst.push_back(SkColorToRGBAString(color_header_gradient_light));  // $7
  subst.push_back(SkColorToRGBAString(color_text));  // $8
  subst.push_back(SkColorToRGBAString(color_link));  // $9
  subst.push_back(SkColorToRGBAString(color_section));  // $10
  subst.push_back(SkColorToRGBAString(color_section_border));  // $11
  subst.push_back(SkColorToRGBAString(color_section_text));  // $12
  subst.push_back(SkColorToRGBAString(color_section_link));  // $13
  subst.push_back(SkColorToRGBAString(color_link_underline));  // $14
  subst.push_back(SkColorToRGBAString(color_section_link_underline));  // $15
  subst.push_back(SkColorToRGBAString(color_section_header_text)); // $16
  subst.push_back(SkColorToRGBAString(
      color_section_header_text_hover)); // $17
  subst.push_back(SkColorToRGBAString(color_section_header_rule));  // $18
  subst.push_back(SkColorToRGBAString(
      color_section_header_rule_light));  // $19
  subst.push_back(SkColorToRGBAString(
      SkColorSetA(color_section_header_rule, 0)));  // $20
  subst.push_back(SkColorToRGBAString(color_text_light));  // $21
  subst.push_back(SkColorToRGBComponents(color_section_border));  // $22
  subst.push_back(SkColorToRGBComponents(color_text));  // $23

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_4_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string css_string;
  css_string = ReplaceStringPlaceholders(new_tab_theme_css, subst, NULL);
  new_tab_css_ = base::RefCountedString::TakeString(&css_string);
}
