// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDER_ADAPTER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDER_ADAPTER_H_

#include <memory>

#include "chrome/browser/prerender/prerender_handle.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
class SessionStorageNamespace;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace offline_pages {

// An adapter for managing a prerendering request for offlining. This handles
// all calls into the prerender stack and tracks the active prerender handle.
class PrerenderAdapter : public prerender::PrerenderHandle::Observer {
 public:
  // An observer of PrerenderHandle events that does not expose the handle.
  class Observer {
   public:
    // Signals that the prerender has had its load event.
    virtual void OnPrerenderStopLoading() = 0;

    // Signals that the prerender has had its 'DOMContentLoaded' event.
    virtual void OnPrerenderDomContentLoaded() = 0;

    // Signals that the prerender has stopped running and any retrieved
    // WebContents (via |GetWebContents()|) have become invalidated.
    virtual void OnPrerenderStop() = 0;

    // Signals that a resource finished loading and altered the running byte
    // count. |bytes| is the cumulative number of bytes received for this
    // handle.
    virtual void OnPrerenderNetworkBytesChanged(int64_t bytes) = 0;

   protected:
    Observer();
    virtual ~Observer();
  };

  explicit PrerenderAdapter(PrerenderAdapter::Observer* observer);
  ~PrerenderAdapter() override;

  // Starts prerendering |url| for offlining. There must be no active
  // prerender request when calling this. Returns whether it was able
  // to start the prerendering operation.
  virtual bool StartPrerender(
      content::BrowserContext* browser_context,
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Returns a pointer to the prerendered WebContents. This should only be
  // called once prerendering observer events indicate content is loaded.
  // It may be used for snapshotting the page. The caller does NOT get
  // ownership on the contents and must call |DestroyActive()|
  // to report when it no longer needs the web contents. The caller should
  // watch for its |PrerenderAdapter::Observer::OnPrerenderStop()| to
  // learn that the web contents should no longer be used.
  virtual content::WebContents* GetWebContents() const;

  // Returns the final status of prerendering.
  virtual prerender::FinalStatus GetFinalStatus() const;

  // Returns whether this adapter has an active prerender request. This
  // adapter supports one request at a time. DestroyActive() may be used
  // to clear an active request (which will allow StartPrerender() to be
  // called to request a new one).
  virtual bool IsActive() const;

  // Cancels any current prerendering operation and destroys its local handle.
  virtual void DestroyActive();

  // PrerenderHandle::Observer interface:
  void OnPrerenderStart(prerender::PrerenderHandle* handle) override;
  void OnPrerenderStopLoading(prerender::PrerenderHandle* handle) override;
  void OnPrerenderDomContentLoaded(prerender::PrerenderHandle* handle) override;
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override;
  void OnPrerenderNetworkBytesChanged(
      prerender::PrerenderHandle* handle) override;

 private:
  // At most one prerender request may be active for this adapter and this
  // holds its handle if one is active.
  std::unique_ptr<prerender::PrerenderHandle> active_handle_;

  // Observer of active handle events. Not owned.
  PrerenderAdapter::Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderAdapter);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDER_ADAPTER_H_
