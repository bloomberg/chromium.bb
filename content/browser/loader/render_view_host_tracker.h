// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RENDER_VIEW_HOST_TRACKER_H_
#define CONTENT_BROWSER_LOADER_RENDER_VIEW_HOST_TRACKER_H_

#include <set>

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_observer.h"

namespace content {

// The ResourceDispatcherHost needs to know when renderers are created and
// destroyed. That happens on the UI thread, but the ResourceDispatcherHost
// operates on the IO thread. RenderViewHostTracker listens for renderer
// notifications on the UI thread, then bounces them over to the IO thread so
// the ResourceDispatcherHost can be notified.
class CONTENT_EXPORT RenderViewHostTracker {
 public:
  RenderViewHostTracker();
  virtual ~RenderViewHostTracker();

 private:
  // TODO(phajdan.jr): Move this declaration of inner class to the .cc file.
  class Observer : public RenderViewHostObserver {
   public:
    Observer(RenderViewHost* rvh,
             RenderViewHostTracker* tracker);
    virtual ~Observer();

   private:
    // RenderViewHostObserver interface:
    virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE;

    RenderViewHostTracker* tracker_;
  };

  friend class Observer;
  typedef std::set<Observer*> ObserverSet;

  void RenderViewHostCreated(RenderViewHost* rvh);

  void RemoveObserver(Observer* observer);

  RenderViewHost::CreatedCallback rvh_created_callback_;
  ObserverSet observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RENDER_VIEW_HOST_TRACKER_H_
