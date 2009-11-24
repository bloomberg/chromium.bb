// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/dom_ui/new_tab_ui.h"

#include <set>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/histogram.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/dom_ui/most_visited_handler.h"
#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"
#include "chrome/browser/dom_ui/ntp_resource_cache.h"
#include "chrome/browser/dom_ui/shown_sections_handler.h"
#include "chrome/browser/dom_ui/tips_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

namespace {

// The number of recent bookmarks we show.
const int kRecentBookmarks = 9;

// The number of search URLs to show.
const int kSearchURLs = 3;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const wchar_t kRTLHtmlTextDirection[] = L"rtl";
const wchar_t kDefaultHtmlTextDirection[] = L"ltr";

////////////////////////////////////////////////////////////////////////////////
// PaintTimer

// To measure end-to-end performance of the new tab page, we observe paint
// messages and wait for the page to stop repainting.
class PaintTimer : public RenderWidgetHost::PaintObserver {
 public:
  PaintTimer() {
    Start();
  }

  // Start the benchmarking and the timer.
  void Start() {
    start_ = base::TimeTicks::Now();
    last_paint_ = start_;

    timer_.Start(base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
                 &PaintTimer::Timeout);
  }

  // A callback that is invoked whenever our RenderWidgetHost paints.
  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) {
    last_paint_ = base::TimeTicks::Now();
  }

  // The timer callback.  If enough time has elapsed since the last paint
  // message, we say we're done painting; otherwise, we keep waiting.
  void Timeout() {
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
                   &PaintTimer::Timeout);
    }
  }

 private:
  // The amount of time there must be no painting for us to consider painting
  // finished.  Observed times are in the ~1200ms range on Windows.
  static const int kTimeoutMs = 2000;
  // The time when we started benchmarking.
  base::TimeTicks start_;
  // The last time we got a paint notification.
  base::TimeTicks last_paint_;
  // Scoping so we can be sure our timeouts don't outlive us.
  base::OneShotTimer<PaintTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(PaintTimer);
};

///////////////////////////////////////////////////////////////////////////////
// PromotionalMessageHandler

class PromotionalMessageHandler : public DOMMessageHandler {
 public:
  PromotionalMessageHandler() {}
  virtual ~PromotionalMessageHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Zero promotional message counter.
  void HandleClosePromotionalMessage(const Value* content);

 private:
  DISALLOW_COPY_AND_ASSIGN(PromotionalMessageHandler);
};

void PromotionalMessageHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("stopPromoLineMessage",
      NewCallback(this,
                  &PromotionalMessageHandler::HandleClosePromotionalMessage));
}

void PromotionalMessageHandler::HandleClosePromotionalMessage(
    const Value* content) {
  dom_ui_->GetProfile()->GetPrefs()->SetInteger(
      prefs::kNTPPromoLineRemaining, 0);
}


///////////////////////////////////////////////////////////////////////////////
// RecentlyClosedTabsHandler

class RecentlyClosedTabsHandler : public DOMMessageHandler,
                                  public TabRestoreService::Observer {
 public:
  RecentlyClosedTabsHandler() : tab_restore_service_(NULL) {}
  virtual ~RecentlyClosedTabsHandler();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "reopenTab" message. Rewrites the history of the
  // currently displayed tab to be the one in TabRestoreService with a
  // history of a session passed in through the content pointer.
  void HandleReopenTab(const Value* content);

  // Callback for the "getRecentlyClosedTabs" message.
  void HandleGetRecentlyClosedTabs(const Value* content);

  // Observer callback for TabRestoreService::Observer. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service);

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

 private:
  // Converts a closed tab to the value sent down to the NTP. Returns true on
  // success, false if the value shouldn't be sent down.
  bool TabToValue(const TabRestoreService::Tab& tab,
                  DictionaryValue* dictionary);

  // Converts a closed window to the value sent down to the NTP. Returns true
  // on success, false if the value shouldn't be sent down.
  bool WindowToValue(const TabRestoreService::Window& window,
                     DictionaryValue* dictionary);

  // Adds tab to unique_items list if it is not present. Returns false if
  // tab was already in the list, true if it was absent.  A tab is
  // considered unique if no other tab shares both its title and its url.
  bool EnsureTabIsUnique(const DictionaryValue* tab,
                   std::set<std::wstring>& unique_items);

  // TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyClosedTabsHandler);
};

void RecentlyClosedTabsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getRecentlyClosedTabs",
      NewCallback(this,
                  &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs));
  dom_ui_->RegisterMessageCallback("reopenTab",
      NewCallback(this, &RecentlyClosedTabsHandler::HandleReopenTab));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const Value* content) {
  Browser* browser = Browser::GetBrowserForController(
      &dom_ui_->tab_contents()->controller(), NULL);
  if (!browser)
    return;

  // Extract the integer value of the tab session to restore from the
  // incoming string array. This will be greatly simplified when
  // DOMUIBindings::send() is generalized to all data types instead of
  // silently failing when passed anything other then an array of
  // strings.
  if (content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        int session_to_restore = StringToInt(WideToUTF16Hack(wstring_value));
        tab_restore_service_->RestoreEntryById(browser, session_to_restore,
                                               true);
        // The current tab has been nuked at this point; don't touch any member
        // variables.
      }
    }
  }
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const Value* content) {
  if (!tab_restore_service_) {
    tab_restore_service_ = dom_ui_->GetProfile()->GetTabRestoreService();

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
  const TabRestoreService::Entries& entries = service->entries();
  ListValue list_value;
  std::set<std::wstring> unique_items;
  int added_count = 0;
  const int max_count = 10;

  // We filter the list of recently closed to only show 'interesting' entries,
  // where an interesting entry is either a closed window or a closed tab
  // whose selected navigation is not the new tab ui.
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < max_count; ++it) {
    TabRestoreService::Entry* entry = *it;
    DictionaryValue* value = new DictionaryValue();
    if ((entry->type == TabRestoreService::TAB &&
         TabToValue(*static_cast<TabRestoreService::Tab*>(entry), value) &&
         EnsureTabIsUnique(value, unique_items)) ||
        (entry->type == TabRestoreService::WINDOW &&
         WindowToValue(*static_cast<TabRestoreService::Window*>(entry),
                       value))) {
      value->SetInteger(L"sessionId", entry->id);
      list_value.Append(value);
      added_count++;
    } else {
      delete value;
    }
  }
  dom_ui_->CallJavascriptFunction(L"recentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

bool RecentlyClosedTabsHandler::TabToValue(
    const TabRestoreService::Tab& tab,
    DictionaryValue* dictionary) {
  if (tab.navigations.empty())
    return false;

  const TabNavigation& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  if (current_navigation.url() == GURL(chrome::kChromeUINewTabURL))
    return false;

  NewTabUI::SetURLTitleAndDirection(dictionary, current_navigation.title(),
                                    current_navigation.url());
  dictionary->SetString(L"type", L"tab");
  dictionary->SetReal(L"timestamp", tab.timestamp.ToDoubleT());
  return true;
}

bool RecentlyClosedTabsHandler::WindowToValue(
    const TabRestoreService::Window& window,
    DictionaryValue* dictionary) {
  if (window.tabs.empty()) {
    NOTREACHED();
    return false;
  }

  ListValue* tab_values = new ListValue();
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    DictionaryValue* tab_value = new DictionaryValue();
    if (TabToValue(window.tabs[i], tab_value))
      tab_values->Append(tab_value);
    else
      delete tab_value;
  }
  if (tab_values->GetSize() == 0) {
    delete tab_values;
    return false;
  }

  dictionary->SetString(L"type", L"window");
  dictionary->SetReal(L"timestamp", window.timestamp.ToDoubleT());
  dictionary->Set(L"tabs", tab_values);
  return true;
}

bool RecentlyClosedTabsHandler::EnsureTabIsUnique(const DictionaryValue* tab,
    std::set<std::wstring>& unique_items) {
  std::wstring title;
  std::wstring url;
  if (tab->GetString(L"title", &title) &&
      tab->GetString(L"url", &url)) {
    std::wstring unique_key = title + url;
    if (unique_items.find(unique_key) != unique_items.end())
      return false;
    else
      unique_items.insert(unique_key);
  }
  return true;
}


///////////////////////////////////////////////////////////////////////////////
// MetricsHandler

// Let the page contents record UMA actions. Only use when you can't do it from
// C++. For example, we currently use it to let the NTP log the postion of the
// Most Visited or Bookmark the user clicked on, as we don't get that
// information through RequestOpenURL. You will need to update the metrics
// dashboard with the action names you use, as our processor won't catch that
// information (treat it as RecordComputedMetrics)
class MetricsHandler : public DOMMessageHandler {
 public:
  MetricsHandler() {}
  virtual ~MetricsHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback which records a user action.
  void HandleMetrics(const Value* content);

  // Callback for the "logEventTime" message.
  void HandleLogEventTime(const Value* content);

 private:

  DISALLOW_COPY_AND_ASSIGN(MetricsHandler);
};

void MetricsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("metrics",
      NewCallback(this, &MetricsHandler::HandleMetrics));

  dom_ui_->RegisterMessageCallback("logEventTime",
      NewCallback(this, &MetricsHandler::HandleLogEventTime));
}

void MetricsHandler::HandleMetrics(const Value* content) {
  if (content && content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        UserMetrics::RecordComputedAction(WideToASCII(wstring_value),
                                          dom_ui_->GetProfile());
      }
    }
  }
}

void MetricsHandler::HandleLogEventTime(const Value* content) {
  if (content && content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      std::string event_name;
      if (list_member->GetAsString(&event_name)) {
        dom_ui_->tab_contents()->LogNewTabTime(event_name);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// NewTabPageSetHomePageHandler

// Sets the new tab page as home page when user clicks on "make this my home
// page" link.
class NewTabPageSetHomePageHandler : public DOMMessageHandler {
 public:
  NewTabPageSetHomePageHandler() {}
  virtual ~NewTabPageSetHomePageHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for "setHomePage".
  void HandleSetHomePage(const Value* value);

 private:

  DISALLOW_COPY_AND_ASSIGN(NewTabPageSetHomePageHandler);
};

void NewTabPageSetHomePageHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("setHomePage", NewCallback(
      this, &NewTabPageSetHomePageHandler::HandleSetHomePage));
}

void NewTabPageSetHomePageHandler::HandleSetHomePage(
    const Value* value) {
  dom_ui_->GetProfile()->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                                true);
  ListValue list_value;
  list_value.Append(new StringValue(
      l10n_util::GetString(IDS_NEW_TAB_HOME_PAGE_SET_NOTIFICATION)));
  list_value.Append(new StringValue(
      l10n_util::GetString(IDS_NEW_TAB_HOME_PAGE_HIDE_NOTIFICATION)));
  dom_ui_->CallJavascriptFunction(L"onHomePageSet", list_value);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(TabContents* contents)
    : DOMUI(contents) {
  // Override some options on the DOM UI.
  hide_favicon_ = true;
  force_bookmark_bar_visible_ = true;
  force_extension_shelf_visible_ = true;
  focus_location_bar_by_default_ = true;
  should_hide_url_ = true;
  overridden_title_ = WideToUTF16Hack(l10n_util::GetString(IDS_NEW_TAB_TITLE));

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  link_transition_type_ = PageTransition::AUTO_BOOKMARK;

  if (NewTabUI::FirstRunDisabled())
    NewTabHTMLSource::set_first_run(false);

  if (!GetProfile()->IsOffTheRecord()) {
    AddMessageHandler((new ShownSectionsHandler())->Attach(this));
    AddMessageHandler((new MostVisitedHandler())->Attach(this));
    AddMessageHandler((new RecentlyClosedTabsHandler())->Attach(this));
    AddMessageHandler((new MetricsHandler())->Attach(this));
    if (WebResourcesEnabled())
      AddMessageHandler((new TipsHandler())->Attach(this));
    if (ProfileSyncService::IsSyncEnabled()) {
      AddMessageHandler((new NewTabPageSyncHandler())->Attach(this));
    }

    AddMessageHandler((new NewTabPageSetHomePageHandler())->Attach(this));
    AddMessageHandler((new PromotionalMessageHandler())->Attach(this));
  }

  // Initializing the CSS and HTML can require some CPU, so do it after
  // we've hooked up the most visited handler.  This allows the DB query
  // for the new tab thumbs to happen earlier.
  InitializeCSSCaches();
  NewTabHTMLSource* html_source =
      new NewTabHTMLSource(GetProfile());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));

  // Listen for theme installation.
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  // Listen for bookmark bar visibility changes.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
}

NewTabUI::~NewTabUI() {
}

void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->set_paint_observer(new PaintTimer);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  render_view_host->set_paint_observer(new PaintTimer);
}

void NewTabUI::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (NotificationType::BROWSER_THEME_CHANGED == type) {
    InitializeCSSCaches();
    CallJavascriptFunction(L"themeChanged");
  } else if (NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED) {
    if (GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
      CallJavascriptFunction(L"bookmarkBarAttached");
    else
      CallJavascriptFunction(L"bookmarkBarDetached");
  }
}

void NewTabUI::InitializeCSSCaches() {
  DOMUIThemeSource* theme = new DOMUIThemeSource(GetProfile());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(theme)));
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
  std::wstring wstring_url = UTF8ToWide(gurl.spec());
  dictionary->SetString(L"url", wstring_url);

  std::wstring wstring_title = UTF16ToWide(title);

  bool using_url_as_the_title = false;
  std::wstring title_to_set(wstring_title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = wstring_url;
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
  std::wstring direction = kDefaultHtmlTextDirection;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    if (using_url_as_the_title) {
      l10n_util::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      if (l10n_util::StringContainsStrongRTLChars(wstring_title)) {
        l10n_util::WrapStringWithRTLFormatting(&title_to_set);
        direction = kRTLHtmlTextDirection;
      } else {
        l10n_util::WrapStringWithLTRFormatting(&title_to_set);
      }
    }
  }
  dictionary->SetString(L"title", title_to_set);
  dictionary->SetString(L"direction", direction);
}

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

bool NewTabUI::NewTabHTMLSource::first_run_ = true;

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUINewTabHost, NULL) {
  static bool first_view = true;
  if (first_view) {
    // Decrement ntp promo counters; the default values are specified in
    // Browser::RegisterUserPrefs.
    profile->GetPrefs()->SetInteger(prefs::kNTPPromoLineRemaining,
        profile->GetPrefs()->GetInteger(prefs::kNTPPromoLineRemaining) - 1);
    profile->GetPrefs()->SetInteger(prefs::kNTPPromoImageRemaining,
        profile->GetPrefs()->GetInteger(prefs::kNTPPromoImageRemaining) - 1);
    first_view = false;
  }

  html_bytes_ = profile->GetNTPResourceCache()->GetNewTabHTML(
      profile->IsOffTheRecord());
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record, int request_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!path.empty()) {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED();
    return;
  }

  SendResponse(request_id, html_bytes_);
}
