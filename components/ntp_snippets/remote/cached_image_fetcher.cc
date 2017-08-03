// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/cached_image_fetcher.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    PrefService* pref_service,
    RemoteSuggestionsDatabase* database)
    : image_fetcher_(std::move(image_fetcher)),
      database_(database),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {
  // |image_fetcher_| can be null in tests.
  if (image_fetcher_) {
    image_fetcher_->SetImageFetcherDelegate(this);
    image_fetcher_->SetDataUseServiceName(
        data_use_measurement::DataUseUserData::NTP_SNIPPETS_THUMBNAILS);
  }
}

CachedImageFetcher::~CachedImageFetcher() {}

void CachedImageFetcher::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    ImageFetchedCallback callback) {
  database_->LoadImage(
      suggestion_id.id_within_category(),
      base::Bind(&CachedImageFetcher::OnImageFetchedFromDatabase,
                 base::Unretained(this), base::Passed(std::move(callback)),
                 suggestion_id, url));
}

// This function gets only called for caching the image data received from the
// network. The actual decoding is done in OnImageDecodedFromDatabase().
void CachedImageFetcher::OnImageDataFetched(
    const std::string& id_within_category,
    const std::string& image_data) {
  if (image_data.empty()) {
    return;
  }
  database_->SaveImage(id_within_category, image_data);
}

void CachedImageFetcher::OnImageDecodingDone(
    ImageFetchedCallback callback,
    const std::string& id_within_category,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  std::move(callback).Run(image);
}

void CachedImageFetcher::OnImageFetchedFromDatabase(
    ImageFetchedCallback callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    std::string data) {  // SnippetImageCallback requires by-value.
  // The image decoder is null in tests.
  if (image_fetcher_->GetImageDecoder() && !data.empty()) {
    image_fetcher_->GetImageDecoder()->DecodeImage(
        data,
        // We're not dealing with multi-frame images.
        /*desired_image_frame_size=*/gfx::Size(),
        base::Bind(&CachedImageFetcher::OnImageDecodedFromDatabase,
                   base::Unretained(this), base::Passed(std::move(callback)),
                   suggestion_id, url));
    return;
  }
  // Fetching from the DB failed; start a network fetch.
  FetchImageFromNetwork(suggestion_id, url, std::move(callback));
}

void CachedImageFetcher::OnImageDecodedFromDatabase(
    ImageFetchedCallback callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    std::move(callback).Run(image);
    return;
  }
  // If decoding the image failed, delete the DB entry.
  database_->DeleteImage(suggestion_id.id_within_category());
  FetchImageFromNetwork(suggestion_id, url, std::move(callback));
}

void CachedImageFetcher::FetchImageFromNetwork(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    ImageFetchedCallback callback) {
  if (url.is_empty() || !thumbnail_requests_throttler_.DemandQuotaForRequest(
                            /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    std::move(callback).Run(gfx::Image());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remote_suggestions_provider", R"(
        semantics {
          sender: "Content Suggestion Thumbnail Fetch"
          description:
            "Retrieves thumbnails for content suggestions, for display on the "
            "New Tab page or Chrome Home."
          trigger:
            "Triggered when the user looks at a content suggestion (and its "
            "thumbnail isn't cached yet)."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Currently not available, but in progress: crbug.com/703684"
        chrome_policy {
          NTPContentSuggestionsEnabled {
            policy_options {mode: MANDATORY}
            NTPContentSuggestionsEnabled: false
          }
        }
      })");
  image_fetcher_->StartOrQueueNetworkRequest(
      suggestion_id.id_within_category(), url,
      base::Bind(&CachedImageFetcher::OnImageDecodingDone,
                 base::Unretained(this), base::Passed(std::move(callback))),
      traffic_annotation);
}

}  // namespace ntp_snippets
