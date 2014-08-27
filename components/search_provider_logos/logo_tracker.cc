// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_tracker.h"

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace search_provider_logos {

namespace {

const int64 kMaxDownloadBytes = 1024 * 1024;

//const int kDecodeLogoTimeoutSeconds = 30;

// Returns whether the metadata for the cached logo indicates that the logo is
// OK to show, i.e. it's not expired or it's allowed to be shown temporarily
// after expiration.
bool IsLogoOkToShow(const LogoMetadata& metadata, base::Time now) {
  base::TimeDelta offset =
      base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS * 3 / 2);
  base::Time distant_past = now - offset;
  base::Time distant_future = now + offset;
  // Sanity check so logos aren't accidentally cached forever.
  if (metadata.expiration_time < distant_past ||
      metadata.expiration_time > distant_future) {
    return false;
  }
  return metadata.can_show_after_expiration || metadata.expiration_time >= now;
}

// Reads the logo from the cache and returns it. Returns NULL if the cache is
// empty, corrupt, expired, or doesn't apply to the current logo URL.
scoped_ptr<EncodedLogo> GetLogoFromCacheOnFileThread(LogoCache* logo_cache,
                                              const GURL& logo_url,
                                              base::Time now) {
  const LogoMetadata* metadata = logo_cache->GetCachedLogoMetadata();
  if (!metadata)
    return scoped_ptr<EncodedLogo>();

  if (metadata->source_url != logo_url.spec() ||
      !IsLogoOkToShow(*metadata, now)) {
    logo_cache->SetCachedLogo(NULL);
    return scoped_ptr<EncodedLogo>();
  }

  return logo_cache->GetCachedLogo().Pass();
}

void DeleteLogoCacheOnFileThread(LogoCache* logo_cache) {
  delete logo_cache;
}

}  // namespace

LogoTracker::LogoTracker(
    base::FilePath cached_logo_directory,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<base::TaskRunner> background_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_ptr<LogoDelegate> delegate)
    : is_idle_(true),
      is_cached_logo_valid_(false),
      logo_delegate_(delegate.Pass()),
      logo_cache_(new LogoCache(cached_logo_directory)),
      clock_(new base::DefaultClock()),
      file_task_runner_(file_task_runner),
      background_task_runner_(background_task_runner),
      request_context_getter_(request_context_getter),
      weak_ptr_factory_(this) {}

LogoTracker::~LogoTracker() {
  ReturnToIdle();
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteLogoCacheOnFileThread, logo_cache_));
  logo_cache_ = NULL;
}

void LogoTracker::SetServerAPI(
    const GURL& logo_url,
    const ParseLogoResponse& parse_logo_response_func,
    const AppendFingerprintToLogoURL& append_fingerprint_func) {
  if (logo_url == logo_url_)
    return;

  ReturnToIdle();

  logo_url_ = logo_url;
  parse_logo_response_func_ = parse_logo_response_func;
  append_fingerprint_func_ = append_fingerprint_func;
}

void LogoTracker::GetLogo(LogoObserver* observer) {
  DCHECK(!logo_url_.is_empty());
  logo_observers_.AddObserver(observer);

  if (is_idle_) {
    is_idle_ = false;
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(),
        FROM_HERE,
        base::Bind(&GetLogoFromCacheOnFileThread,
                   logo_cache_,
                   logo_url_,
                   clock_->Now()),
        base::Bind(&LogoTracker::OnCachedLogoRead,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (is_cached_logo_valid_) {
    observer->OnLogoAvailable(cached_logo_.get(), true);
  }
}

void LogoTracker::RemoveObserver(LogoObserver* observer) {
  logo_observers_.RemoveObserver(observer);
}

void LogoTracker::SetLogoCacheForTests(scoped_ptr<LogoCache> cache) {
  DCHECK(cache);
  file_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteLogoCacheOnFileThread, logo_cache_));
  logo_cache_ = cache.release();
}

void LogoTracker::SetClockForTests(scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();
}

void LogoTracker::ReturnToIdle() {
  // Cancel the current asynchronous operation, if any.
  fetcher_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset state.
  is_idle_ = true;
  cached_logo_.reset();
  is_cached_logo_valid_ = false;

  // Clear obsevers.
  FOR_EACH_OBSERVER(LogoObserver, logo_observers_, OnObserverRemoved());
  logo_observers_.Clear();
}

void LogoTracker::OnCachedLogoRead(scoped_ptr<EncodedLogo> cached_logo) {
  DCHECK(!is_idle_);

  if (cached_logo) {
    logo_delegate_->DecodeUntrustedImage(
        cached_logo->encoded_image,
        base::Bind(&LogoTracker::OnCachedLogoAvailable,
                   weak_ptr_factory_.GetWeakPtr(),
                   cached_logo->metadata));
  } else {
    OnCachedLogoAvailable(LogoMetadata(), SkBitmap());
  }
}

void LogoTracker::OnCachedLogoAvailable(const LogoMetadata& metadata,
                                        const SkBitmap& image) {
  DCHECK(!is_idle_);

  if (!image.isNull()) {
    cached_logo_.reset(new Logo());
    cached_logo_->metadata = metadata;
    cached_logo_->image = image;
  }
  is_cached_logo_valid_ = true;
  Logo* logo = cached_logo_.get();
  FOR_EACH_OBSERVER(LogoObserver, logo_observers_, OnLogoAvailable(logo, true));
  FetchLogo();
}

void LogoTracker::SetCachedLogo(scoped_ptr<EncodedLogo> logo) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LogoCache::SetCachedLogo,
                 base::Unretained(logo_cache_),
                 base::Owned(logo.release())));
}

void LogoTracker::SetCachedMetadata(const LogoMetadata& metadata) {
  file_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LogoCache::UpdateCachedLogoMetadata,
                                         base::Unretained(logo_cache_),
                                         metadata));
}

void LogoTracker::FetchLogo() {
  DCHECK(!fetcher_);
  DCHECK(!is_idle_);

  GURL url;
  if (cached_logo_ && !cached_logo_->metadata.fingerprint.empty() &&
      cached_logo_->metadata.expiration_time >= clock_->Now()) {
    url = append_fingerprint_func_.Run(logo_url_,
                                       cached_logo_->metadata.fingerprint);
  } else {
    url = logo_url_;
  }

  fetcher_.reset(net::URLFetcher::Create(url, net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(request_context_getter_.get());
  fetcher_->Start();
}

void LogoTracker::OnFreshLogoParsed(scoped_ptr<EncodedLogo> logo) {
  DCHECK(!is_idle_);

  if (logo)
    logo->metadata.source_url = logo_url_.spec();

  if (!logo || !logo->encoded_image.get()) {
    OnFreshLogoAvailable(logo.Pass(), SkBitmap());
  } else {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaulated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image = logo->encoded_image;
    logo_delegate_->DecodeUntrustedImage(
        encoded_image,
        base::Bind(&LogoTracker::OnFreshLogoAvailable,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&logo)));
  }
}

void LogoTracker::OnFreshLogoAvailable(scoped_ptr<EncodedLogo> encoded_logo,
                                       const SkBitmap& image) {
  DCHECK(!is_idle_);

  if (encoded_logo && !encoded_logo->encoded_image.get() && cached_logo_ &&
      !encoded_logo->metadata.fingerprint.empty() &&
      encoded_logo->metadata.fingerprint ==
          cached_logo_->metadata.fingerprint) {
    // The cached logo was revalidated, i.e. its fingerprint was verified.
    SetCachedMetadata(encoded_logo->metadata);
  } else if (encoded_logo && image.isNull()) {
    // Image decoding failed. Do nothing.
  } else {
    scoped_ptr<Logo> logo;
    // Check if the server returned a valid, non-empty response.
    if (encoded_logo) {
      DCHECK(!image.isNull());
      logo.reset(new Logo());
      logo->metadata = encoded_logo->metadata;
      logo->image = image;
    }

    // Notify observers if a new logo was fetched, or if the new logo is NULL
    // but the cached logo was non-NULL.
    if (logo || cached_logo_) {
      FOR_EACH_OBSERVER(LogoObserver,
                        logo_observers_,
                        OnLogoAvailable(logo.get(), false));
      SetCachedLogo(encoded_logo.Pass());
    }
  }

  ReturnToIdle();
}

void LogoTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!is_idle_);
  scoped_ptr<net::URLFetcher> cleanup_fetcher(fetcher_.release());

  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200)) {
    ReturnToIdle();
    return;
  }

  scoped_ptr<std::string> response(new std::string());
  source->GetResponseAsString(response.get());
  base::Time response_time = clock_->Now();

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          parse_logo_response_func_, base::Passed(&response), response_time),
      base::Bind(&LogoTracker::OnFreshLogoParsed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void LogoTracker::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                             int64 current,
                                             int64 total) {
  if (total > kMaxDownloadBytes || current > kMaxDownloadBytes) {
    LOG(WARNING) << "Search provider logo exceeded download size limit";
    ReturnToIdle();
  }
}

}  // namespace search_provider_logos
