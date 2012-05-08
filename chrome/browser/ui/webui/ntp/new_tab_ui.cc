// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
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
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"
#include "chrome/browser/ui/webui/ntp/foreign_session_handler.h"
#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/ntp/recently_closed_tabs_handler.h"
#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_sync_handler.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::UserMetricsAction;
using content::WebContents;
using content::WebUIController;

namespace {

// The amount of time there must be no painting for us to consider painting
// finished.  Observed times are in the ~1200ms range on Windows.
const int kTimeoutMs = 2000;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kLTRHtmlTextDirection[] = "ltr";

static base::LazyInstance<std::set<const WebUIController*> > g_live_new_tabs;

// Group IDs for the web store link field trial.
int g_footer_group = 0;
int g_hint_group = 0;

bool WebStoreLinkExperimentGroupIs(int group) {
  return base::FieldTrialList::TrialExists(kWebStoreLinkExperiment) &&
      base::FieldTrialList::FindValue(kWebStoreLinkExperiment) == group;
}

}  // namespace

// The Web Store footer experiment FieldTrial name.
const char kWebStoreLinkExperiment[] = "WebStoreLinkExperiment";

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      showing_sync_bubble_(false) {
  g_live_new_tabs.Pointer()->insert(this);
  // Override some options on the Web UI.
  web_ui->HideFavicon();

  web_ui->FocusLocationBarByDefault();
  web_ui->HideURL();
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  web_ui->SetLinkTransitionType(content::PAGE_TRANSITION_AUTO_BOOKMARK);

  if (!GetProfile()->IsOffTheRecord()) {
    web_ui->AddMessageHandler(new browser_sync::ForeignSessionHandler());
    web_ui->AddMessageHandler(new MostVisitedHandler());
    if (NewTabUI::IsSuggestionsPageEnabled())
      web_ui->AddMessageHandler(new SuggestionsHandler());
    web_ui->AddMessageHandler(new RecentlyClosedTabsHandler());
    web_ui->AddMessageHandler(new MetricsHandler());
#if !defined(OS_ANDROID)
    // Android doesn't have a sync promo/username on NTP.
    if (GetProfile()->IsSyncAccessible())
      web_ui->AddMessageHandler(new NewTabPageSyncHandler());

    // Or apps.
    if (ShouldShowApps()) {
      ExtensionService* service = GetProfile()->GetExtensionService();
      // We might not have an ExtensionService (on ChromeOS when not logged in
      // for example).
      if (service)
        web_ui->AddMessageHandler(new AppLauncherHandler(service));
    }
#endif

    web_ui->AddMessageHandler(new NewTabPageHandler());
    web_ui->AddMessageHandler(new FaviconWebUIHandler());
  }

#if !defined(OS_ANDROID)
  // Android uses native UI for sync setup.
  if (NTPLoginHandler::ShouldShow(GetProfile()))
    web_ui->AddMessageHandler(new NTPLoginHandler());
#endif

  // Initializing the CSS and HTML can require some CPU, so do it after
  // we've hooked up the most visited handler.  This allows the DB query
  // for the new tab thumbs to happen earlier.
  InitializeCSSCaches();
  NewTabHTMLSource* html_source =
      new NewTabHTMLSource(GetProfile()->GetOriginalProfile());
  // These two resources should be loaded only if suggestions NTP is enabled.
  html_source->AddResource("suggestions_page.css", "text/css",
      NewTabUI::IsSuggestionsPageEnabled() ? IDR_SUGGESTIONS_PAGE_CSS : 0);
  if (NewTabUI::IsSuggestionsPageEnabled()) {
    html_source->AddResource("suggestions_page.js", "application/javascript",
        IDR_SUGGESTIONS_PAGE_JS);
  }
  // ChromeURLDataManager assumes the ownership of the html_source and in some
  // tests immediately deletes it, so html_source should not be accessed after
  // this call.
  Profile* profile = GetProfile();
  ChromeURLDataManager::AddDataSource(profile, html_source);

  pref_change_registrar_.Init(GetProfile()->GetPrefs());
  pref_change_registrar_.Add(prefs::kShowBookmarkBar, this);

#if defined(ENABLE_THEMES)
  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
#endif
}

NewTabUI::~NewTabUI() {
  g_live_new_tabs.Pointer()->erase(this);
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

  content::NotificationSource source =
      content::Source<content::RenderWidgetHost>(render_view_host);
  if (!registrar_.IsRegistered(this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
          source)) {
    registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
                   source);
  }

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
               &NewTabUI::PaintTimeout);
}

bool NewTabUI::CanShowBookmarkBar() const {
  PrefService* prefs = GetProfile()->GetPrefs();
  bool disabled_by_policy =
      prefs->IsManagedPreference(prefs::kShowBookmarkBar) &&
      !prefs->GetBoolean(prefs::kShowBookmarkBar);
  return browser_defaults::bookmarks_enabled && !disabled_by_policy;
}

void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {  // kShowBookmarkBar
      StringValue attached(
          GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
              "true" : "false");
      web_ui()->CallJavascriptFunction("ntp.setBookmarkBarAttached", attached);
      break;
    }
#if defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      InitializeCSSCaches();
      StringValue attribution(
          ThemeServiceFactory::GetForProfile(GetProfile())->HasCustomImage(
              IDR_THEME_NTP_ATTRIBUTION) ? "true" : "false");
      web_ui()->CallJavascriptFunction("ntp.themeChanged", attribution);
      break;
    }
#endif
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT: {
      last_paint_ = base::TimeTicks::Now();
      break;
    }
    default:
      CHECK(false) << "Unexpected notification: " << type;
  }
}

void NewTabUI::InitializeCSSCaches() {
#if defined(ENABLE_THEMES)
  Profile* profile = GetProfile();
  ThemeSource* theme = new ThemeSource(profile);
  ChromeURLDataManager::AddDataSource(profile, theme);
#endif
}

// static
void NewTabUI::RegisterUserPrefs(PrefService* prefs) {
  NewTabPageHandler::RegisterUserPrefs(prefs);
#if !defined(OS_ANDROID)
  AppLauncherHandler::RegisterUserPrefs(prefs);
#endif
  MostVisitedHandler::RegisterUserPrefs(prefs);
  if (NewTabUI::IsSuggestionsPageEnabled())
    SuggestionsHandler::RegisterUserPrefs(prefs);
  browser_sync::ForeignSessionHandler::RegisterUserPrefs(prefs);
}

// static
void NewTabUI::SetupFieldTrials() {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kWebStoreLinkExperiment, 1, "Disabled", 2025, 6, 1, NULL));

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // 33.3% in each group.
  g_footer_group = trial->AppendGroup("FooterLink", 1);
  g_hint_group = trial->AppendGroup("PlusIcon", 0);
}

// static
bool NewTabUI::ShouldShowWebStoreFooterLink() {
  const CommandLine* cli = CommandLine::ForCurrentProcess();
  return cli->HasSwitch(switches::kEnableWebStoreLink) ||
      WebStoreLinkExperimentGroupIs(g_footer_group);
}

// static
bool NewTabUI::ShouldShowAppInstallHint() {
  const CommandLine* cli = CommandLine::ForCurrentProcess();
  return cli->HasSwitch(switches::kNtpAppInstallHint) ||
      WebStoreLinkExperimentGroupIs(g_hint_group);
}

// static
bool NewTabUI::ShouldShowApps() {
#if defined(USE_ASH) || defined(OS_ANDROID)
  // Ash shows apps in app list thus should not show apps page in NTP4.
  // Android does not have apps.
  return false;
#else
  return true;
#endif
}

// static
bool NewTabUI::IsSuggestionsPageEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSuggestionsTabPage);
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
NewTabUI* NewTabUI::FromWebUIController(content::WebUIController* ui) {
  if (!g_live_new_tabs.Pointer()->count(ui))
    return NULL;
  return static_cast<NewTabUI*>(ui);
}

Profile* NewTabUI::GetProfile() const {
  return Profile::FromWebUI(web_ui());
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

  std::map<std::string, std::pair<std::string, int> >::iterator it =
    resource_map_.find(path);
  if (it != resource_map_.end()) {
    scoped_refptr<base::RefCountedStaticMemory> resource_bytes(
        it->second.second ?
            ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                it->second.second) :
            new base::RefCountedStaticMemory);
    SendResponse(request_id, resource_bytes);
    return;
  }

  if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED() << path << " should not have been requested on the NTP";
    return;
  }

  scoped_refptr<base::RefCountedMemory> html_bytes(
      NTPResourceCacheFactory::GetForProfile(profile_)->
      GetNewTabHTML(is_incognito));

  SendResponse(request_id, html_bytes);
}

std::string NewTabUI::NewTabHTMLSource::GetMimeType(const std::string& resource)
    const {
  std::map<std::string, std::pair<std::string, int> >::const_iterator it =
      resource_map_.find(resource);
  if (it != resource_map_.end())
    return it->second.first;
  return "text/html";
}

bool NewTabUI::NewTabHTMLSource::ShouldReplaceExistingSource() const {
  return false;
}

void NewTabUI::NewTabHTMLSource::AddResource(const char* resource,
                                             const char* mime_type,
                                             int resource_id) {
  DCHECK(resource);
  DCHECK(mime_type);
  resource_map_[std::string(resource)] =
      std::make_pair(std::string(mime_type), resource_id);
}
