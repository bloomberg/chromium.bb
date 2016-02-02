// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

#include <set>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_sync_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_THEMES)
#include "chrome/browser/ui/webui/theme_handler.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/host_desktop.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
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

const char* GetHtmlTextDirection(const base::string16& text) {
  if (base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(text))
    return kRTLHtmlTextDirection;
  else
    return kLTRHtmlTextDirection;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()),
      showing_sync_bubble_(false) {
  g_live_new_tabs.Pointer()->insert(this);
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  web_ui->SetLinkTransitionType(ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  Profile* profile = GetProfile();
  if (!profile->IsOffTheRecord()) {
    web_ui->AddMessageHandler(new MetricsHandler());
    web_ui->AddMessageHandler(new FaviconWebUIHandler());
    web_ui->AddMessageHandler(new NewTabPageHandler());
    web_ui->AddMessageHandler(new CoreAppLauncherHandler());
    web_ui->AddMessageHandler(new NewTabPageSyncHandler());

    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    // We might not have an ExtensionService (on ChromeOS when not logged in
    // for example).
    if (service)
      web_ui->AddMessageHandler(new AppLauncherHandler(service));
  }

#if defined(ENABLE_THEMES)
  // The theme handler can require some CPU, so do it after hooking up the most
  // visited handler. This allows the DB query for the new tab thumbs to happen
  // earlier.
  if (!profile->IsGuestSession())
    web_ui->AddMessageHandler(new ThemeHandler());
#endif

  scoped_ptr<NewTabHTMLSource> html_source(
      new NewTabHTMLSource(profile->GetOriginalProfile()));

  // content::URLDataSource assumes the ownership of the html_source.
  content::URLDataSource::Add(profile, html_source.release());

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(bookmarks::prefs::kShowBookmarkBar,
                             base::Bind(&NewTabUI::OnShowBookmarkBarChanged,
                                        base::Unretained(this)));
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
      content::Source<content::RenderWidgetHost>(render_view_host->GetWidget());
  if (!registrar_.IsRegistered(this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          source)) {
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        source);
  }

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMs), this,
               &NewTabUI::PaintTimeout);
}

void NewTabUI::RenderViewCreated(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::RenderViewReused(RenderViewHost* render_view_host) {
  StartTimingPaint(render_view_host);
}

void NewTabUI::WasHidden() {
  EmitNtpStatistics();
}

void NewTabUI::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE: {
      last_paint_ = base::TimeTicks::Now();
      break;
    }
    default:
      CHECK(false) << "Unexpected notification: " << type;
  }
}

void NewTabUI::EmitNtpStatistics() {
  NTPUserDataLogger::GetOrCreateFromWebContents(
      web_contents())->EmitNtpStatistics();
}

void NewTabUI::OnShowBookmarkBarChanged() {
  base::StringValue attached(
      GetProfile()->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar) ?
          "true" : "false");
  web_ui()->CallJavascriptFunction("ntp.setBookmarkBarAttached", attached);
}

// static
void NewTabUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  CoreAppLauncherHandler::RegisterProfilePrefs(registry);
  NewTabPageHandler::RegisterProfilePrefs(registry);
}

// static
bool NewTabUI::ShouldShowApps() {
// Ash shows apps in app list thus should not show apps page in NTP4.
#if defined(USE_ASH)
  return chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH;
#else
  return true;
#endif
}

// static
void NewTabUI::SetUrlTitleAndDirection(base::DictionaryValue* dictionary,
                                       const base::string16& title,
                                       const GURL& gurl) {
  dictionary->SetString("url", gurl.spec());

  bool using_url_as_the_title = false;
  base::string16 title_to_set(title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = base::UTF8ToUTF16(gurl.spec());
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
  if (using_url_as_the_title)
    direction = kLTRHtmlTextDirection;
  else
    direction = GetHtmlTextDirection(title);

  dictionary->SetString("title", title_to_set);
  dictionary->SetString("direction", direction);
}

// static
void NewTabUI::SetFullNameAndDirection(const base::string16& full_name,
                                       base::DictionaryValue* dictionary) {
  dictionary->SetString("full_name", full_name);
  dictionary->SetString("full_name_direction", GetHtmlTextDirection(full_name));
}

// static
NewTabUI* NewTabUI::FromWebUIController(WebUIController* ui) {
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
    : profile_(profile) {
}

std::string NewTabUI::NewTabHTMLSource::GetSource() const {
  return chrome::kChromeUINewTabHost;
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::map<std::string, std::pair<std::string, int> >::iterator it =
    resource_map_.find(path);
  if (it != resource_map_.end()) {
    scoped_refptr<base::RefCountedStaticMemory> resource_bytes(
        it->second.second ?
            ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                it->second.second) :
            new base::RefCountedStaticMemory);
    callback.Run(resource_bytes.get());
    return;
  }

  if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED() << path << " should not have been requested on the NTP";
    callback.Run(NULL);
    return;
  }

  content::RenderProcessHost* render_host =
      content::RenderProcessHost::FromID(render_process_id);
  NTPResourceCache::WindowType win_type = NTPResourceCache::GetWindowType(
      profile_, render_host);
  scoped_refptr<base::RefCountedMemory> html_bytes(
      NTPResourceCacheFactory::GetForProfile(profile_)->
      GetNewTabHTML(win_type));

  callback.Run(html_bytes.get());
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

bool NewTabUI::NewTabHTMLSource::ShouldAddContentSecurityPolicy() const {
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

NewTabUI::NewTabHTMLSource::~NewTabHTMLSource() {}
