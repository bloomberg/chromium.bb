// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_media_sinks_observer.h"

#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_source.h"
#include "content/public/browser/presentation_screen_availability_listener.h"

namespace media_router {

PresentationMediaSinksObserver::PresentationMediaSinksObserver(
    MediaRouter* router,
    content::PresentationScreenAvailabilityListener* listener,
    const MediaSource& source,
    const url::Origin& origin)
    : MediaSinksObserver(router, source, origin),
      listener_(listener),
      previous_availablity_(UNKNOWN) {
  DCHECK(router);
  DCHECK(listener_);
}

PresentationMediaSinksObserver::~PresentationMediaSinksObserver() {
}

void PresentationMediaSinksObserver::OnSinksReceived(
    const std::vector<MediaSink>& result) {
  Availability current_availability = result.empty() ? UNAVAILABLE : AVAILABLE;

  DVLOG(1) << "PresentationMediaSinksObserver::OnSinksReceived: "
           << source().ToString() << " "
           << (result.empty() ? "unavailable" : "available");

  // Don't send if new result is same as previous.
  if (previous_availablity_ != current_availability)
    listener_->OnScreenAvailabilityChanged(current_availability == AVAILABLE);

  previous_availablity_ = current_availability;
}

}  // namespace media_router
