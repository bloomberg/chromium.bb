// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

std::string GetThemePath() {
  return std::string(content::kChromeUIScheme) + "://" +
         std::string(chrome::kChromeUIThemePath) + "/";
}

// use a resource map rather than hard-coded strings.
static const char* kNewTabCSSPath = "css/new_tab_theme.css";
static const char* kNewIncognitoTabCSSPath = "css/incognito_new_tab_theme.css";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, public:

ThemeSource::ThemeSource(Profile* profile)
    : profile_(profile->GetOriginalProfile()) {
  NTPResourceCache::WindowType win_type = NTPResourceCache::GetWindowType(
      profile_, NULL);
  css_bytes_ =
      NTPResourceCacheFactory::GetForProfile(profile)->GetNewTabCSS(win_type);
}

ThemeSource::~ThemeSource() {
}

std::string ThemeSource::GetSource() const {
  return chrome::kChromeUIThemePath;
}

void ThemeSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // Default scale factor if not specified.
  float scale_factor = 1.0f;
  std::string uncached_path;
  webui::ParsePathAndScale(GURL(GetThemePath() + path),
                           &uncached_path,
                           &scale_factor);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    callback.Run(css_bytes_.get());
    return;
  }


  int resource_id = ResourcesUtil::GetThemeResourceId(uncached_path);
  if (resource_id != -1) {
    SendThemeBitmap(callback, resource_id, scale_factor);
    return;
  }

  // We don't have any data to send back.
  callback.Run(NULL);
}

std::string ThemeSource::GetMimeType(const std::string& path) const {
  std::string uncached_path;
  webui::ParsePathAndScale(GURL(GetThemePath() + path), &uncached_path, NULL);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    return "text/css";
  }

  return "image/png";
}

base::MessageLoop* ThemeSource::MessageLoopForRequestPath(
    const std::string& path) const {
  std::string uncached_path;
  webui::ParsePathAndScale(GURL(GetThemePath() + path), &uncached_path, NULL);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    // We generated and cached this when we initialized the object.  We don't
    // have to go back to the UI thread to send the data.
    return NULL;
  }

  // If it's not a themeable image, we don't need to go to the UI thread.
  int resource_id = ResourcesUtil::GetThemeResourceId(uncached_path);
  if (!ThemeProperties::IsThemeableImage(resource_id))
    return NULL;

  return content::URLDataSource::MessageLoopForRequestPath(path);
}

bool ThemeSource::ShouldReplaceExistingSource() const {
  // We currently get the css_bytes_ in the ThemeSource constructor, so we need
  // to recreate the source itself when a theme changes.
  return true;
}

bool ThemeSource::ShouldServiceRequest(const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, private:

void ThemeSource::SendThemeBitmap(
    const content::URLDataSource::GotDataCallback& callback,
    int resource_id,
    float scale_factor) {
  ui::ScaleFactor resource_scale_factor =
      ui::GetSupportedScaleFactor(scale_factor);
  if (ThemeProperties::IsThemeableImage(resource_id)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
    DCHECK(tp);

    scoped_refptr<base::RefCountedMemory> image_data(
        tp->GetRawData(resource_id, resource_scale_factor));
    callback.Run(image_data.get());
  } else {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    callback.Run(
        rb.LoadDataResourceBytesForScale(resource_id, resource_scale_factor));
  }
}
