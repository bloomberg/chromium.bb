// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_HANDLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_HANDLE_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/prerender/prerender_manager.h"

class GURL;

namespace content {
class SessionStorageNamespace;
}

namespace prerender {

class PrerenderContents;

// A class representing a running prerender to a client of the PrerenderManager.
// Methods on PrerenderManager which start prerenders return a caller-owned
// PrerenderHandle* to the client (or NULL if they are unable to start a
// prerender). Because the PrerenderManager can stop running prerenders at any
// time, callers may wish to check PrerenderHandle::IsValid() before operating
// on their prerenders.
class PrerenderHandle : public base::NonThreadSafe {
 public:
  // Before calling the destructor, the caller must invalidate the handle by
  // calling either OnNavigateAway or OnCancel.
  ~PrerenderHandle();

  // The launcher is navigating away from the context that launched this
  // prerender. The prerender will likely stay alive briefly though, in case we
  // are going through a redirect chain that will target it. This call
  // invalidates the handle. If the prerender handle is already invalid, this
  // call does nothing.
  void OnNavigateAway();

  // The launcher has taken explicit action to remove this prerender (for
  // instance, removing a link element from a document). This call invalidates
  // the handle. If the prerender handle is already invalid, this call does
  // nothing.
  void OnCancel();

  // True if the prerender handle is still connected to a (pending or running)
  // prerender. Handles can become invalid through explicit requests by the
  // client, such as calling OnCancel() or OnNavigateAway(), and handles
  // also become invalid when the PrerenderManager cancels prerenders.
  bool IsValid() const;

  // True if this prerender was launched by a page that was itself being
  // prerendered, and so has not yet been started.
  bool IsPending() const;

  // True if this prerender is currently active.
  bool IsPrerendering() const;

  // True if we started a prerender, and it has finished loading.
  bool IsFinishedLoading() const;

 private:
  friend class PrerenderManager;

  explicit PrerenderHandle(PrerenderManager::PrerenderData* prerender_data);

  void SwapPrerenderDataWith(PrerenderHandle* other_prerender_handle);

  base::WeakPtr<PrerenderManager::PrerenderData> prerender_data_;
  base::WeakPtrFactory<PrerenderHandle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHandle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_HANDLE_H_
