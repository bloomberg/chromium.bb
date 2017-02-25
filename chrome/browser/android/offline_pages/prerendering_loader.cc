// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_loader.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Whether to report DomContentLoaded event to the snapshot controller.
bool kConsiderDclForSnapshot = false;
// The delay to wait for snapshotting after DomContentLoaded event if
// kConsiderDclForSnapshot is true.
long kOfflinePageDclDelayMs = 25000;
// The delay to wait for snapshotting after OnLoad event.
long kOfflinePageOnloadDelayMs = 2000;
}  // namespace


namespace offline_pages {


// Classifies the appropriate RequestStatus for for the given prerender
// FinalStatus.
Offliner::RequestStatus ClassifyFinalStatus(
    prerender::FinalStatus final_status) {
  switch (final_status) {
    // Identify aborted/canceled operations.

    case prerender::FINAL_STATUS_CANCELLED:
    // TODO(dougarnett): Reconsider if/when get better granularity (642768)
    case prerender::FINAL_STATUS_UNSUPPORTED_SCHEME:
      return Offliner::LOADING_CANCELED;

    // Identify non-retryable failures. These are a hard type failures
    // associated with the page and so are expected to occur again if retried.

    case prerender::FINAL_STATUS_SAFE_BROWSING:
    case prerender::FINAL_STATUS_CREATING_AUDIO_STREAM:
    case prerender::FINAL_STATUS_JAVASCRIPT_ALERT:
    case prerender::FINAL_STATUS_CREATE_NEW_WINDOW:
    case prerender::FINAL_STATUS_INVALID_HTTP_METHOD:
    case prerender::FINAL_STATUS_OPEN_URL:
      return Offliner::RequestStatus::LOADING_FAILED_NO_RETRY;

    // Identify failures that indicate we should stop further processing
    // for now. These may be current resource issues or app closing.

    case prerender::FINAL_STATUS_MEMORY_LIMIT_EXCEEDED:
    case prerender::FINAL_STATUS_RATE_LIMIT_EXCEEDED:
    case prerender::FINAL_STATUS_RENDERER_CRASHED:
    case prerender::FINAL_STATUS_TOO_MANY_PROCESSES:
    case prerender::FINAL_STATUS_TIMED_OUT:
    case prerender::FINAL_STATUS_APP_TERMINATING:
    case prerender::FINAL_STATUS_PROFILE_DESTROYED:
      return Offliner::RequestStatus::LOADING_FAILED_NO_NEXT;

    // Otherwise, assume retryable failure.

    case prerender::FINAL_STATUS_NEW_NAVIGATION_ENTRY:
    case prerender::FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    case prerender::FINAL_STATUS_SSL_ERROR:
    case prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED:
    case prerender::FINAL_STATUS_WINDOW_PRINT:
    default:
      return Offliner::RequestStatus::LOADING_FAILED;
  }
}

PrerenderingLoader::PrerenderingLoader(content::BrowserContext* browser_context)
    : state_(State::IDLE),
      snapshot_controller_(nullptr),
      browser_context_(browser_context),
      is_lowbar_met_(false) {
  adapter_.reset(new PrerenderAdapter(this));
}

PrerenderingLoader::~PrerenderingLoader() {
  CancelPrerender();
}

bool PrerenderingLoader::LoadPage(const GURL& url,
                                  const LoadPageCallback& load_done_callback,
                                  const ProgressCallback& progress_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsIdle()) {
    DVLOG(1)
        << "WARNING: Existing request in progress or waiting for StopLoading()";
    return false;
  }

  // Create a WebContents instance to define and hold a SessionStorageNamespace
  // for this load request.
  DCHECK(!session_contents_.get());
  std::unique_ptr<content::WebContents> new_web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context_)));
  content::SessionStorageNamespace* sessionStorageNamespace =
      new_web_contents->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size renderWindowSize = new_web_contents->GetContainerBounds().size();
  bool accepted = adapter_->StartPrerender(
      browser_context_, url, sessionStorageNamespace, renderWindowSize);
  if (!accepted)
    return false;

  DCHECK(adapter_->IsActive());
  snapshot_controller_.reset(
      new SnapshotController(base::ThreadTaskRunnerHandle::Get(), this,
                             kOfflinePageDclDelayMs,
                             kOfflinePageOnloadDelayMs));
  load_done_callback_ = load_done_callback;
  progress_callback_ = progress_callback;
  session_contents_.swap(new_web_contents);
  state_ = State::LOADING;
  return true;
}

void PrerenderingLoader::StopLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelPrerender();
}

bool PrerenderingLoader::IsIdle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return state_ == State::IDLE;
}

bool PrerenderingLoader::IsLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return state_ == State::LOADED;
}

void PrerenderingLoader::SetAdapterForTesting(
    std::unique_ptr<PrerenderAdapter> prerender_adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  adapter_ = std::move(prerender_adapter);
}

void PrerenderingLoader::OnPrerenderStopLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!IsIdle());
  DCHECK(adapter_->GetWebContents());
  // Inform SnapshotController of OnLoad event so it can determine
  // when to consider it really LOADED.
  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
}

void PrerenderingLoader::OnPrerenderDomContentLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!IsIdle());
  if (!adapter_->GetWebContents()) {
    // Without a WebContents object at this point, we are done.
    HandleLoadingStopped();
  } else {
    is_lowbar_met_ = true;
    if (kConsiderDclForSnapshot) {
      // Inform SnapshotController of DomContentLoaded event so it can
      // determine when to consider it really LOADED (e.g., some multiple
      // second delay from this event).
      snapshot_controller_->DocumentAvailableInMainFrame();
    }
  }
}

void PrerenderingLoader::OnPrerenderStop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleLoadingStopped();
}

void PrerenderingLoader::OnPrerenderNetworkBytesChanged(int64_t bytes) {
  if (state_ == State::LOADING)
    progress_callback_.Run(bytes);
}

void PrerenderingLoader::StartSnapshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleLoadEvent();
}

bool PrerenderingLoader::IsLowbarMet() {
  return is_lowbar_met_;
}

void PrerenderingLoader::HandleLoadEvent() {
  // If still loading, check if the load succeeded or not, then update
  // the internal state (LOADED for success or IDLE for failure) and post
  // callback.
  // Note: it is possible to receive a load event (e.g., if timeout-based)
  // after the request has completed via another path (e.g., canceled) so
  // the Loader may be idle at this point.

  if (IsIdle() || IsLoaded())
    return;

  content::WebContents* web_contents = adapter_->GetWebContents();
  if (web_contents) {
    state_ = State::LOADED;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(load_done_callback_,
                              Offliner::RequestStatus::LOADED, web_contents));
  } else {
    // No WebContents means that the load failed (and it stopped).
    HandleLoadingStopped();
  }
}

void PrerenderingLoader::HandleLoadingStopped() {
  // Loading has stopped so unless the Loader has already transitioned to the
  // idle state, clean up the previous request state, transition to the idle
  // state, and post callback.
  // Note: it is possible to receive some asynchronous stopped indication after
  // the request has completed/stopped via another path so the Loader may be
  // idle at this point.

  if (IsIdle())
    return;

  Offliner::RequestStatus request_status;

  if (adapter_->IsActive()) {
    if (IsLoaded()) {
      // If page already loaded, then prerender is telling us that it is
      // canceling (and we should stop using the loaded WebContents).
      request_status = Offliner::RequestStatus::LOADING_CANCELED;
    } else {
      // Otherwise, get the available FinalStatus to classify the outcome.
      prerender::FinalStatus final_status = adapter_->GetFinalStatus();
      DVLOG(1) << "Load failed: " << final_status;
      request_status = ClassifyFinalStatus(final_status);

      // Loss of network connection can show up as unsupported scheme per
      // a redirect to a special data URL is used to navigate to error page.
      // Capture the current connectivity here in case we can leverage that
      // to differentiate how to treat it.
      if (final_status == prerender::FINAL_STATUS_UNSUPPORTED_SCHEME) {
        UMA_HISTOGRAM_ENUMERATION(
            "OfflinePages.Background.UnsupportedScheme.ConnectionType",
            net::NetworkChangeNotifier::GetConnectionType(),
            net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      }
    }

    // Now clean up the active prerendering operation detail.
    adapter_->DestroyActive();
  } else {
    // No access to FinalStatus so classify as retryable failure.
    request_status = Offliner::RequestStatus::LOADING_FAILED;
  }

  snapshot_controller_.reset(nullptr);
  session_contents_.reset(nullptr);
  state_ = State::IDLE;
  is_lowbar_met_ = false;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(load_done_callback_, request_status, nullptr));
}

void PrerenderingLoader::CancelPrerender() {
  if (adapter_->IsActive()) {
    adapter_->DestroyActive();
  }
  snapshot_controller_.reset(nullptr);
  session_contents_.reset(nullptr);
  state_ = State::IDLE;
  is_lowbar_met_ = false;
}

}  // namespace offline_pages
