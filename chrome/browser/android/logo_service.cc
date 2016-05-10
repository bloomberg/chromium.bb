// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_service.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using search_provider_logos::Logo;
using search_provider_logos::LogoDelegate;
using search_provider_logos::LogoTracker;

namespace {

const char kCachedLogoDirectory[] = "Search Logo";
const int kDecodeLogoTimeoutSeconds = 30;

// Returns the URL where the doodle can be downloaded, e.g.
// https://www.google.com/async/newtab_mobile. This depends on the user's
// Google domain.
GURL GetGoogleDoodleURL(Profile* profile) {
  GURL google_base_url(UIThreadSearchTermsData(profile).GoogleBaseURLValue());
  const char kGoogleDoodleURLPath[] = "async/newtab_mobile";
  GURL::Replacements replacements;
  replacements.SetPathStr(kGoogleDoodleURLPath);
  return google_base_url.ReplaceComponents(replacements);
}

class LogoDecoderDelegate : public ImageDecoder::ImageRequest {
 public:
  LogoDecoderDelegate(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback)
      : image_decoded_callback_(image_decoded_callback),
        weak_ptr_factory_(this) {
    // If the ImageDecoder crashes or otherwise never completes, call
    // OnImageDecodeTimedOut() eventually to ensure that image_decoded_callback_
    // is run.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&LogoDecoderDelegate::OnDecodeImageFailed,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kDecodeLogoTimeoutSeconds));
  }

  ~LogoDecoderDelegate() override {}

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    image_decoded_callback_.Run(decoded_image);
    delete this;
  }

  void OnDecodeImageFailed() override {
    image_decoded_callback_.Run(SkBitmap());
    delete this;
  }

 private:
  base::Callback<void(const SkBitmap&)> image_decoded_callback_;
  base::WeakPtrFactory<LogoDecoderDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoDecoderDelegate);
};

class ChromeLogoDelegate : public search_provider_logos::LogoDelegate {
 public:
  ChromeLogoDelegate() {}
  ~ChromeLogoDelegate() override {}

  // search_provider_logos::LogoDelegate:
  void DecodeUntrustedImage(
      const scoped_refptr<base::RefCountedString>& encoded_image,
      base::Callback<void(const SkBitmap&)> image_decoded_callback) override {
    LogoDecoderDelegate* delegate =
        new LogoDecoderDelegate(image_decoded_callback);
    ImageDecoder::Start(delegate, encoded_image->data());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLogoDelegate);
};

}  // namespace

// LogoService ----------------------------------------------------------------

LogoService::LogoService(Profile* profile) : profile_(profile) {
}

LogoService::~LogoService() {
}

void LogoService::GetLogo(search_provider_logos::LogoObserver* observer) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service)
    return;

  TemplateURL* template_url = template_url_service->GetDefaultSearchProvider();
  if (!template_url || !template_url->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data()))
    return;

  if (!logo_tracker_) {
    logo_tracker_.reset(new LogoTracker(
        profile_->GetPath().Append(kCachedLogoDirectory),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        BrowserThread::GetBlockingPool(), profile_->GetRequestContext(),
        std::unique_ptr<search_provider_logos::LogoDelegate>(
            new ChromeLogoDelegate())));
  }

  logo_tracker_->SetServerAPI(
      GetGoogleDoodleURL(profile_),
      base::Bind(&search_provider_logos::GoogleParseLogoResponse),
      base::Bind(&search_provider_logos::GoogleAppendQueryparamsToLogoURL),
      true, /* wants_cta */
      base::FeatureList::IsEnabled(chrome::android::kNTPTransparentDoodle));
  logo_tracker_->GetLogo(observer);
}

// LogoServiceFactory ---------------------------------------------------------

// static
LogoService* LogoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LogoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LogoServiceFactory* LogoServiceFactory::GetInstance() {
  return base::Singleton<LogoServiceFactory>::get();
}

LogoServiceFactory::LogoServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LogoService",
          BrowserContextDependencyManager::GetInstance()) {
}

LogoServiceFactory::~LogoServiceFactory() {}

KeyedService* LogoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsOffTheRecord());
  return new LogoService(profile);
}
