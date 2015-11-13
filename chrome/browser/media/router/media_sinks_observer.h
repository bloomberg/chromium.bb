// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINKS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINKS_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"

namespace media_router {

class MediaRouter;

// Base class for observing when the collection of sinks compatible with
// a MediaSource has been updated.
// A MediaSinksObserver implementation can be registered to MediaRouter to
// receive results. It can then interpret / process the results accordingly.
class MediaSinksObserver {
 public:
  // Constructs an observer that will observe for sinks compatible
  // with |source|.
  MediaSinksObserver(MediaRouter* router, const MediaSource& source);
  virtual ~MediaSinksObserver();

  // Registers with MediaRouter to start observing. Must be called before the
  // observer will start receiving updates. Returns |true| if the observer is
  // initialized. This method is no-op if the observer is already initialized.
  bool Init();

  // This function is invoked when the list of sinks compatible
  // with |source_| has been updated.
  // Implementations may not perform operations that modify the Media Router's
  // observer list. In particular, invoking this observer's destructor within
  // OnSinksReceived will result in undefined behavior.
  virtual void OnSinksReceived(const std::vector<MediaSink>& sinks) {}

  const MediaSource& source() const { return source_; }

 private:
  const MediaSource source_;
  MediaRouter* router_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(MediaSinksObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINKS_OBSERVER_H_
