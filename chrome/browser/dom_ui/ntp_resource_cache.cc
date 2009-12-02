// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/ntp_resource_cache.h"

#include "app/animation.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/bookmark_bar_view.h"
#elif defined(OS_LINUX)
#include "chrome/browser/gtk/bookmark_bar_gtk.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/bookmark_bar_constants.h"
#endif

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char* const kLearnMoreIncognitoUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=95464";

// The URL for bookmark sync service help.
const char* const kSyncServiceHelpUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=165139";

std::wstring GetUrlWithLang(const char* const url) {
  return ASCIIToWide(google_util::AppendGoogleLocaleParam(GURL(url)).spec());
}

// In case a file path to the new tab page was provided this tries to load
// the file and returns the file content if successful. This returns an
// empty string in case of failure.
std::string GetCustomNewTabPageFromCommandLine() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath file_path = command_line->GetSwitchValuePath(
      switches::kNewTabPage);

  if (!file_path.empty()) {
    // Read the file contents in, blocking the UI thread of the browser.
    // This is for testing purposes only! It is used to test new versions of
    // the new tab page using a special command line option. Never use this
    // in a way that will be used by default in product.
    std::string file_contents;
    if (file_util::ReadFileToString(FilePath(file_path), &file_contents))
      return file_contents;
  }

  return std::string();
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
#elif defined(OS_LINUX)
  int offset = BookmarkBarGtk::kBookmarkBarNTPHeight;
#elif defined(OS_MACOSX)
  int offset = bookmarks::kNTPBookmarkBarHeight;
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

  // Watch for pref changes that cause us to need to invalidate the CSS cache.
  pref_service->AddPrefObserver(prefs::kNTPPromoLineRemaining, this);
}

NTPResourceCache::~NTPResourceCache() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->RemovePrefObserver(prefs::kShowBookmarkBar, this);
  pref_service->RemovePrefObserver(prefs::kNTPShownSections, this);

  pref_service->RemovePrefObserver(prefs::kNTPPromoLineRemaining, this);
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
    } else if (*pref_name == prefs::kNTPPromoLineRemaining) {
      new_tab_incognito_css_ = NULL;
      new_tab_css_ = NULL;
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
                            GetUrlWithLang(kLearnMoreIncognitoUrl)));
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
  std::wstring title;
  std::wstring most_visited;
  if (UserDataManager::Get()->is_current_profile_default()) {
    title = l10n_util::GetString(IDS_NEW_TAB_TITLE);
    most_visited = l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED);
  } else {
    // Get the current profile name.
    std::wstring profile_name =
      UserDataManager::Get()->current_profile_name();
    title = l10n_util::GetStringF(IDS_NEW_TAB_TITLE_WITH_PROFILE_NAME,
                                  profile_name);
    most_visited = l10n_util::GetStringF(
        IDS_NEW_TAB_MOST_VISITED_WITH_PROFILE_NAME,
        profile_name);
  }
  DictionaryValue localized_strings;
  localized_strings.SetString(L"bookmarkbarattached",
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      "true" : "false");
  localized_strings.SetString(L"hasattribution",
      profile_->GetThemeProvider()->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ?
      "true" : "false");
  localized_strings.SetString(L"title", title);
  localized_strings.SetString(L"mostvisited", most_visited);
  localized_strings.SetString(L"searches",
      l10n_util::GetString(IDS_NEW_TAB_SEARCHES));
  localized_strings.SetString(L"bookmarks",
      l10n_util::GetString(IDS_NEW_TAB_BOOKMARKS));
  localized_strings.SetString(L"recent",
      l10n_util::GetString(IDS_NEW_TAB_RECENT));
  localized_strings.SetString(L"showhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SHOW));
  localized_strings.SetString(L"showhistoryurl",
      chrome::kChromeUIHistoryURL);
  localized_strings.SetString(L"editthumbnails",
      l10n_util::GetString(IDS_NEW_TAB_REMOVE_THUMBNAILS));
  localized_strings.SetString(L"restorethumbnails",
      l10n_util::GetString(IDS_NEW_TAB_RESTORE_THUMBNAILS_LINK));
  localized_strings.SetString(L"editmodeheading",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_EDIT_MODE_HEADING));
  localized_strings.SetString(L"doneediting",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_DONE_REMOVING_BUTTON));
  localized_strings.SetString(L"cancelediting",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_CANCEL_REMOVING_BUTTON));
  localized_strings.SetString(L"searchhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SEARCH));
  localized_strings.SetString(L"recentlyclosed",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED));
  localized_strings.SetString(L"mostvisitedintro",
      l10n_util::GetStringF(IDS_NEW_TAB_MOST_VISITED_INTRO,
          l10n_util::GetString(IDS_WELCOME_PAGE_URL)));
  localized_strings.SetString(L"closedwindowsingle",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE));
  localized_strings.SetString(L"closedwindowmultiple",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE));
  localized_strings.SetString(L"attributionintro",
      l10n_util::GetString(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  localized_strings.SetString(L"viewfullhistory",
      l10n_util::GetString(IDS_NEW_TAB_VIEW_FULL_HISTORY));
  localized_strings.SetString(L"showthumbnails",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_THUMBNAILS));
  localized_strings.SetString(L"hidethumbnails",
      l10n_util::GetString(IDS_NEW_TAB_HIDE_THUMBNAILS));
  localized_strings.SetString(L"showlist",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_LIST));
  localized_strings.SetString(L"hidelist",
      l10n_util::GetString(IDS_NEW_TAB_HIDE_LIST));
  localized_strings.SetString(L"showrecentlyclosedtabs",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_RECENTLY_CLOSED_TABS));
  localized_strings.SetString(L"hiderecentlyclosedtabs",
      l10n_util::GetString(IDS_NEW_TAB_HIDE_RECENTLY_CLOSED_TABS));
  localized_strings.SetString(L"thumbnailremovednotification",
      l10n_util::GetString(IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION));
  localized_strings.SetString(L"undothumbnailremove",
      l10n_util::GetString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE));
  localized_strings.SetString(L"otrmessage",
      l10n_util::GetString(IDS_NEW_TAB_OTR_MESSAGE));
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
  localized_strings.SetString(L"makethishomepage",
      l10n_util::GetString(IDS_NEW_TAB_MAKE_THIS_HOMEPAGE));
  localized_strings.SetString(L"themelink",
      l10n_util::GetString(IDS_THEMES_GALLERY_URL));
  localized_strings.SetString(L"tips",
      l10n_util::GetString(IDS_NEW_TAB_TIPS));
  localized_strings.SetString(L"sync",
      l10n_util::GetString(IDS_NEW_TAB_SHOW_HIDE_BOOKMARK_SYNC));
  localized_strings.SetString(L"promonew",
      l10n_util::GetString(IDS_NTP_PROMOTION_NEW));
  std::wstring extensionLink = ASCIIToWide(
      google_util::AppendGoogleLocaleParam(
          GURL(extension_urls::kGalleryBrowsePrefix)).spec());
  localized_strings.SetString(L"promomessage",
      l10n_util::GetStringF(IDS_NTP_PROMO_MESSAGE,
          l10n_util::GetString(IDS_PRODUCT_NAME), extensionLink));
  localized_strings.SetString(L"extensionslink", extensionLink);

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

  // Pass the shown_sections pref early so that we can prevent flicker.
  const int shown_sections = profile_->GetPrefs()->GetInteger(
      prefs::kNTPShownSections);
  localized_strings.SetInteger(L"shown_sections", shown_sections);

  // In case we have the new new tab page enabled we first try to read the file
  // provided on the command line. If that fails we just get the resource from
  // the resource bundle.
  base::StringPiece new_tab_html;
  std::string new_tab_html_str;
  new_tab_html_str = GetCustomNewTabPageFromCommandLine();

  if (!new_tab_html_str.empty()) {
    new_tab_html = base::StringPiece(new_tab_html_str);
  }

  if (new_tab_html.empty()) {
    new_tab_html = ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_NEW_NEW_TAB_HTML);
  }

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
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);

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
  subst.push_back(WideToUTF8(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID)));  // $1

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
  subst.push_back(WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID)));  // $1

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

#if defined(OS_MACOSX)
  // No extensions available on Mac yet.
  subst2.push_back("none");  // $$8: display of lower right promo image
  subst2.push_back("none");  // $$9: display of butterbar footer promo line
#else
  if (profile_->GetPrefs()->GetInteger(prefs::kNTPPromoImageRemaining) > 0) {
    subst2.push_back("block");  // $$8
  } else {
    subst2.push_back("none");  // $$8
  }
  if (profile_->GetPrefs()->GetInteger(prefs::kNTPPromoLineRemaining) > 0) {
    subst2.push_back("inline-block");  // $$9
  } else {
    subst2.push_back("none");  // $$9
  }
#endif

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
