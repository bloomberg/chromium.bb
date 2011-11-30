// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

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
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
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
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

// The amount of time there must be no painting for us to consider painting
// finished.  Observed times are in the ~1200ms range on Windows.
const int kTimeoutMs = 2000;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kLTRHtmlTextDirection[] = "ltr";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(TabContents* contents)
    : ChromeWebUI(contents) {
  // Override some options on the Web UI.
  hide_favicon_ = true;

  focus_location_bar_by_default_ = true;
  should_hide_url_ = true;
  overridden_title_ = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  link_transition_type_ = content::PAGE_TRANSITION_AUTO_BOOKMARK;

  if (!GetProfile()->IsOffTheRecord()) {
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
    AddMessageHandler((new BookmarksHandler())->Attach(this));
    AddMessageHandler((new FaviconWebUIHandler())->Attach(this));
  }

  if (NTPLoginHandler::ShouldShow(GetProfile()))
    AddMessageHandler((new NTPLoginHandler())->Attach(this));

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
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
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
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INITIAL_NEW_TAB_UI_LOAD,
        content::Source<Profile>(GetProfile()),
        content::Details<int>(&load_time_ms));
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
      content::Source<RenderWidgetHost>(render_view_host));
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
               &NewTabUI::PaintTimeout);

}
void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

bool NewTabUI::CanShowBookmarkBar() const {
  PrefService* prefs = GetProfile()->GetPrefs();
  bool disabled_by_policy =
      prefs->IsManagedPreference(prefs::kShowBookmarkBar) &&
      !prefs->GetBoolean(prefs::kShowBookmarkBar);
  return browser_defaults::bookmarks_enabled &&
      !disabled_by_policy &&
      !NTP4BookmarkFeaturesEnabled();
}

void NewTabUI::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
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
  NewTabPageHandler::RegisterUserPrefs(prefs);
  AppLauncherHandler::RegisterUserPrefs(prefs);
  MostVisitedHandler::RegisterUserPrefs(prefs);
  BookmarksHandler::RegisterUserPrefs(prefs);
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
  std::string direction;
  if (!using_url_as_the_title &&
      base::i18n::IsRTL() &&
      base::i18n::StringContainsStrongRTLChars(title)) {
    direction = kRTLHtmlTextDirection;
  } else {
    direction = kLTRHtmlTextDirection;
  }
  dictionary->SetString("title", title_to_set);
  dictionary->SetString("direction", direction);
}

// static
bool NewTabUI::NTP4BookmarkFeaturesEnabled() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  return cl->HasSwitch(switches::kEnableNTPBookmarkFeatures);
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

  if (!path.empty() && path[0] != '#') {
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
