// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_service_impl.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/fixed_logo_api.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "components/search_provider_logos/logo_cache.h"
#include "components/search_provider_logos/logo_tracker.h"
#include "components/search_provider_logos/switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/image/image.h"

using search_provider_logos::LogoDelegate;
using search_provider_logos::LogoTracker;

namespace search_provider_logos {
namespace {

const int kDecodeLogoTimeoutSeconds = 30;

// Implements a callback for image_fetcher::ImageDecoder. If Run() is called on
// a callback returned by GetCallback() within 30 seconds, forwards the decoded
// image to the wrapped callback. If not, sends an empty image to the wrapped
// callback instead. Either way, deletes the object and prevents further calls.
//
// TODO(sfiera): find a more idiomatic way of setting a deadline on the
// callback. This is implemented as a self-deleting object in part because it
// needed to when it used to be a delegate and in part because I couldn't figure
// out a better way, now that it isn't.
class ImageDecodedHandlerWithTimeout {
 public:
  static base::Callback<void(const gfx::Image&)> Wrap(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback) {
    auto* handler = new ImageDecodedHandlerWithTimeout(image_decoded_callback);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                   handler->weak_ptr_factory_.GetWeakPtr(), gfx::Image()),
        base::TimeDelta::FromSeconds(kDecodeLogoTimeoutSeconds));
    return base::Bind(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                      handler->weak_ptr_factory_.GetWeakPtr());
  }

 private:
  explicit ImageDecodedHandlerWithTimeout(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback)
      : image_decoded_callback_(image_decoded_callback),
        weak_ptr_factory_(this) {}

  void OnImageDecoded(const gfx::Image& decoded_image) {
    image_decoded_callback_.Run(decoded_image.AsBitmap());
    delete this;
  }

  base::Callback<void(const SkBitmap&)> image_decoded_callback_;
  base::WeakPtrFactory<ImageDecodedHandlerWithTimeout> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodedHandlerWithTimeout);
};

class LogoDelegateImpl : public search_provider_logos::LogoDelegate {
 public:
  explicit LogoDelegateImpl(
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder)
      : image_decoder_(std::move(image_decoder)) {}

  ~LogoDelegateImpl() override = default;

  // search_provider_logos::LogoDelegate:
  void DecodeUntrustedImage(
      const scoped_refptr<base::RefCountedString>& encoded_image,
      base::Callback<void(const SkBitmap&)> image_decoded_callback) override {
    image_decoder_->DecodeImage(
        encoded_image->data(),
        gfx::Size(),  // No particular size desired.
        ImageDecodedHandlerWithTimeout::Wrap(image_decoded_callback));
  }

 private:
  const std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  DISALLOW_COPY_AND_ASSIGN(LogoDelegateImpl);
};

void ObserverOnLogoAvailable(LogoObserver* observer,
                             bool from_cache,
                             LogoCallbackReason type,
                             const base::Optional<Logo>& logo) {
  switch (type) {
    case LogoCallbackReason::DISABLED:
    case LogoCallbackReason::CANCELED:
    case LogoCallbackReason::FAILED:
      break;

    case LogoCallbackReason::REVALIDATED:
      // TODO(sfiera): double-check whether we should inform the observer of the
      // fresh metadata.
      break;

    case LogoCallbackReason::DETERMINED:
      observer->OnLogoAvailable(logo ? &logo.value() : nullptr, from_cache);
      break;
  }
  if (!from_cache) {
    observer->OnObserverRemoved();
  }
}

void RunCallbacksWithDisabled(LogoCallbacks callbacks) {
  if (callbacks.on_cached_encoded_logo_available) {
    std::move(callbacks.on_cached_encoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_cached_decoded_logo_available) {
    std::move(callbacks.on_cached_decoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_fresh_encoded_logo_available) {
    std::move(callbacks.on_fresh_encoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_fresh_decoded_logo_available) {
    std::move(callbacks.on_fresh_decoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
}

}  // namespace

LogoServiceImpl::LogoServiceImpl(
    const base::FilePath& cache_directory,
    TemplateURLService* template_url_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    bool use_gray_background)
    : LogoService(),
      cache_directory_(cache_directory),
      template_url_service_(template_url_service),
      request_context_getter_(request_context_getter),
      use_gray_background_(use_gray_background),
      image_decoder_(std::move(image_decoder)) {}

LogoServiceImpl::~LogoServiceImpl() = default;

void LogoServiceImpl::GetLogo(search_provider_logos::LogoObserver* observer) {
  LogoCallbacks callbacks;
  callbacks.on_cached_decoded_logo_available =
      base::BindOnce(ObserverOnLogoAvailable, observer, true);
  callbacks.on_fresh_decoded_logo_available =
      base::BindOnce(ObserverOnLogoAvailable, observer, false);
  GetLogo(std::move(callbacks));
}

void LogoServiceImpl::GetLogo(LogoCallbacks callbacks) {
  if (!template_url_service_) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  if (!template_url) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  GURL logo_url;
  if (command_line->HasSwitch(switches::kSearchProviderLogoURL)) {
    logo_url = GURL(
        command_line->GetSwitchValueASCII(switches::kSearchProviderLogoURL));
  } else {
#if defined(OS_ANDROID)
    // Non-Google default search engine logos are currently enabled only on
    // Android (https://crbug.com/737283).
    logo_url = template_url->logo_url();
#endif
  }

  GURL base_url;
  GURL doodle_url;
  const bool is_google = template_url->url_ref().HasGoogleBaseURLs(
      template_url_service_->search_terms_data());
  if (is_google) {
    // TODO(treib): After features::kUseDdljsonApi has launched, put the Google
    // doodle URL into prepopulated_engines.json.
    base_url =
        GURL(template_url_service_->search_terms_data().GoogleBaseURLValue());
    doodle_url = search_provider_logos::GetGoogleDoodleURL(base_url);
  } else if (base::FeatureList::IsEnabled(features::kThirdPartyDoodles)) {
    // First try to get the Doodle URL from the command line, then from a
    // feature param, and finally the "real" one from TemplateURL.
    if (command_line->HasSwitch(switches::kThirdPartyDoodleURL)) {
      doodle_url = GURL(
          command_line->GetSwitchValueASCII(switches::kThirdPartyDoodleURL));
    } else {
      std::string override_url = base::GetFieldTrialParamValueByFeature(
          features::kThirdPartyDoodles,
          features::kThirdPartyDoodlesOverrideUrlParam);
      if (!override_url.empty()) {
        doodle_url = GURL(override_url);
      } else {
        doodle_url = template_url->doodle_url();
      }
    }
    base_url = doodle_url.GetOrigin();
  }

  if (!logo_url.is_valid() && !doodle_url.is_valid()) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  const bool use_fixed_logo = !doodle_url.is_valid();

  if (!logo_tracker_) {
    std::unique_ptr<LogoCache> logo_cache = std::move(logo_cache_for_test_);
    if (!logo_cache) {
      logo_cache = std::make_unique<LogoCache>(cache_directory_);
    }

    std::unique_ptr<base::Clock> clock = std::move(clock_for_test_);
    if (!clock) {
      clock = std::make_unique<base::DefaultClock>();
    }

    logo_tracker_ = std::make_unique<LogoTracker>(
        request_context_getter_,
        std::make_unique<LogoDelegateImpl>(std::move(image_decoder_)),
        std::move(logo_cache), std::move(clock));
  }

  if (use_fixed_logo) {
    logo_tracker_->SetServerAPI(
        logo_url, base::Bind(&search_provider_logos::ParseFixedLogoResponse),
        base::Bind(&search_provider_logos::UseFixedLogoUrl));
  } else if (is_google) {
    // TODO(treib): Get rid of this Google special case after
    // features::kUseDdljsonApi has launched.
    logo_tracker_->SetServerAPI(
        doodle_url,
        search_provider_logos::GetGoogleParseLogoResponseCallback(base_url),
        search_provider_logos::GetGoogleAppendQueryparamsCallback(
            use_gray_background_));
  } else {
    logo_tracker_->SetServerAPI(
        doodle_url,
        base::Bind(&search_provider_logos::GoogleNewParseLogoResponse,
                   base_url),
        base::Bind(&search_provider_logos::GoogleNewAppendQueryparamsToLogoURL,
                   use_gray_background_));
  }

  logo_tracker_->GetLogo(std::move(callbacks));
}

void LogoServiceImpl::SetLogoCacheForTests(std::unique_ptr<LogoCache> cache) {
  logo_cache_for_test_ = std::move(cache);
}

void LogoServiceImpl::SetClockForTests(std::unique_ptr<base::Clock> clock) {
  clock_for_test_ = std::move(clock);
}

}  // namespace search_provider_logos
