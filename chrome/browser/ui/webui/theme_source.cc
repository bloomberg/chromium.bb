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
#include "chrome/browser/themes/browser_theme_pack.h"
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
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
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

void ProcessImageOnUIThread(const gfx::ImageSkia& image,
                            float scale_factor,
                            scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const gfx::ImageSkiaRep& rep = image.GetRepresentation(scale_factor);
  gfx::PNGCodec::EncodeBGRASkBitmap(
      rep.sk_bitmap(), false /* discard transparency */, &data->data());
}

void ProcessResourceOnUIThread(int resource_id,
                               float scale_factor,
                               scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProcessImageOnUIThread(
      *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id),
      scale_factor, data);
}

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
    if (GetMimeType(path) == "image/png")
      SendThemeImage(callback, resource_id, scale_factor);
    else
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
  if (!BrowserThemePack::IsPersistentImageID(resource_id))
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
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
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

void ThemeSource::SendThemeImage(
    const content::URLDataSource::GotDataCallback& callback,
    int resource_id,
    float scale_factor) {
  // If the resource bundle contains the data pack for |scale_factor|, we can
  // safely fallback to SendThemeBitmap().
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (ui::GetScaleForScaleFactor(rb.GetMaxScaleFactor()) >= scale_factor) {
    SendThemeBitmap(callback, resource_id, scale_factor);
    return;
  }

  // Otherwise, we should use gfx::ImageSkia to obtain the data. ImageSkia can
  // rescale the bitmap if its backend doesn't contain the representation for
  // the specified scale factor. This is the fallback path in case chrome is
  // shipped without 2x resource pack but needs to use HighDPI display, which
  // can happen in ChromeOS or Linux.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
    DCHECK(tp);

    ProcessImageOnUIThread(*tp->GetImageSkiaNamed(resource_id), scale_factor,
                           data);
    callback.Run(data.get());
  } else {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // Fetching image data in ResourceBundle should happen on the UI thread. See
    // crbug.com/449277
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ProcessResourceOnUIThread, resource_id, scale_factor, data),
        base::Bind(callback, data));
  }
}
