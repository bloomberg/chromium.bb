// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/ntp_resource_cache.h"

#include <algorithm>
#include <vector>

#include "app/animation.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/color_utils.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/bookmark_bar_view.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/bookmark_bar_constants.h"
#elif defined(OS_POSIX)
#include "chrome/browser/gtk/bookmark_bar_gtk.h"
#endif

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char kLearnMoreIncognitoUrl[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=95464";

// The URL for bookmark sync service help.
const char kSyncServiceHelpUrl[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=165139";

// The URL to be loaded to display Help.
const char kHelpContentUrl[] =
    "http://www.google.com/support/chrome/";

std::wstring GetUrlWithLang(const GURL& url) {
  return ASCIIToWide(google_util::AppendGoogleLocaleParam(url).spec());
}

std::string SkColorToRGBAString(SkColor color) {
  // We convert the alpha using DoubleToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return StringPrintf("rgba(%d,%d,%d,%s)", SkColorGetR(color),
      SkColorGetG(color), SkColorGetB(color),
      DoubleToString(SkColorGetA(color) / 255.0).c_str());
}

// Get the CSS string for the background position on the new tab page for the
// states when the bar is attached or detached.
std::string GetNewTabBackgroundCSS(const ThemeProvider* theme_provider,
                                   bool bar_attached) {
  int alignment;
  theme_provider->GetDisplayProperty(
      BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT, &alignment);

  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  if (bar_attached)
    return BrowserThemeProvider::AlignmentToString(alignment);

  // The bar is detached, so we must offset the background by the bar size
  // if it's a top-aligned bar.
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
  int offset = BookmarkBarView::kNewtabBarHeight;
#elif defined(OS_MACOSX)
  int offset = bookmarks::kNTPBookmarkBarHeight;
#elif defined(OS_POSIX)
  int offset = BookmarkBarGtk::kBookmarkBarNTPHeight;
#else
  int offset = 0;
#endif

  if (alignment & BrowserThemeProvider::ALIGN_TOP) {
    if (alignment & BrowserThemeProvider::ALIGN_LEFT)
      return "0% " + IntToString(-offset) + "px";
    else if (alignment & BrowserThemeProvider::ALIGN_RIGHT)
      return "100% " + IntToString(-offset) + "px";
    return "center " + IntToString(-offset) + "px";
  }
  return BrowserThemeProvider::AlignmentToString(alignment);
}

// How the background image on the new tab page should be tiled (see tiling
// masks in browser_theme_provider.h).
std::string GetNewTabBackgroundTilingCSS(const ThemeProvider* theme_provider) {
  int repeat_mode;
  theme_provider->GetDisplayProperty(
      BrowserThemeProvider::NTP_BACKGROUND_TILING, &repeat_mode);
  return BrowserThemeProvider::TilingToString(repeat_mode);
}

}  // namespace

NTPResourceCache::NTPResourceCache(Profile* profile) : profile_(profile) {
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->AddPrefObserver(prefs::kShowBookmarkBar, this);
  pref_service->AddPrefObserver(prefs::kNTPShownSections, this);
}

NTPResourceCache::~NTPResourceCache() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->RemovePrefObserver(prefs::kShowBookmarkBar, this);
  pref_service->RemovePrefObserver(prefs::kNTPShownSections, this);
}

RefCountedBytes* NTPResourceCache::GetNewTabHTML(bool is_off_the_record) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (is_off_the_record) {
    if (!new_tab_incognito_html_.get())
      CreateNewTabIncognitoHTML();
  } else {
    if (!new_tab_html_.get())
      CreateNewTabHTML();
  }
  return is_off_the_record ? new_tab_incognito_html_.get()
                           : new_tab_html_.get();
}

RefCountedBytes* NTPResourceCache::GetNewTabCSS(bool is_off_the_record) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (is_off_the_record) {
    if (!new_tab_incognito_css_.get())
      CreateNewTabIncognitoCSS();
  } else {
    if (!new_tab_css_.get())
      CreateNewTabCSS();
  }
  return is_off_the_record ? new_tab_incognito_css_.get()
                           : new_tab_css_.get();
}

void NTPResourceCache::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  // Invalidate the cache.
  if (NotificationType::BROWSER_THEME_CHANGED == type) {
    new_tab_incognito_html_ = NULL;
    new_tab_html_ = NULL;
    new_tab_incognito_css_ = NULL;
    new_tab_css_ = NULL;
  } else if (NotificationType::PREF_CHANGED == type) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowBookmarkBar ||
        *pref_name == prefs::kHomePageIsNewTabPage ||
        *pref_name == prefs::kNTPShownSections) {
      new_tab_incognito_html_ = NULL;
      new_tab_html_ = NULL;
    } else {
      NOTREACHED();
    }
  } else {
    NOTREACHED();
  }
}

void NTPResourceCache::CreateNewTabIncognitoHTML() {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"content",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_MESSAGE,
                            GetUrlWithLang(GURL(kLearnMoreIncognitoUrl))));
  localized_strings.SetString(L"extensionsmessage",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_EXTENSIONS_MESSAGE,
                            l10n_util::GetString(IDS_PRODUCT_NAME),
                            ASCIIToWide(chrome::kChromeUIExtensionsURL)));
  bool bookmark_bar_attached = profile_->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar);
  localized_strings.SetString(L"bookmarkbarattached",
      bookmark_bar_attached ? "true" : "false");

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_INCOGNITO_TAB_HTML));

  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      incognito_tab_html, &localized_strings);

  new_tab_incognito_html_ = new RefCountedBytes;
  new_tab_incognito_html_->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(),
            new_tab_incognito_html_->data.begin());
}

void NTPResourceCache::CreateNewTabHTML() {
  // Show the profile name in the title and most visited labels if the current
  // profile is not the default.
  std::wstring title = l10n_util::GetString(IDS_NEW_TAB_TITLE);
  std::wstring most_visited = l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED);
  DictionaryValue localized_strings;
  localized_strings.SetString(L"bookmarkbarattached",
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      "true" : "false");
  localized_strings.SetString(L"hasattribution",
      profile_->GetThemeProvider()->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ?
      "true" : "false");
  localized_strings.SetString(L"title", title);
  localized_strings.SetString(L"mostvisited", most_visited);
  localized_strings.SetString(L"restorethumbnails",
      l10n_util::GetString(IDS_NEW_TAB_RESTORE_THUMBNAILS_LINK));
  localized_strings.SetString(L"recentlyclosed",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED));
  localized_strings.SetString(L"closedwindowsingle",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE));
  localized_strings.SetString(L"closedwindowmultiple",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE));
  localized_strings.SetString(L"attributionintro",
      l10n_util::GetString(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  localized_strings.SetString(L"thumbnailremovednotification",
      l10n_util::GetString(IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION));
  localized_strings.SetString(L"undothumbnailremove",
      l10n_util::GetString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE));
  localized_strings.SetString(L"removethumbnailtooltip",
      l10n_util::GetString(IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP));
  localized_strings.SetString(L"pinthumbnailtooltip",
      l10n_util::GetString(IDS_NEW_TAB_PIN_THUMBNAIL_TOOLTIP));
  localized_strings.SetString(L"unpinthumbnailtooltip",
      l10n_util::GetString(IDS_NEW_TAB_UNPIN_THUMBNAIL_TOOLTIP));
  localized_strings.SetString(L"showhidethumbnailtooltip",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_HIDE_THUMBNAIL_TOOLTIP));
  localized_strings.SetString(L"showhidelisttooltip",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_HIDE_LIST_TOOLTIP));
  localized_strings.SetString(L"pagedisplaytooltip",
      l10n_util::GetString(IDS_NEW_TAB_PAGE_DISPLAY_TOOLTIP));
  localized_strings.SetString(L"firstrunnotification",
      l10n_util::GetString(IDS_NEW_TAB_FIRST_RUN_NOTIFICATION));
  localized_strings.SetString(L"closefirstrunnotification",
      l10n_util::GetString(IDS_NEW_TAB_CLOSE_FIRST_RUN_NOTIFICATION));
  localized_strings.SetString(L"tips",
      l10n_util::GetString(IDS_NEW_TAB_TIPS));
  localized_strings.SetString(L"close", l10n_util::GetString(IDS_CLOSE));
  localized_strings.SetString(L"history",
                              l10n_util::GetString(IDS_NEW_TAB_HISTORY));
  localized_strings.SetString(L"downloads",
                              l10n_util::GetString(IDS_NEW_TAB_DOWNLOADS));
  localized_strings.SetString(L"help",
                              l10n_util::GetString(IDS_NEW_TAB_HELP));
  localized_strings.SetString(L"helpurl",
      GetUrlWithLang(GURL(kHelpContentUrl)));
  localized_strings.SetString(L"appsettings",
      l10n_util::GetString(IDS_NEW_TAB_APP_SETTINGS));
  localized_strings.SetString(L"appuninstall",
      l10n_util::GetString(IDS_NEW_TAB_APP_UNINSTALL));
  localized_strings.SetString(L"appoptions",
      l10n_util::GetString(IDS_NEW_TAB_APP_OPTIONS));
  localized_strings.SetString(L"web_store_title",
      l10n_util::GetString(IDS_EXTENSION_WEB_STORE_TITLE));
  localized_strings.SetString(L"web_store_url",
      GetUrlWithLang(GURL(Extension::ChromeStoreURL())));

  // Don't initiate the sync related message passing with the page if the sync
  // code is not present.
  if (profile_->GetProfileSyncService())
    localized_strings.SetString(L"syncispresent", "true");
  else
    localized_strings.SetString(L"syncispresent", "false");

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  // Control fade and resize animations.
  std::string anim =
      Animation::ShouldRenderRichAnimation() ? "true" : "false";
  localized_strings.SetString(L"anim", anim);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool has_3d =
      command_line->HasSwitch(switches::kEnableAcceleratedCompositing);
  localized_strings.SetString(L"has_3d", has_3d ? "true" : "false");

  // Pass the shown_sections pref early so that we can prevent flicker.
  const int shown_sections = profile_->GetPrefs()->GetInteger(
      prefs::kNTPShownSections);
  localized_strings.SetInteger(L"shown_sections", shown_sections);

  base::StringPiece new_tab_html(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_NEW_NEW_TAB_HTML));

  // Inject the template data into the HTML so that it is available before any
  // layout is needed.
  std::string json_html;
  jstemplate_builder::AppendJsonHtml(&localized_strings, &json_html);

  static const base::StringPiece template_data_placeholder(
      "<!-- template data placeholder -->");
  size_t pos = new_tab_html.find(template_data_placeholder);

  std::string full_html;
  if (pos != base::StringPiece::npos) {
    full_html.assign(new_tab_html.data(), pos);
    full_html.append(json_html);
    size_t after_offset = pos + template_data_placeholder.size();
    full_html.append(new_tab_html.data() + after_offset,
                     new_tab_html.size() - after_offset);
  } else {
    NOTREACHED();
    full_html.assign(new_tab_html.data(), new_tab_html.size());
  }

  new_tab_html_ = new RefCountedBytes;
  new_tab_html_->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), new_tab_html_->data.begin());
}

void NTPResourceCache::CreateNewTabIncognitoCSS() {
  ThemeProvider* tp = profile_->GetThemeProvider();
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND);

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

  new_tab_incognito_css_ = new RefCountedBytes;
  new_tab_incognito_css_->data.resize(full_css.size());
  std::copy(full_css.begin(), full_css.end(),
            new_tab_incognito_css_->data.begin());
}

void NTPResourceCache::CreateNewTabCSS() {
  ThemeProvider* tp = profile_->GetThemeProvider();
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND);
  SkColor color_text = tp->GetColor(BrowserThemeProvider::COLOR_NTP_TEXT);
  SkColor color_link = tp->GetColor(BrowserThemeProvider::COLOR_NTP_LINK);
  SkColor color_link_underline =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE);

  SkColor color_section =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION);
  SkColor color_section_text =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_TEXT);
  SkColor color_section_link =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_LINK);
  SkColor color_section_link_underline =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE);

  SkColor color_header =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_HEADER);
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
  // A second list of replacements, each of which must be in $$x format,
  // where x is a digit from 1-9.
  std::vector<std::string> subst2;

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

  subst2.push_back(SkColorToRGBAString(color_section));  // $$1
  subst2.push_back(SkColorToRGBAString(color_section_border));  // $$2
  subst2.push_back(SkColorToRGBAString(color_section_text));  // $$3
  subst2.push_back(SkColorToRGBAString(color_section_link));  // $$4
  subst2.push_back(
      tp->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ? "block" : "none");  // $$5
  subst2.push_back(SkColorToRGBAString(color_link_underline));  // $$6
  subst2.push_back(SkColorToRGBAString(color_section_link_underline));  // $$7

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_NEW_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  const std::string css_string = ReplaceStringPlaceholders(
      new_tab_theme_css, subst, NULL);
  std::string full_css = ReplaceStringPlaceholders(css_string, subst2, NULL);

  new_tab_css_ = new RefCountedBytes;
  new_tab_css_->data.resize(full_css.size());
  std::copy(full_css.begin(), full_css.end(),
            new_tab_css_->data.begin());
}
