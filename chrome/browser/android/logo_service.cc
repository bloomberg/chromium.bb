// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_service.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/fixed_logo_api.h"
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

class LogoDecoderDelegate : public ImageDecoder::ImageRequest {
 public:
  LogoDecoderDelegate(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback)
      : image_decoded_callback_(image_decoded_callback),
        weak_ptr_factory_(this) {
    // If the ImageDecoder crashes or otherwise never completes, call
    // OnImageDecodeTimedOut() eventually to ensure that image_decoded_callback_
    // is run.
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&LogoDecoderDelegate::OnDecodeImageFailed,
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
    // TODO(bauerb): Switch to the components/image_fetcher implementation.
    auto delegate =
        base::MakeUnique<LogoDecoderDelegate>(image_decoded_callback);
    ImageDecoder::Start(delegate.release(), encoded_image->data());
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

  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  if (!template_url)
    return;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL logo_url;
  if (command_line->HasSwitch(switches::kSearchProviderLogoURL)) {
    logo_url = GURL(
        command_line->GetSwitchValueASCII(switches::kSearchProviderLogoURL));
  } else {
    logo_url = template_url->logo_url();
  }
  const bool use_fixed_logo = logo_url.is_valid();

  if (!template_url->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data()) &&
      !use_fixed_logo) {
    return;
  }

  if (!logo_tracker_) {
    logo_tracker_ = base::MakeUnique<LogoTracker>(
        profile_->GetPath().Append(kCachedLogoDirectory),
        profile_->GetRequestContext(), base::MakeUnique<ChromeLogoDelegate>());
  }

  if (use_fixed_logo) {
    logo_tracker_->SetServerAPI(
        logo_url, base::Bind(&search_provider_logos::ParseFixedLogoResponse),
        base::Bind(&search_provider_logos::UseFixedLogoUrl));
  } else {
    GURL google_base_url =
        GURL(UIThreadSearchTermsData(profile_).GoogleBaseURLValue());

    bool use_gray_background =
        !base::FeatureList::IsEnabled(chrome::android::kChromeHomeFeature);

    logo_tracker_->SetServerAPI(
        search_provider_logos::GetGoogleDoodleURL(google_base_url),
        search_provider_logos::GetGoogleParseLogoResponseCallback(
            google_base_url),
        search_provider_logos::GetGoogleAppendQueryparamsCallback(
            use_gray_background));
  }

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
