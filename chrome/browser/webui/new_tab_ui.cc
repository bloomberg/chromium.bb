// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/webui/new_tab_ui.h"

#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webui/app_launcher_handler.h"
#include "chrome/browser/webui/foreign_session_handler.h"
#include "chrome/browser/webui/most_visited_handler.h"
#include "chrome/browser/webui/new_tab_page_sync_handler.h"
#include "chrome/browser/webui/ntp_login_handler.h"
#include "chrome/browser/webui/ntp_resource_cache.h"
#include "chrome/browser/webui/shown_sections_handler.h"
#include "chrome/browser/webui/theme_source.h"
#include "chrome/browser/webui/tips_handler.h"
#include "chrome/browser/webui/value_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The number of recent bookmarks we show.
const int kRecentBookmarks = 9;

// The number of search URLs to show.
const int kSearchURLs = 3;

// The amount of time there must be no painting for us to consider painting
// finished.  Observed times are in the ~1200ms range on Windows.
const int kTimeoutMs = 2000;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kDefaultHtmlTextDirection[] = "ltr";

///////////////////////////////////////////////////////////////////////////////
// RecentlyClosedTabsHandler

class RecentlyClosedTabsHandler : public WebUIMessageHandler,
                                  public TabRestoreServiceObserver {
 public:
  RecentlyClosedTabsHandler() : tab_restore_service_(NULL) {}
  virtual ~RecentlyClosedTabsHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "reopenTab" message. Rewrites the history of the
  // currently displayed tab to be the one in TabRestoreService with a
  // history of a session passed in through the content pointer.
  void HandleReopenTab(const ListValue* args);

  // Callback for the "getRecentlyClosedTabs" message.
  void HandleGetRecentlyClosedTabs(const ListValue* args);

  // Observer callback for TabRestoreServiceObserver. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service);

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

 private:
  // TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyClosedTabsHandler);
};

void RecentlyClosedTabsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getRecentlyClosedTabs",
      NewCallback(this,
                  &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs));
  web_ui_->RegisterMessageCallback("reopenTab",
      NewCallback(this, &RecentlyClosedTabsHandler::HandleReopenTab));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const ListValue* args) {
  Browser* browser = Browser::GetBrowserForController(
      &web_ui_->tab_contents()->controller(), NULL);
  if (!browser)
    return;

  int session_to_restore;
  if (ExtractIntegerValue(args, &session_to_restore))
    tab_restore_service_->RestoreEntryById(browser, session_to_restore, true);
  // The current tab has been nuked at this point; don't touch any member
  // variables.
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const ListValue* args) {
  if (!tab_restore_service_) {
    tab_restore_service_ = web_ui_->GetProfile()->GetTabRestoreService();

    // GetTabRestoreService() can return NULL (i.e., when in Off the
    // Record mode)
    if (tab_restore_service_) {
      // This does nothing if the tabs have already been loaded or they
      // shouldn't be loaded.
      tab_restore_service_->LoadTabsFromLastSession();

      tab_restore_service_->AddObserver(this);
    }
  }

  if (tab_restore_service_)
    TabRestoreServiceChanged(tab_restore_service_);
}

void RecentlyClosedTabsHandler::TabRestoreServiceChanged(
    TabRestoreService* service) {
  ListValue list_value;
  NewTabUI::AddRecentlyClosedEntries(service->entries(), &list_value);

  web_ui_->CallJavascriptFunction(L"recentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// MetricsHandler

// Let the page contents record UMA actions. Only use when you can't do it from
// C++. For example, we currently use it to let the NTP log the postion of the
// Most Visited or Bookmark the user clicked on, as we don't get that
// information through RequestOpenURL. You will need to update the metrics
// dashboard with the action names you use, as our processor won't catch that
// information (treat it as RecordComputedMetrics)
class MetricsHandler : public WebUIMessageHandler {
 public:
  MetricsHandler() {}
  virtual ~MetricsHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback which records a user action.
  void HandleMetrics(const ListValue* args);

  // Callback for the "logEventTime" message.
  void HandleLogEventTime(const ListValue* args);

 private:

  DISALLOW_COPY_AND_ASSIGN(MetricsHandler);
};

void MetricsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("metrics",
      NewCallback(this, &MetricsHandler::HandleMetrics));

  web_ui_->RegisterMessageCallback("logEventTime",
      NewCallback(this, &MetricsHandler::HandleLogEventTime));
}

void MetricsHandler::HandleMetrics(const ListValue* args) {
  std::string string_action = WideToUTF8(ExtractStringValue(args));
  UserMetrics::RecordComputedAction(string_action, web_ui_->GetProfile());
}

void MetricsHandler::HandleLogEventTime(const ListValue* args) {
  std::string event_name = WideToUTF8(ExtractStringValue(args));
  web_ui_->tab_contents()->LogNewTabTime(event_name);
}

///////////////////////////////////////////////////////////////////////////////
// NewTabPageSetHomePageHandler

// Sets the new tab page as home page when user clicks on "make this my home
// page" link.
class NewTabPageSetHomePageHandler : public WebUIMessageHandler {
 public:
  NewTabPageSetHomePageHandler() {}
  virtual ~NewTabPageSetHomePageHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for "setHomePage".
  void HandleSetHomePage(const ListValue* args);

 private:

  DISALLOW_COPY_AND_ASSIGN(NewTabPageSetHomePageHandler);
};

void NewTabPageSetHomePageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("setHomePage", NewCallback(
      this, &NewTabPageSetHomePageHandler::HandleSetHomePage));
}

void NewTabPageSetHomePageHandler::HandleSetHomePage(
    const ListValue* args) {
  web_ui_->GetProfile()->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                                true);
  ListValue list_value;
  list_value.Append(new StringValue(
      l10n_util::GetStringUTF16(IDS_NEW_TAB_HOME_PAGE_SET_NOTIFICATION)));
  list_value.Append(new StringValue(
      l10n_util::GetStringUTF16(IDS_NEW_TAB_HOME_PAGE_HIDE_NOTIFICATION)));
  web_ui_->CallJavascriptFunction(L"onHomePageSet", list_value);
}

///////////////////////////////////////////////////////////////////////////////
// NewTabPageClosePromoHandler

// Turns off the promo line permanently when it has been explicitly closed by
// the user.
class NewTabPageClosePromoHandler : public WebUIMessageHandler {
 public:
  NewTabPageClosePromoHandler() {}
  virtual ~NewTabPageClosePromoHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for "closePromo".
  void HandleClosePromo(const ListValue* args);

 private:

  DISALLOW_COPY_AND_ASSIGN(NewTabPageClosePromoHandler);
};

void NewTabPageClosePromoHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("closePromo", NewCallback(
      this, &NewTabPageClosePromoHandler::HandleClosePromo));
}

void NewTabPageClosePromoHandler::HandleClosePromo(
    const ListValue* args) {
  web_ui_->GetProfile()->GetPrefs()->SetBoolean(prefs::kNTPPromoClosed, true);
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::PROMO_RESOURCE_STATE_CHANGED,
                  Source<NewTabPageClosePromoHandler>(this),
                  NotificationService::NoDetails());
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(TabContents* contents)
    : WebUI(contents) {
  // Override some options on the Web UI.
  hide_favicon_ = true;
  force_bookmark_bar_visible_ = true;
  focus_location_bar_by_default_ = true;
  should_hide_url_ = true;
  overridden_title_ = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  link_transition_type_ = PageTransition::AUTO_BOOKMARK;

  if (NewTabUI::FirstRunDisabled())
    NewTabHTMLSource::set_first_run(false);

  static bool first_view = true;
  if (first_view) {
    first_view = false;
  }

  if (!GetProfile()->IsOffTheRecord()) {
    PrefService* pref_service = GetProfile()->GetPrefs();
    AddMessageHandler((new NTPLoginHandler())->Attach(this));
    AddMessageHandler((new ShownSectionsHandler(pref_service))->Attach(this));
    AddMessageHandler((new browser_sync::ForeignSessionHandler())->
      Attach(this));
    AddMessageHandler((new MostVisitedHandler())->Attach(this));
    AddMessageHandler((new RecentlyClosedTabsHandler())->Attach(this));
    AddMessageHandler((new MetricsHandler())->Attach(this));
    if (GetProfile()->IsSyncAccessible())
      AddMessageHandler((new NewTabPageSyncHandler())->Attach(this));
    ExtensionService* service = GetProfile()->GetExtensionService();
    // We might not have an ExtensionService (on ChromeOS when not logged in
    // for example).
    if (service)
      AddMessageHandler((new AppLauncherHandler(service))->Attach(this));

    AddMessageHandler((new NewTabPageSetHomePageHandler())->Attach(this));
    AddMessageHandler((new NewTabPageClosePromoHandler())->Attach(this));
  }

  // Initializing the CSS and HTML can require some CPU, so do it after
  // we've hooked up the most visited handler.  This allows the DB query
  // for the new tab thumbs to happen earlier.
  InitializeCSSCaches();
  NewTabHTMLSource* html_source =
      new NewTabHTMLSource(GetProfile()->GetOriginalProfile());
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);

  // Listen for theme installation.
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  // Listen for bookmark bar visibility changes.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
}

NewTabUI::~NewTabUI() {
}

// The timer callback.  If enough time has elapsed since the last paint
// message, we say we're done painting; otherwise, we keep waiting.
void NewTabUI::PaintTimeout() {
  // The amount of time there must be no painting for us to consider painting
  // finished.  Observed times are in the ~1200ms range on Windows.
  base::TimeTicks now = base::TimeTicks::Now();
  if ((now - last_paint_) >= base::TimeDelta::FromMilliseconds(kTimeoutMs)) {
    // Painting has quieted down.  Log this as the full time to run.
    base::TimeDelta load_time = last_paint_ - start_;
    int load_time_ms = static_cast<int>(load_time.InMilliseconds());
    NotificationService::current()->Notify(
        NotificationType::INITIAL_NEW_TAB_UI_LOAD,
        NotificationService::AllSources(),
        Details<int>(&load_time_ms));
    UMA_HISTOGRAM_TIMES("NewTabUI load", load_time);
  } else {
    // Not enough quiet time has elapsed.
    // Some more paints must've occurred since we set the timeout.
    // Wait some more.
    timer_.Start(base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
                 &NewTabUI::PaintTimeout);
  }
}

void NewTabUI::StartTimingPaint(RenderViewHost* render_view_host) {
  start_ = base::TimeTicks::Now();
  last_paint_ = start_;
  registrar_.Add(this, NotificationType::RENDER_WIDGET_HOST_DID_PAINT,
      Source<RenderWidgetHost>(render_view_host));
  timer_.Start(base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
               &NewTabUI::PaintTimeout);

}
void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BROWSER_THEME_CHANGED: {
      InitializeCSSCaches();
      ListValue args;
      args.Append(Value::CreateStringValue(
          GetProfile()->GetThemeProvider()->HasCustomImage(
              IDR_THEME_NTP_ATTRIBUTION) ?
          "true" : "false"));
      CallJavascriptFunction(L"themeChanged", args);
      break;
    }
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED: {
      if (GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
        CallJavascriptFunction(L"bookmarkBarAttached");
      else
        CallJavascriptFunction(L"bookmarkBarDetached");
      break;
    }
    case NotificationType::RENDER_WIDGET_HOST_DID_PAINT: {
      last_paint_ = base::TimeTicks::Now();
      break;
    }
    default:
      CHECK(false) << "Unexpected notification: " << type.value;
  }
}

void NewTabUI::InitializeCSSCaches() {
  Profile* profile = GetProfile();
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);
}

// static
void NewTabUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kNTPPrefVersion, 0);

  MostVisitedHandler::RegisterUserPrefs(prefs);
  ShownSectionsHandler::RegisterUserPrefs(prefs);
  if (NewTabUI::WebResourcesEnabled())
    TipsHandler::RegisterUserPrefs(prefs);

  UpdateUserPrefsVersion(prefs);
}

// static
bool NewTabUI::UpdateUserPrefsVersion(PrefService* prefs) {
  const int old_pref_version = prefs->GetInteger(prefs::kNTPPrefVersion);
  if (old_pref_version != current_pref_version()) {
    MigrateUserPrefs(prefs, old_pref_version, current_pref_version());
    prefs->SetInteger(prefs::kNTPPrefVersion, current_pref_version());
    return true;
  }
  return false;
}

// static
void NewTabUI::MigrateUserPrefs(PrefService* prefs, int old_pref_version,
                                int new_pref_version) {
  ShownSectionsHandler::MigrateUserPrefs(prefs, old_pref_version,
                                         current_pref_version());
}

// static
bool NewTabUI::WebResourcesEnabled() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableWebResources);
}

// static
bool NewTabUI::FirstRunDisabled() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kDisableNewTabFirstRun);
}

// static
void NewTabUI::SetURLTitleAndDirection(DictionaryValue* dictionary,
                                       const string16& title,
                                       const GURL& gurl) {
  dictionary->SetString("url", gurl.spec());

  bool using_url_as_the_title = false;
  string16 title_to_set(title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = UTF8ToUTF16(gurl.spec());
  }

  // We set the "dir" attribute of the title, so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For example,
  // the title of http://msdn.microsoft.com/en-us/default.aspx is "MSDN:
  // Microsoft developer network". In RTL locales, in the [New Tab] page, if
  // the "dir" of this title is not specified, it takes Chrome UI's
  // directionality. So the title will be truncated as "soft developer
  // network". Setting the "dir" attribute as "ltr" renders the truncated title
  // as "MSDN: Microsoft D...". As another example, the title of
  // http://yahoo.com is "Yahoo!". In RTL locales, in the [New Tab] page, the
  // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
  // "ltr".
  //
  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings. Simply setting the title's "dir" attribute works
  // fine for rendering and truncating the title. However, it does not work for
  // entire title within a tooltip when the mouse is over the title link.. For
  // example, without LRE-PDF pair, the title "Yahoo!" will be rendered as
  // "!Yahoo" within the tooltip when the mouse is over the title link.
  std::string direction = kDefaultHtmlTextDirection;
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title) {
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      if (base::i18n::StringContainsStrongRTLChars(title)) {
        base::i18n::WrapStringWithRTLFormatting(&title_to_set);
        direction = kRTLHtmlTextDirection;
      } else {
        base::i18n::WrapStringWithLTRFormatting(&title_to_set);
      }
    }
  }
  dictionary->SetString("title", title_to_set);
  dictionary->SetString("direction", direction);
}

namespace {

bool IsTabUnique(const DictionaryValue* tab,
                 std::set<std::string>* unique_items) {
  DCHECK(unique_items);
  std::string title;
  std::string url;
  if (tab->GetString("title", &title) &&
      tab->GetString("url", &url)) {
    // TODO(viettrungluu): this isn't obviously reliable, since different
    // combinations of titles/urls may conceivably yield the same string.
    std::string unique_key = title + url;
    if (unique_items->find(unique_key) != unique_items->end())
      return false;
    else
      unique_items->insert(unique_key);
  }
  return true;
}

}  // namespace

// static
void NewTabUI::AddRecentlyClosedEntries(
    const TabRestoreService::Entries& entries, ListValue* entry_list_value) {
  const int max_count = 10;
  int added_count = 0;
  std::set<std::string> unique_items;
  // We filter the list of recently closed to only show 'interesting' entries,
  // where an interesting entry is either a closed window or a closed tab
  // whose selected navigation is not the new tab ui.
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < max_count; ++it) {
    TabRestoreService::Entry* entry = *it;
    scoped_ptr<DictionaryValue> entry_dict(new DictionaryValue());
    if ((entry->type == TabRestoreService::TAB &&
         ValueHelper::TabToValue(
             *static_cast<TabRestoreService::Tab*>(entry),
             entry_dict.get()) &&
         IsTabUnique(entry_dict.get(), &unique_items)) ||
        (entry->type == TabRestoreService::WINDOW &&
         ValueHelper::WindowToValue(
             *static_cast<TabRestoreService::Window*>(entry),
             entry_dict.get()))) {
      entry_dict->SetInteger("sessionId", entry->id);
      entry_list_value->Append(entry_dict.release());
      added_count++;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

bool NewTabUI::NewTabHTMLSource::first_run_ = true;

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()),
      profile_(profile) {
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(const std::string& path,
                                                  bool is_off_the_record,
                                                  int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (AppLauncherHandler::HandlePing(profile_, path)) {
    return;
  } else if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED();
    return;
  }

  scoped_refptr<RefCountedBytes> html_bytes(
      profile_->GetNTPResourceCache()->GetNewTabHTML(is_off_the_record));

  SendResponse(request_id, html_bytes);
}

std::string NewTabUI::NewTabHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

bool NewTabUI::NewTabHTMLSource::ShouldReplaceExistingSource() const {
  return false;
}
