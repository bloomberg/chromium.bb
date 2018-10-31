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
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using search_provider_logos::LogoTracker;

namespace search_provider_logos {
namespace {

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

class LogoServiceImpl::SigninObserver
    : public GaiaCookieManagerService::Observer {
 public:
  using SigninStatusChangedCallback = base::RepeatingClosure;

  SigninObserver(GaiaCookieManagerService* cookie_service,
                 const SigninStatusChangedCallback& callback)
      : cookie_service_(cookie_service), callback_(callback) {
    if (cookie_service_) {
      cookie_service_->AddObserver(this);
    }
  }

  ~SigninObserver() override {
    if (cookie_service_) {
      cookie_service_->RemoveObserver(this);
    }
  }

 private:
  // GaiaCookieManagerService::Observer implementation.
  void OnGaiaAccountsInCookieUpdated(const std::vector<gaia::ListedAccount>&,
                                     const std::vector<gaia::ListedAccount>&,
                                     const GoogleServiceAuthError&) override {
    callback_.Run();
  }

  GaiaCookieManagerService* const cookie_service_;
  SigninStatusChangedCallback callback_;
};

LogoServiceImpl::LogoServiceImpl(
    const base::FilePath& cache_directory,
    GaiaCookieManagerService* cookie_service,
    TemplateURLService* template_url_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<bool()> want_gray_logo_getter)
    : cache_directory_(cache_directory),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      want_gray_logo_getter_(std::move(want_gray_logo_getter)),
      image_decoder_(std::move(image_decoder)),
      signin_observer_(std::make_unique<SigninObserver>(
          cookie_service,
          base::BindRepeating(&LogoServiceImpl::SigninStatusChanged,
                              base::Unretained(this)))) {}

LogoServiceImpl::~LogoServiceImpl() = default;

void LogoServiceImpl::Shutdown() {
  // The GaiaCookieManagerService may be destroyed at any point after Shutdown,
  // so make sure we drop any references to it.
  signin_observer_.reset();
}

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
    // TODO(treib): Put the Google doodle URL into prepopulated_engines.json.
    base_url =
        GURL(template_url_service_->search_terms_data().GoogleBaseURLValue());
    doodle_url = search_provider_logos::GetGoogleDoodleURL(base_url);
  } else {
    if (command_line->HasSwitch(switches::kThirdPartyDoodleURL)) {
      doodle_url = GURL(
          command_line->GetSwitchValueASCII(switches::kThirdPartyDoodleURL));
    } else {
      doodle_url = template_url->doodle_url();
    }
    base_url = doodle_url.GetOrigin();
  }

  if (!logo_url.is_valid() && !doodle_url.is_valid()) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  InitializeLogoTrackerIfNecessary();

  const bool use_fixed_logo = !doodle_url.is_valid();
  if (use_fixed_logo) {
    logo_tracker_->SetServerAPI(
        logo_url, base::Bind(&search_provider_logos::ParseFixedLogoResponse),
        base::Bind(&search_provider_logos::UseFixedLogoUrl));
  } else {
    // We encode the type of doodle (regular or gray) in the URL so that the
    // logo cache gets cleared when that value changes.
    GURL prefilled_url = AppendPreliminaryParamsToDoodleURL(
        want_gray_logo_getter_.Run(), doodle_url);
    logo_tracker_->SetServerAPI(
        prefilled_url,
        base::Bind(&search_provider_logos::ParseDoodleLogoResponse, base_url),
        base::Bind(&search_provider_logos::AppendFingerprintParamToDoodleURL));
  }

  logo_tracker_->GetLogo(std::move(callbacks));
}

void LogoServiceImpl::SetLogoCacheForTests(std::unique_ptr<LogoCache> cache) {
  logo_cache_for_test_ = std::move(cache);
}

void LogoServiceImpl::SetClockForTests(base::Clock* clock) {
  clock_for_test_ = clock;
}

void LogoServiceImpl::InitializeLogoTrackerIfNecessary() {
  if (logo_tracker_) {
    return;
  }

  std::unique_ptr<LogoCache> logo_cache = std::move(logo_cache_for_test_);
  if (!logo_cache) {
    logo_cache = std::make_unique<LogoCache>(cache_directory_);
  }

  base::Clock* clock = clock_for_test_;
  if (!clock) {
    clock = base::DefaultClock::GetInstance();
  }

  logo_tracker_ = std::make_unique<LogoTracker>(url_loader_factory_,
                                                std::move(image_decoder_),
                                                std::move(logo_cache), clock);
}

void LogoServiceImpl::SigninStatusChanged() {
  // Clear any cached logo, since it may be personalized (e.g. birthday Doodle).
  InitializeLogoTrackerIfNecessary();
  logo_tracker_->ClearCachedLogo();
}

}  // namespace search_provider_logos
