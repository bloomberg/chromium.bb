// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/android/offline_pages/prerender_adapter.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/snapshot_controller.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace offline_pages {

// A client-side page loader that integrates with the PrerenderManager to do
// the page loading in the background. It operates on a single thread and
// needs to run on BrowserThread::UI to work with the PrerenderManager.
// It supports a single load request at a time.
class PrerenderingLoader : public PrerenderAdapter::Observer,
                           public SnapshotController::Client {
 public:
  // Reports status of a load page request with loaded contents if available.
  typedef base::Callback<void(Offliner::RequestStatus, content::WebContents*)>
      LoadPageCallback;

  typedef base::Callback<void(int64_t)> ProgressCallback;

  explicit PrerenderingLoader(content::BrowserContext* browser_context);
  ~PrerenderingLoader() override;

  // Loads a page in the background if possible and returns whether the
  // request was accepted. If so, the LoadPageCallback will be informed
  // of status. Only one load request may exist as a time. If a previous
  // request is still in progress it must be stopped before a new
  // request will be accepted. The callback may be called more than
  // once - first for a successful load and then if canceled after the
  // load (which may be from resources being reclaimed) at which point
  // the retrieved WebContents should no longer be used.
  virtual bool LoadPage(const GURL& url,
                        const LoadPageCallback& load_done_callback,
                        const ProgressCallback& progress_callback);

  // Stops (completes or cancels) the load request. Must be called when
  // LoadPageCallback is done with consuming the contents. May be called
  // prior to LoadPageCallback in order to cancel the current request (in
  // which case the callback will not be run).
  // This loader should also be responsible for stopping offline
  // prerenders when Chrome is transitioned to foreground.
  virtual void StopLoading();

  // Returns whether the loader is idle and able to accept new LoadPage
  // request.
  virtual bool IsIdle();

  // Returns whether the loader has successfully loaded web contents.
  // Note that |StopLoading()| should be used to clear this state once
  // the loaded web contents are no longer needed.
  virtual bool IsLoaded();

  // Overrides the prerender stack adapter for unit testing.
  void SetAdapterForTesting(
      std::unique_ptr<PrerenderAdapter> prerender_adapter);

  // PrerenderAdapter::Observer implementation:
  void OnPrerenderStopLoading() override;
  void OnPrerenderDomContentLoaded() override;
  void OnPrerenderStop() override;
  void OnPrerenderNetworkBytesChanged(int64_t bytes) override;

  // SnapshotController::Client implementation:
  void StartSnapshot() override;

  // Returns true if the lowbar of snapshotting a page is met.
  virtual bool IsLowbarMet();

 private:
  // State of the loader (only one request may be active at a time).
  enum class State {
    IDLE,     // No active load request.
    PENDING,  // Load request is pending the start of prerendering.
    LOADING,  // Loading in progress.
    LOADED,   // Loaded and now waiting for requestor to StopLoading().
  };

  // Handles some event/signal that the load request has succeeded or failed.
  // It may be due to some asynchronous trigger that occurs after the request
  // has completed for some other reason/event.
  void HandleLoadEvent();

  // Handles some event/signal that loading has stopped (whether due to a
  // failure, cancel, or stop request). It may be due to some asynchronous
  // trigger that occurs after the request has stopped for some other reason.
  void HandleLoadingStopped();

  // Cancels any current prerender and moves loader to idle state.
  void CancelPrerender();

  // Tracks loading state including whether the Loader is idle.
  State state_;

  // Handles determining when to report page is LOADED.
  std::unique_ptr<SnapshotController> snapshot_controller_;

  // Not owned.
  content::BrowserContext* browser_context_;

  // Adapter for handling calls to the prerender stack.
  std::unique_ptr<PrerenderAdapter> adapter_;

  // A WebContents for the active load request that is used to hold the session
  // storage namespace for rendering. This will NOT have the loaded page.
  std::unique_ptr<content::WebContents> session_contents_;

  // Callback to call when the active load request completes, fails, or is
  // canceled.
  LoadPageCallback load_done_callback_;

  // Callback to call when we know more bytes have loaded from the network.
  ProgressCallback progress_callback_;

  // True if the lowbar of snapshotting a page is met.
  bool is_lowbar_met_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingLoader);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_
