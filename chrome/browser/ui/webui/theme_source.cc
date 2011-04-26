// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp_resource_cache.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"

// use a resource map rather than hard-coded strings.
static const char* kNewTabCSSPath = "css/newtab.css";
static const char* kNewIncognitoTabCSSPath = "css/newincognitotab.css";

static std::string StripQueryParams(const std::string& path) {
  GURL path_url = GURL(std::string(chrome::kChromeUIScheme) + "://" +
                       std::string(chrome::kChromeUIThemePath) + "/" + path);
  return path_url.path().substr(1);  // path() always includes a leading '/'.
}

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, public:

ThemeSource::ThemeSource(Profile* profile)
    : DataSource(chrome::kChromeUIThemePath, MessageLoop::current()),
      profile_(profile->GetOriginalProfile()) {
  css_bytes_ = profile_->GetNTPResourceCache()->GetNewTabCSS(
      profile->IsOffTheRecord());
}

ThemeSource::~ThemeSource() {
}

void ThemeSource::StartDataRequest(const std::string& path,
                                   bool is_incognito,
                                   int request_id) {
  // Our path may include cachebuster arguments, so trim them off.
  std::string uncached_path = StripQueryParams(path);

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
      SendThemeBitmap(request_id, resource_id);
      return;
    }
  }
  // We don't have any data to send back.
  SendResponse(request_id, NULL);
}

std::string ThemeSource::GetMimeType(const std::string& path) const {
  std::string uncached_path = StripQueryParams(path);

  if (uncached_path == kNewTabCSSPath ||
      uncached_path == kNewIncognitoTabCSSPath) {
    return "text/css";
  }

  return "image/png";
}

MessageLoop* ThemeSource::MessageLoopForRequestPath(
    const std::string& path) const {
  std::string uncached_path = StripQueryParams(path);

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

void ThemeSource::SendThemeBitmap(int request_id, int resource_id) {
  if (ThemeService::IsThemeableImage(resource_id)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
    DCHECK(tp);

    scoped_refptr<RefCountedMemory> image_data(tp->GetRawData(resource_id));
    SendResponse(request_id, image_data);
  } else {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SendResponse(request_id, rb.LoadDataResourceBytes(resource_id));
  }
}
