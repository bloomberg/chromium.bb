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
long kOfflinePageDclDelayMs = 25000;
long kOfflinePageOnloadDelayMs = 2000;
}  // namespace


namespace offline_pages {


// Classifies the appropriate RequestStatus for for the given prerender
// FinalStatus.
Offliner::RequestStatus ClassifyFinalStatus(
    prerender::FinalStatus final_status) {
  switch (final_status) {

    // Identify aborted/canceled operations

    case prerender::FINAL_STATUS_CANCELLED:
    // TODO(dougarnett): Reconsider if/when get better granularity (642768)
    case prerender::FINAL_STATUS_UNSUPPORTED_SCHEME:
      return Offliner::PRERENDERING_CANCELED;

    // Identify non-retryable failues.

    case prerender::FINAL_STATUS_SAFE_BROWSING:
    case prerender::FINAL_STATUS_CREATING_AUDIO_STREAM:
    case prerender::FINAL_STATUS_JAVASCRIPT_ALERT:
      return Offliner::RequestStatus::PRERENDERING_FAILED_NO_RETRY;

    // Otherwise, assume retryable failure.
    default:
      return Offliner::RequestStatus::PRERENDERING_FAILED;
  }
}

PrerenderingLoader::PrerenderingLoader(content::BrowserContext* browser_context)
    : state_(State::IDLE),
      snapshot_controller_(nullptr),
      browser_context_(browser_context) {
  adapter_.reset(new PrerenderAdapter(this));
}

PrerenderingLoader::~PrerenderingLoader() {
  CancelPrerender();
}

bool PrerenderingLoader::LoadPage(const GURL& url,
                                  const LoadPageCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsIdle()) {
    DVLOG(1)
        << "WARNING: Existing request in progress or waiting for StopLoading()";
    return false;
  }
  if (!CanPrerender())
    return false;

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
  callback_ = callback;
  session_contents_.swap(new_web_contents);
  state_ = State::LOADING;
  return true;
}

void PrerenderingLoader::StopLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelPrerender();
}

bool PrerenderingLoader::CanPrerender() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return adapter_->CanPrerender();
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
  }
}

void PrerenderingLoader::OnPrerenderStop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleLoadingStopped();
}

void PrerenderingLoader::StartSnapshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleLoadEvent();
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
        FROM_HERE,
        base::Bind(callback_, Offliner::RequestStatus::LOADED, web_contents));
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
      request_status = Offliner::RequestStatus::PRERENDERING_CANCELED;
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
    request_status = Offliner::RequestStatus::PRERENDERING_FAILED;
  }

  snapshot_controller_.reset(nullptr);
  session_contents_.reset(nullptr);
  state_ = State::IDLE;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback_, request_status, nullptr));
}

void PrerenderingLoader::CancelPrerender() {
  if (adapter_->IsActive()) {
    adapter_->DestroyActive();
  }
  snapshot_controller_.reset(nullptr);
  session_contents_.reset(nullptr);
  state_ = State::IDLE;
}

}  // namespace offline_pages
