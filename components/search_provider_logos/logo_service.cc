// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_service.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/fixed_logo_api.h"
#include "components/search_provider_logos/google_logo_api.h"
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

}  // namespace

LogoService::LogoService(
    const base::FilePath& cache_directory,
    TemplateURLService* template_url_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    bool use_gray_background)
    : cache_directory_(cache_directory),
      template_url_service_(template_url_service),
      request_context_getter_(request_context_getter),
      use_gray_background_(use_gray_background),
      image_decoder_(std::move(image_decoder)) {}

LogoService::~LogoService() = default;

void LogoService::GetLogo(search_provider_logos::LogoObserver* observer) {
  if (!template_url_service_) {
    observer->OnObserverRemoved();
    return;
  }

  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  if (!template_url) {
    observer->OnObserverRemoved();
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  GURL logo_url;
  if (command_line->HasSwitch(switches::kSearchProviderLogoURL)) {
    logo_url = GURL(
        command_line->GetSwitchValueASCII(switches::kSearchProviderLogoURL));
  } else {
    logo_url = template_url->logo_url();
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
    if (command_line->HasSwitch(switches::kThirdPartyDoodleURL)) {
      doodle_url = GURL(
          command_line->GetSwitchValueASCII(switches::kThirdPartyDoodleURL));
    } else {
      doodle_url = template_url->doodle_url();
    }
    base_url = doodle_url.GetOrigin();
  }

  if (!logo_url.is_valid() && !doodle_url.is_valid()) {
    observer->OnObserverRemoved();
    return;
  }

  const bool use_fixed_logo = !doodle_url.is_valid();

  if (!logo_tracker_) {
    logo_tracker_ = base::MakeUnique<LogoTracker>(
        cache_directory_, request_context_getter_,
        base::MakeUnique<LogoDelegateImpl>(std::move(image_decoder_)));
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

  logo_tracker_->GetLogo(observer);
}

}  // namespace search_provider_logos
