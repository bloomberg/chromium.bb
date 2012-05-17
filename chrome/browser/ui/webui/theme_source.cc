// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"

using content::BrowserThread;

namespace {

// use a resource map rather than hard-coded strings.
static const char* kNewTabCSSPath = "css/new_tab_theme.css";
static const char* kNewIncognitoTabCSSPath = "css/incognito_new_tab_theme.css";

struct ScaleFactorMap {
  const char* name;
  ui::ScaleFactor scale_factor;
};

const ScaleFactorMap kScaleFactorMap[] = {
  { "1x", ui::SCALE_FACTOR_100P },
  { "2x", ui::SCALE_FACTOR_200P },
};

std::string StripQueryParams(const std::string& path) {
  GURL path_url = GURL(std::string(chrome::kChromeUIScheme) + "://" +
                       std::string(chrome::kChromeUIThemePath) + "/" + path);
  return path_url.path().substr(1);  // path() always includes a leading '/'.
}

std::string ParsePathAndScale(const std::string& path,
                              ui::ScaleFactor* scale_factor) {
  // Our path may include cachebuster arguments, so trim them off.
  std::string uncached_path = StripQueryParams(path);
  if (scale_factor)
    *scale_factor = ui::SCALE_FACTOR_100P;

  // Detect and parse resource string ending in @<scale>x.
  std::size_t pos = uncached_path.rfind('@');
  if (pos != std::string::npos) {
    if (scale_factor) {
      for (size_t i = 0; i < arraysize(kScaleFactorMap); i++) {
        if (uncached_path.compare(pos + 1, uncached_path.length() - pos - 1,
            kScaleFactorMap[i].name) == 0) {
          *scale_factor = kScaleFactorMap[i].scale_factor;
        }
      }
    }
    // Strip scale factor specification from path.
    uncached_path = uncached_path.substr(0, pos);
  }
  return uncached_path;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, public:

ThemeSource::ThemeSource(Profile* profile)
    : DataSource(chrome::kChromeUIThemePath, MessageLoop::current()),
      profile_(profile->GetOriginalProfile()) {
  css_bytes_ = NTPResourceCacheFactory::GetForProfile(profile)->GetNewTabCSS(
      profile->IsOffTheRecord());
}

ThemeSource::~ThemeSource() {
}

void ThemeSource::StartDataRequest(const std::string& path,
                                   bool is_incognito,
                                   int request_id) {
  // Default scale factor if not specified.
  ui::ScaleFactor scale_factor;
  std::string uncached_path = ParsePathAndScale(path, &scale_factor);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK((uncached_path == kNewTabCSSPath && !is_incognito) ||
           (uncached_path == kNewIncognitoTabCSSPath && is_incognito));

    SendResponse(request_id, css_bytes_);
    return;
  } else {
    int resource_id = ResourcesUtil::GetThemeResourceId(uncached_path);
    if (resource_id != -1) {
      SendThemeBitmap(request_id, resource_id, scale_factor);
      return;
    }
  }
  // We don't have any data to send back.
  SendResponse(request_id, NULL);
}

std::string ThemeSource::GetMimeType(const std::string& path) const {
  std::string uncached_path = ParsePathAndScale(path, NULL);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    return "text/css";
  }

  return "image/png";
}

MessageLoop* ThemeSource::MessageLoopForRequestPath(
    const std::string& path) const {
  std::string uncached_path = ParsePathAndScale(path, NULL);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    // We generated and cached this when we initialized the object.  We don't
    // have to go back to the UI thread to send the data.
    return NULL;
  }

  // If it's not a themeable image, we don't need to go to the UI thread.
  int resource_id = ResourcesUtil::GetThemeResourceId(uncached_path);
  if (!ThemeService::IsThemeableImage(resource_id))
    return NULL;

  return DataSource::MessageLoopForRequestPath(path);
}

bool ThemeSource::ShouldReplaceExistingSource() const {
  // We currently get the css_bytes_ in the ThemeSource constructor, so we need
  // to recreate the source itself when a theme changes.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, private:

void ThemeSource::SendThemeBitmap(int request_id,
                                  int resource_id,
                                  ui::ScaleFactor scale_factor) {
  if (ThemeService::IsThemeableImage(resource_id)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
    DCHECK(tp);

    // TODO(flackr): Pass scale factor when fetching themeable images.
    scoped_refptr<base::RefCountedMemory> image_data(tp->GetRawData(
        resource_id));
    SendResponse(request_id, image_data);
  } else {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SendResponse(request_id,
                 rb.LoadDataResourceBytes(resource_id, scale_factor));
  }
}
