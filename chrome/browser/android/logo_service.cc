// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_service.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/google/google_profile_helper.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
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

const char kGoogleDoodleURLPath[] = "async/newtab_mobile";
const char kCachedLogoDirectory[] = "Search Logo";
const int kDecodeLogoTimeoutSeconds = 30;

// Returns the URL where the doodle can be downloaded, e.g.
// https://www.google.com/async/newtab_mobile. This depends on the user's
// Google domain.
GURL GetGoogleDoodleURL(Profile* profile) {
  // SetPathStr() requires its argument to stay in scope as long as
  // |replacements| is, so a std::string is needed, instead of a char*.
  std::string path = kGoogleDoodleURLPath;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);

  GURL base_url(google_util::CommandLineGoogleBaseURL());
  if (!base_url.is_valid())
    base_url = google_profile_helper::GetGoogleHomePageURL(profile);
  return base_url.ReplaceComponents(replacements);
}

class LogoDecoderDelegate : public ImageDecoder::Delegate {
 public:
  LogoDecoderDelegate(
      const scoped_refptr<ImageDecoder>& image_decoder,
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback)
      : image_decoder_(image_decoder),
        image_decoded_callback_(image_decoded_callback),
        weak_ptr_factory_(this) {
    // If the ImageDecoder crashes or otherwise never completes, call
    // OnImageDecodeTimedOut() eventually to ensure that image_decoded_callback_
    // is run.
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&LogoDecoderDelegate::OnDecodeImageFailed,
                   weak_ptr_factory_.GetWeakPtr(),
                   (const ImageDecoder*) NULL),
        base::TimeDelta::FromSeconds(kDecodeLogoTimeoutSeconds));
  }

  virtual ~LogoDecoderDelegate() {
    image_decoder_->set_delegate(NULL);
  }

  // ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    image_decoded_callback_.Run(decoded_image);
    delete this;
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    image_decoded_callback_.Run(SkBitmap());
    delete this;
  }

 private:
  scoped_refptr<ImageDecoder> image_decoder_;
  base::Callback<void(const SkBitmap&)> image_decoded_callback_;
  base::WeakPtrFactory<LogoDecoderDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoDecoderDelegate);
};

class ChromeLogoDelegate : public search_provider_logos::LogoDelegate {
 public:
  ChromeLogoDelegate() {}
  virtual ~ChromeLogoDelegate() {}

  // search_provider_logos::LogoDelegate:
  virtual void DecodeUntrustedImage(
      const scoped_refptr<base::RefCountedString>& encoded_image,
      base::Callback<void(const SkBitmap&)> image_decoded_callback) OVERRIDE {
    scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(
        NULL,
        encoded_image->data(),
        ImageDecoder::DEFAULT_CODEC);
    LogoDecoderDelegate* delegate =
        new LogoDecoderDelegate(image_decoder, image_decoded_callback);
    image_decoder->set_delegate(delegate);
    image_decoder->Start(base::MessageLoopProxy::current());
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
        BrowserThread::GetBlockingPool(),
        profile_->GetRequestContext(),
        scoped_ptr<search_provider_logos::LogoDelegate>(
            new ChromeLogoDelegate())));
  }

  logo_tracker_->SetServerAPI(
      GetGoogleDoodleURL(profile_),
      base::Bind(&search_provider_logos::GoogleParseLogoResponse),
      base::Bind(&search_provider_logos::GoogleAppendFingerprintToLogoURL));
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
  return Singleton<LogoServiceFactory>::get();
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
