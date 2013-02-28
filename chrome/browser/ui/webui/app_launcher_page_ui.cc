// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_launcher_page_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/app_resource_cache_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_THEMES)
#include "chrome/browser/ui/webui/theme_handler.h"
#endif

class ExtensionService;

using content::BrowserThread;

namespace {

const char kUmaPaintTimesLabel[] = "AppLauncherPageUI load";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AppLauncherPageUI

AppLauncherPageUI::AppLauncherPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(IDS_APP_LAUNCHER_TAB_TITLE));

#if !defined(OS_ANDROID)
  // Android uses native UI for sync setup.
  if (NTPLoginHandler::ShouldShow(GetProfile()))
    web_ui->AddMessageHandler(new NTPLoginHandler());

  if (!GetProfile()->IsOffTheRecord()) {
    ExtensionService* service = GetProfile()->GetExtensionService();
    // We should not be launched without an ExtensionService.
    DCHECK(service);
    web_ui->AddMessageHandler(new AppLauncherHandler(service));
  }
#endif

#if defined(ENABLE_THEMES)
  // The theme handler can require some CPU, so do it after hooking up the most
  // visited handler. This allows the DB query for the new tab thumbs to happen
  // earlier.
  web_ui->AddMessageHandler(new ThemeHandler());
#endif

  scoped_ptr<HTMLSource> html_source(
      new HTMLSource(GetProfile()->GetOriginalProfile()));
  content::URLDataSource::Add(GetProfile(), html_source.release());
}

AppLauncherPageUI::~AppLauncherPageUI() {
}

// static
base::RefCountedMemory* AppLauncherPageUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_WEBSTORE_ICON_16, scale_factor);
}

Profile* AppLauncherPageUI::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

///////////////////////////////////////////////////////////////////////////////
// HTMLSource

AppLauncherPageUI::HTMLSource::HTMLSource(Profile* profile)
    : profile_(profile) {
}

std::string AppLauncherPageUI::HTMLSource::GetSource() {
  return chrome::kChromeUIAppLauncherPageHost;
}

void AppLauncherPageUI::HTMLSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  NTPResourceCache* resource = AppResourceCacheFactory::GetForProfile(profile_);
  resource->set_should_show_most_visited_page(false);
  resource->set_should_show_other_devices_menu(false);
  resource->set_should_show_recently_closed_menu(false);

  scoped_refptr<base::RefCountedMemory> html_bytes(
      resource->GetNewTabHTML(is_incognito));

  callback.Run(html_bytes);
}

std::string AppLauncherPageUI::HTMLSource::GetMimeType(
    const std::string& resource) const {
  return "text/html";
}

bool AppLauncherPageUI::HTMLSource::ShouldReplaceExistingSource() const {
  return false;
}

bool AppLauncherPageUI::HTMLSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

AppLauncherPageUI::HTMLSource::~HTMLSource() {}
