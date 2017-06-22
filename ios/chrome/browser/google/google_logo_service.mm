// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/google/google_logo_service.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using search_provider_logos::Logo;
using search_provider_logos::LogoDelegate;
using search_provider_logos::LogoTracker;

namespace {

const char kGoogleDoodleURLPath[] = "async/newtab_mobile";
static NSArray* const kDoodleCacheDirectory = @[ @"Chromium", @"Doodle" ];

// Cache directory for doodle.
base::FilePath DoodleDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString* path = [paths objectAtIndex:0];
  NSArray* path_components =
      [NSArray arrayWithObjects:path, kDoodleCacheDirectory[0],
                                kDoodleCacheDirectory[1], nil];
  return base::FilePath(
      base::SysNSStringToUTF8([NSString pathWithComponents:path_components]));
}

// Returns the URL where the doodle can be downloaded, e.g.
// https://www.google.com/async/newtab_mobile. This depends on the user's
// Google domain.
GURL GetGoogleDoodleURL(ios::ChromeBrowserState* browser_state) {
  GURL google_base_url(
      ios::UIThreadSearchTermsData(browser_state).GoogleBaseURLValue());
  // SetPathStr() requires its argument to stay in scope as long as
  // |replacements| is, so a std::string is needed, instead of a char*.
  std::string path = kGoogleDoodleURLPath;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);

  return google_base_url.ReplaceComponents(replacements);
}

class IOSChromeLogoDelegate : public search_provider_logos::LogoDelegate {
 public:
  IOSChromeLogoDelegate() {}
  ~IOSChromeLogoDelegate() override {}

  // search_provider_logos::LogoDelegate implementation.
  void DecodeUntrustedImage(
      const scoped_refptr<base::RefCountedString>& encoded_image,
      base::Callback<void(const SkBitmap&)> image_decoded_callback) override {
    SkBitmap bitmap = gfx::Image::CreateFrom1xPNGBytes(encoded_image->front(),
                                                       encoded_image->size())
                          .AsBitmap();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(image_decoded_callback, bitmap));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeLogoDelegate);
};

}  // namespace

GoogleLogoService::GoogleLogoService(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

GoogleLogoService::~GoogleLogoService() {}

void GoogleLogoService::GetLogo(search_provider_logos::LogoObserver* observer) {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
  if (!template_url_service)
    return;

  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  if (!template_url ||
      !template_url->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data()))
    return;

  if (!logo_tracker_) {
    logo_tracker_.reset(new LogoTracker(
        DoodleDirectory(),
        web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
        web::WebThread::GetBlockingPool(), browser_state_->GetRequestContext(),
        std::unique_ptr<search_provider_logos::LogoDelegate>(
            new IOSChromeLogoDelegate())));
  }

  logo_tracker_->SetServerAPI(
      GetGoogleDoodleURL(browser_state_),
      base::Bind(&search_provider_logos::GoogleParseLogoResponse),
      base::Bind(&search_provider_logos::GoogleAppendQueryparamsToLogoURL,
                 false /* gray_background */));
  logo_tracker_->GetLogo(observer);
}

void GoogleLogoService::SetCachedLogo(const search_provider_logos::Logo* logo) {
  if (logo) {
    if (cached_metadata_.fingerprint == logo->metadata.fingerprint) {
      return;
    }
    if (cached_image_.tryAllocPixels(logo->image.info())) {
      logo->image.readPixels(cached_image_.info(), cached_image_.getPixels(),
                             cached_image_.rowBytes(), 0, 0);
    }
    cached_metadata_ = logo->metadata;
  } else {
    cached_image_ = SkBitmap();
    cached_metadata_ = empty_metadata;
  }
}

search_provider_logos::Logo GoogleLogoService::GetCachedLogo() {
  search_provider_logos::Logo logo;
  logo.image = cached_image_;
  logo.metadata = cached_metadata_;
  return logo;
}
