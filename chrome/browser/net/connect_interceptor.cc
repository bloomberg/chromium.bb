// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connect_interceptor.h"

#include "chrome/browser/net/predictor.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

namespace chrome_browser_net {

// We don't bother learning to preconnect via a GET if the original URL
// navigation was so long ago, that a preconnection would have been dropped
// anyway.  We believe most servers will drop the connection in 10 seconds, so
// we currently estimate this time-till-drop at 10 seconds.
// TODO(jar): We should do a persistent field trial to validate/optimize this.
static const int kMaxUnusedSocketLifetimeSecondsWithoutAGet = 10;

ConnectInterceptor::ConnectInterceptor(Predictor* predictor)
    : timed_cache_(base::TimeDelta::FromSeconds(
          kMaxUnusedSocketLifetimeSecondsWithoutAGet)),
      predictor_(predictor) {
  DCHECK(predictor);
}

ConnectInterceptor::~ConnectInterceptor() {
}

void ConnectInterceptor::WitnessURLRequest(net::URLRequest* request) {
  GURL request_scheme_host(Predictor::CanonicalizeUrl(request->url()));
  if (request_scheme_host == GURL::EmptyGURL())
    return;

  // Learn what URLs are likely to be needed during next startup.
  predictor_->LearnAboutInitialNavigation(request_scheme_host);

  bool redirected_host = false;
  if (request->referrer().empty()) {
    if (request->url() != request->original_url()) {
      // This request was completed with a redirect.
      GURL original_scheme_host(request->original_url().GetWithEmptyPath());
      if (request_scheme_host != original_scheme_host) {
        redirected_host = true;
        // Don't learn from redirects that take path as an argument, but do
        // learn from short-hand typing entries, such as "cnn.com" redirects to
        // "www.cnn.com".  We can't just check for has_path(), as a mere "/"
        // will count as a path, so we check that the path is at most a "/"
        // (1 character long) to decide the redirect is "definitive" and has no
        // significant path.
        // TODO(jar): It may be ok to learn from all redirects, as the adaptive
        // system will not respond until several identical redirects have taken
        // place.  Hence a use of a path (that changes) wouldn't really be
        // learned from anyway.
        if (request->original_url().path().length() <= 1 &&
            timed_cache_.WasRecentlySeen(original_scheme_host)) {
          // TODO(jar): These definite redirects could be learned much faster.
          predictor_->LearnFromNavigation(original_scheme_host,
                                          request_scheme_host);
        }
      }
    }
  } else {
    GURL referring_scheme_host = GURL(request->referrer()).GetWithEmptyPath();
    bool is_subresource = !(request->load_flags() & net::LOAD_MAIN_FRAME);
    // Learn about our referring URL, for use in the future.
    if (is_subresource && timed_cache_.WasRecentlySeen(referring_scheme_host))
      predictor_->LearnFromNavigation(referring_scheme_host,
                                      request_scheme_host);
    if (referring_scheme_host == request_scheme_host) {
      // We've already made any/all predictions when we navigated to the
      // referring host, so we can bail out here.
      // We don't update the RecentlySeen() time because any preconnections
      // need to be made at the first navigation (i.e., when referer was loaded)
      // and wouldn't have waited for this current request navigation.
      return;
    }
  }
  timed_cache_.SetRecentlySeen(request_scheme_host);

  // Subresources for main frames usually get predicted when we detected the
  // main frame request - way back in RenderViewHost::Navigate.  So only handle
  // predictions now for subresources or for redirected hosts.
  if ((request->load_flags() & net::LOAD_SUB_FRAME) || redirected_host)
    predictor_->PredictFrameSubresources(request_scheme_host,
                                         request->first_party_for_cookies());
  return;
}

ConnectInterceptor::TimedCache::TimedCache(const base::TimeDelta& max_duration)
    : mru_cache_(UrlMruTimedCache::NO_AUTO_EVICT),
      max_duration_(max_duration) {
}

// Make Clang compilation happy with explicit destructor.
ConnectInterceptor::TimedCache::~TimedCache() {}

bool ConnectInterceptor::TimedCache::WasRecentlySeen(const GURL& url) {
  DCHECK_EQ(url.GetWithEmptyPath(), url);
  // Evict any overly old entries.
  base::TimeTicks now = base::TimeTicks::Now();
  UrlMruTimedCache::reverse_iterator eldest = mru_cache_.rbegin();
  while (!mru_cache_.empty()) {
    DCHECK(eldest == mru_cache_.rbegin());
    if (now - eldest->second < max_duration_)
      break;
    eldest = mru_cache_.Erase(eldest);
  }
  return mru_cache_.end() != mru_cache_.Peek(url);
}

void ConnectInterceptor::TimedCache::SetRecentlySeen(const GURL& url) {
  DCHECK_EQ(url.GetWithEmptyPath(), url);
  mru_cache_.Put(url, base::TimeTicks::Now());
}

}  // namespace chrome_browser_net
