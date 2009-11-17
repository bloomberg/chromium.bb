// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/dom_ui/new_tab_ui.h"

#include <set>

#include "app/animation.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/dom_ui/most_visited_handler.h"
#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"
#include "chrome/browser/dom_ui/shown_sections_handler.h"
#include "chrome/browser/dom_ui/tips_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

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
// IncognitoTabHTMLSource

class IncognitoTabHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // Creates our datasource and registers initial state of the bookmark bar.
  explicit IncognitoTabHTMLSource(bool bookmark_bar_attached);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

  virtual MessageLoop* MessageLoopForRequestPath(const std::string& path)
      const {
    // IncognitoTabHTMLSource does all of the operations that need to be on
    // the UI thread from InitFullHTML, called by the constructor.  It is safe
    // to call StartDataRequest from any thread, so return NULL.
    return NULL;
  }

 private:
  ~IncognitoTabHTMLSource() {}

  // Populate full_html_.  This must be called from the UI thread because it
  // involves profile access.
  //
  // A new IncognitoTabHTMLSource object is used for each incognito tab page
  // instance and each reload of an existing incognito tab page, so there is
  // no concern about cached data becoming stale.
  void InitFullHTML();

  // The content to be served by StartDataRequest, stored by InitFullHTML.
  std::string full_html_;

  bool bookmark_bar_attached_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoTabHTMLSource);
};

IncognitoTabHTMLSource::IncognitoTabHTMLSource(bool bookmark_bar_attached)
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()),
      bookmark_bar_attached_(bookmark_bar_attached) {
  InitFullHTML();
}

void IncognitoTabHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record, int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html_.size());
  std::copy(full_html_.begin(), full_html_.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

void IncognitoTabHTMLSource::InitFullHTML() {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"content",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_MESSAGE,
          l10n_util::GetString(IDS_LEARN_MORE_INCOGNITO_URL)));
  localized_strings.SetString(L"bookmarkbarattached",
      bookmark_bar_attached_ ? "true" : "false");

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_INCOGNITO_TAB_HTML));

  full_html_ = jstemplate_builder::GetI18nTemplateHtml(incognito_tab_html,
                                                       &localized_strings);
}

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
        UserMetrics::RecordComputedAction(wstring_value, dom_ui_->GetProfile());
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
    : DOMUI(contents),
      motd_message_id_(0),
      incognito_(false) {
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

  InitializeCSSCaches();
  if (GetProfile()->IsOffTheRecord()) {
    incognito_ = true;

    IncognitoTabHTMLSource* html_source = new IncognitoTabHTMLSource(
        GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            Singleton<ChromeURLDataManager>::get(),
            &ChromeURLDataManager::AddDataSource,
            html_source));
  } else {
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

    NewTabHTMLSource* html_source = new NewTabHTMLSource(GetProfile());
    bool posted = ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            Singleton<ChromeURLDataManager>::get(),
            &ChromeURLDataManager::AddDataSource,
            html_source));
    if (!posted) {
      html_source->AddRef();
      html_source->Release();  // Keep Valgrind happy in tests.
    }
  }

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
  bool posted = ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          theme));
  if (!posted) {
    theme->AddRef();
    theme->Release();  // Keep Valgrind happy in tests.
  }
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

bool NewTabUI::NewTabHTMLSource::first_view_ = true;

bool NewTabUI::NewTabHTMLSource::first_run_ = true;

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()) {
  InitFullHTML(profile);
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

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html_.size());
  std::copy(full_html_.begin(), full_html_.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

// static
std::string NewTabUI::NewTabHTMLSource::GetCustomNewTabPageFromCommandLine() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  const std::wstring file_path_wstring = command_line->GetSwitchValue(
      switches::kNewTabPage);

#if defined(OS_WIN)
  const FilePath::StringType file_path = file_path_wstring;
#else
  const FilePath::StringType file_path = WideToASCII(file_path_wstring);
#endif

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

void NewTabUI::NewTabHTMLSource::InitFullHTML(Profile* profile) {
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
      profile->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      "true" : "false");
  localized_strings.SetString(L"hasattribution",
      profile->GetThemeProvider()->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ?
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
          l10n_util::GetString(IDS_SYNC_SERVICE_HELP_URL)));
  localized_strings.SetString(L"extensionslink",
      ASCIIToWide(Extension::kGalleryBrowseUrl));

  // Don't initiate the sync related message passing with the page if the sync
  // code is not present.
  if (profile->GetProfileSyncService())
    localized_strings.SetString(L"syncispresent", "true");
  else
    localized_strings.SetString(L"syncispresent", "false");

  if (!profile->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    localized_strings.SetString(L"showsetashomepage", "true");

  SetFontAndTextDirection(&localized_strings);

  // Let the tab know whether it's the first tab being viewed.
  if (first_view_) {
    localized_strings.SetString(L"firstview", L"true");

    // Decrement ntp promo counters; the default values are specified in
    // Browser::RegisterUserPrefs.
    profile->GetPrefs()->SetInteger(prefs::kNTPPromoLineRemaining,
        profile->GetPrefs()->GetInteger(prefs::kNTPPromoLineRemaining) - 1);
    profile->GetPrefs()->SetInteger(prefs::kNTPPromoImageRemaining,
        profile->GetPrefs()->GetInteger(prefs::kNTPPromoImageRemaining) - 1);
    first_view_ = false;
  }

  // Control fade and resize animations.
  std::wstring anim =
      Animation::ShouldRenderRichAnimation() ? L"true" : L"false";
  localized_strings.SetString(L"anim", anim);

  // Pass the shown_sections pref early so that we can prevent flicker.
  const int shown_sections = profile->GetPrefs()->GetInteger(
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

  if (pos != base::StringPiece::npos) {
    full_html_.assign(new_tab_html.data(), pos);
    full_html_.append(json_html);
    size_t after_offset = pos + template_data_placeholder.size();
    full_html_.append(new_tab_html.data() + after_offset,
                      new_tab_html.size() - after_offset);
  } else {
    NOTREACHED();
    full_html_.assign(new_tab_html.data(), new_tab_html.size());
  }
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html_);
}
