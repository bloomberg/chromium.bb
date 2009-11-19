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
#include "base/values.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
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

}

NTPResourceCache::NTPResourceCache(Profile* profile) : profile_(profile) {
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());

  // Watch for pref changes that cause us to need to invalidate the cache.
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->AddPrefObserver(prefs::kShowBookmarkBar, this);
  pref_service->AddPrefObserver(prefs::kHomePageIsNewTabPage, this);
  pref_service->AddPrefObserver(prefs::kNTPShownSections, this);
}

NTPResourceCache::~NTPResourceCache() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->RemovePrefObserver(prefs::kShowBookmarkBar, this);
  pref_service->RemovePrefObserver(prefs::kHomePageIsNewTabPage, this);
  pref_service->RemovePrefObserver(prefs::kNTPShownSections, this);
}

RefCountedBytes* NTPResourceCache::GetNewTabHTML(bool is_off_the_record) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (is_off_the_record) {
    if (!new_tab_incognito_html_.get())
      CreateNewTabIncognitoHtml();
  } else {
    if (!new_tab_html_.get())
      CreateNewTabHtml();
  }
  return is_off_the_record ? new_tab_incognito_html_.get()
                           : new_tab_html_.get();
}

void NTPResourceCache::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  new_tab_incognito_html_ = NULL;
  new_tab_html_ = NULL;
}

void NTPResourceCache::CreateNewTabIncognitoHtml() {
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

void NTPResourceCache::CreateNewTabHtml() {
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
  localized_strings.SetString(L"promomessage",
      l10n_util::GetStringF(IDS_NTP_PROMOTION_MESSAGE,
          l10n_util::GetString(IDS_PRODUCT_NAME),
          ASCIIToWide(Extension::kGalleryBrowseUrl),
          GetUrlWithLang(kSyncServiceHelpUrl)));
  localized_strings.SetString(L"extensionslink",
      ASCIIToWide(Extension::kGalleryBrowseUrl));

  // Don't initiate the sync related message passing with the page if the sync
  // code is not present.
  if (profile_->GetProfileSyncService())
    localized_strings.SetString(L"syncispresent", "true");
  else
    localized_strings.SetString(L"syncispresent", "false");

  if (!profile_->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    localized_strings.SetString(L"showsetashomepage", "true");

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
