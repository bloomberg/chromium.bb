// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/metrics/metric_event_duration_details.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/bookmarks_handler.h"
#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"
#include "chrome/browser/ui/webui/ntp/foreign_session_handler.h"
#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_sync_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/ntp/recently_closed_tabs_handler.h"
#include "chrome/browser/ui/webui/ntp/shown_sections_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
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
  virtual void RegisterMessages() OVERRIDE;

  // Callback which records a user action.
  void HandleRecordAction(const ListValue* args);

  // Callback which records into a histogram. |args| contains the histogram
  // name, the value to record, and the maximum allowed value, which can be at
  // most 4000.  The histogram will use at most 100 buckets, one for each 1,
  // 10, or 100 different values, depending on the maximum value.
  void HandleRecordInHistogram(const ListValue* args);

  // Callback for the "logEventTime" message.
  void HandleLogEventTime(const ListValue* args);

 private:

  DISALLOW_COPY_AND_ASSIGN(MetricsHandler);
};

void MetricsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("recordAction",
      base::Bind(&MetricsHandler::HandleRecordAction,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("recordInHistogram",
      base::Bind(&MetricsHandler::HandleRecordInHistogram,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("logEventTime",
      base::Bind(&MetricsHandler::HandleLogEventTime,
                 base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const ListValue* args) {
  std::string string_action = UTF16ToUTF8(ExtractStringValue(args));
  UserMetrics::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const ListValue* args) {
  std::string histogram_name;
  double value;
  double boundary_value;
  if (!args->GetString(0, &histogram_name) ||
      !args->GetDouble(1, &value) ||
      !args->GetDouble(2, &boundary_value)) {
    NOTREACHED();
    return;
  }

  int int_value = static_cast<int>(value);
  int int_boundary_value = static_cast<int>(boundary_value);
  if (int_boundary_value >= 4000 ||
      int_value > int_boundary_value ||
      int_value < 0) {
    NOTREACHED();
    return;
  }

  int bucket_count = int_boundary_value;
  while (bucket_count >= 100) {
    bucket_count /= 10;
  }

  // As |histogram_name| may change between calls, the UMA_HISTOGRAM_ENUMERATION
  // macro cannot be used here.
  base::Histogram* counter =
      base::LinearHistogram::FactoryGet(
          histogram_name, 1, int_boundary_value, bucket_count + 1,
          base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(int_value);
}

void MetricsHandler::HandleLogEventTime(const ListValue* args) {
  std::string event_name = UTF16ToUTF8(ExtractStringValue(args));
  TabContents* tab = web_ui_->tab_contents();

  // Not all new tab pages get timed.  In those cases, we don't have a
  // new_tab_start_time_.
  if (tab->new_tab_start_time().is_null())
    return;

  base::TimeDelta duration = base::TimeTicks::Now() - tab->new_tab_start_time();
  MetricEventDurationDetails details(event_name,
      static_cast<int>(duration.InMilliseconds()));

  if (event_name == "Tab.NewTabScriptStart") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabScriptStart", duration);
  } else if (event_name == "Tab.NewTabDOMContentLoaded") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabDOMContentLoaded", duration);
  } else if (event_name == "Tab.NewTabOnload") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabOnload", duration);
    // The new tab page has finished loading; reset it.
    tab->set_new_tab_start_time(base::TimeTicks());
  } else {
    NOTREACHED();
  }
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_METRIC_EVENT_DURATION,
      Source<TabContents>(tab),
      Details<MetricEventDurationDetails>(&details));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(TabContents* contents)
    : ChromeWebUI(contents) {
  // Override some options on the Web UI.
  hide_favicon_ = true;

  if (!NTP4Enabled() &&
      GetProfile()->GetPrefs()->GetBoolean(prefs::kEnableBookmarkBar) &&
      browser_defaults::bookmarks_enabled) {
    set_force_bookmark_bar_visible(true);
  }

  focus_location_bar_by_default_ = true;
  should_hide_url_ = true;
  overridden_title_ = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  link_transition_type_ = PageTransition::AUTO_BOOKMARK;

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

    AddMessageHandler((new NewTabPageHandler())->Attach(this));
    if (NTP4Enabled()) {
      AddMessageHandler((new BookmarksHandler())->Attach(this));
      AddMessageHandler((new FaviconWebUIHandler())->Attach(this));
    }
  }

  // Initializing the CSS and HTML can require some CPU, so do it after
  // we've hooked up the most visited handler.  This allows the DB query
  // for the new tab thumbs to happen earlier.
  InitializeCSSCaches();
  NewTabHTMLSource* html_source =
      new NewTabHTMLSource(GetProfile()->GetOriginalProfile());
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
  // Listen for bookmark bar visibility changes.
  registrar_.Add(this,
                 chrome::NOTIFICATION_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
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
        chrome::NOTIFICATION_INITIAL_NEW_TAB_UI_LOAD,
        Source<Profile>(GetProfile()),
        Details<int>(&load_time_ms));
    UMA_HISTOGRAM_TIMES("NewTabUI load", load_time);
  } else {
    // Not enough quiet time has elapsed.
    // Some more paints must've occurred since we set the timeout.
    // Wait some more.
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
                 &NewTabUI::PaintTimeout);
  }
}

void NewTabUI::StartTimingPaint(RenderViewHost* render_view_host) {
  start_ = base::TimeTicks::Now();
  last_paint_ = start_;
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
      Source<RenderWidgetHost>(render_view_host));
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
               &NewTabUI::PaintTimeout);

}
void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      InitializeCSSCaches();
      ListValue args;
      args.Append(Value::CreateStringValue(
          ThemeServiceFactory::GetForProfile(GetProfile())->HasCustomImage(
              IDR_THEME_NTP_ATTRIBUTION) ?
          "true" : "false"));
      CallJavascriptFunction("themeChanged", args);
      break;
    }
    case chrome::NOTIFICATION_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED: {
      if (GetProfile()->GetPrefs()->IsManagedPreference(
              prefs::kEnableBookmarkBar)) {
        break;
      }
      if (!NTP4Enabled()) {
        if (GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
          CallJavascriptFunction("bookmarkBarAttached");
        else
          CallJavascriptFunction("bookmarkBarDetached");
      }
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT: {
      last_paint_ = base::TimeTicks::Now();
      break;
    }
    default:
      CHECK(false) << "Unexpected notification: " << type;
  }
}

void NewTabUI::InitializeCSSCaches() {
  Profile* profile = GetProfile();
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);
}

// static
void NewTabUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kNTPPrefVersion,
                             0,
                             PrefService::UNSYNCABLE_PREF);

  NewTabPageHandler::RegisterUserPrefs(prefs);
  AppLauncherHandler::RegisterUserPrefs(prefs);
  MostVisitedHandler::RegisterUserPrefs(prefs);
  ShownSectionsHandler::RegisterUserPrefs(prefs);
  if (NTP4Enabled())
    BookmarksHandler::RegisterUserPrefs(prefs);

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

// static
bool NewTabUI::NTP4Enabled() {
#if defined(TOUCH_UI)
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewTabPage);
#else
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewTabPage);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()),
      profile_(profile) {
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(const std::string& path,
                                                  bool is_incognito,
                                                  int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (AppLauncherHandler::HandlePing(profile_, path)) {
    return;
  } else if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED() << path << " should not have been requested on the NTP";
    return;
  }

  scoped_refptr<RefCountedMemory> html_bytes(
      NTPResourceCacheFactory::GetForProfile(profile_)->
      GetNewTabHTML(is_incognito));

  SendResponse(request_id, html_bytes);
}

std::string NewTabUI::NewTabHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

bool NewTabUI::NewTabHTMLSource::ShouldReplaceExistingSource() const {
  return false;
}
