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
// prerender). Calls on the handle of a prerender that is not running at no-ops.
// Destroying a handle before a prerender starts will prevent it from ever
// starting. Destroying a handle while a prerendering is running will stop the
// prerender, without making any calls to the observer.
class PrerenderHandle : public base::NonThreadSafe,
                        public PrerenderContents::Observer {
 public:
  class Observer {
   public:
    // Signals that the prerender has started running.
    virtual void OnPrerenderStart(PrerenderHandle* handle) = 0;

    // Signals that the prerender has had its load event.
    virtual void OnPrerenderStopLoading(PrerenderHandle* handle) = 0;

    // Signals that the prerender has had its 'DOMContentLoaded' event.
    virtual void OnPrerenderDomContentLoaded(PrerenderHandle* handle) = 0;

    // Signals that the prerender has stopped running.
    virtual void OnPrerenderStop(PrerenderHandle* handle) = 0;

    // Signals that this prerender has just become a MatchComplete replacement.
    virtual void OnPrerenderCreatedMatchCompleteReplacement(
        PrerenderHandle* handle) = 0;

   protected:
    Observer();
    virtual ~Observer();
  };

  // Before calling the destructor, the caller must invalidate the handle by
  // calling either OnNavigateAway or OnCancel.
  virtual ~PrerenderHandle();

  void SetObserver(Observer* observer);

  // The launcher is navigating away from the context that launched this
  // prerender. The prerender will likely stay alive briefly though, in case we
  // are going through a redirect chain that will target it.
  void OnNavigateAway();

  // The launcher has taken explicit action to remove this prerender (for
  // instance, removing a link element from a document). This call invalidates
  // the handle. If the prerender handle is already invalid, this call does
  // nothing.
  void OnCancel();

  // True if this prerender is currently active.
  bool IsPrerendering() const;

  // True if we started a prerender, and it has finished loading.
  bool IsFinishedLoading() const;

  // True if the prerender is currently active, but is abandoned.
  bool IsAbandoned() const;

  PrerenderContents* contents() const;

  // Returns whether the prerender matches the URL provided.
  bool Matches(
      const GURL& url,
      const content::SessionStorageNamespace* session_storage_namespace) const;

  // Returns whether this PrerenderHandle represents the same prerender as
  // the other PrerenderHandle object specified.
  bool RepresentingSamePrerenderAs(PrerenderHandle* other) const;

  // Retrieves the SessionStorageNamespace of the underlying prerender, if
  // available.
  content::SessionStorageNamespace* GetSessionStorageNamespace() const;

  // Returns the child id of the prerender.
  int GetChildId() const;

 private:
  friend class PrerenderManager;

  explicit PrerenderHandle(PrerenderManager::PrerenderData* prerender_data);

  // From PrerenderContents::Observer:
  virtual void OnPrerenderStart(PrerenderContents* prerender_contents) OVERRIDE;
  virtual void OnPrerenderStopLoading(PrerenderContents* prerender_contents)
      OVERRIDE;
  virtual void OnPrerenderDomContentLoaded(
      PrerenderContents* prerender_contents) OVERRIDE;
  virtual void OnPrerenderStop(PrerenderContents* prerender_contents) OVERRIDE;
  virtual void OnPrerenderCreatedMatchCompleteReplacement(
      PrerenderContents* contents, PrerenderContents* replacement) OVERRIDE;

  Observer* observer_;

  base::WeakPtr<PrerenderManager::PrerenderData> prerender_data_;
  base::WeakPtrFactory<PrerenderHandle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHandle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_HANDLE_H_
