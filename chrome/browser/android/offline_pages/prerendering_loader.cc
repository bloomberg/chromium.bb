// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_loader.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace offline_pages {

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
  session_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_context_)));
  content::SessionStorageNamespace* sessionStorageNamespace =
      session_contents_->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size renderWindowSize = session_contents_->GetContainerBounds().size();
  bool accepted = adapter_->StartPrerender(
      browser_context_, url, sessionStorageNamespace, renderWindowSize);
  if (!accepted)
    return false;

  DCHECK(adapter_->IsActive());
  snapshot_controller_.reset(
      new SnapshotController(base::ThreadTaskRunnerHandle::Get(), this));
  callback_ = callback;
  state_ = State::PENDING;
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

void PrerenderingLoader::OnPrerenderStart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(state_ == State::PENDING);
  state_ = State::LOADING;
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
    // Inform SnapshotController of DomContentContent event so it can
    // determine when to consider it really LOADED (e.g., some multiple
    // second delay from this event).
    snapshot_controller_->DocumentAvailableInMainFrame();
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
  // Loading has stopped so unless the Loader has already transistioned to the
  // idle state, clean up the previous request state, transition to the idle
  // state, and post callback.
  // Note: it is possible to receive some asynchronous stopped indication after
  // the request has completed/stopped via another path so the Loader may be
  // idle at this point.

  if (IsIdle())
    return;

  if (adapter_->IsActive()) {
    DVLOG(1) << "Load failed: " << adapter_->GetFinalStatus();
    adapter_->DestroyActive();
  }
  // Request status depends on whether we are still loading (failed) or
  // did load and then loading was stopped (cancel - from prerender stack).
  Offliner::RequestStatus request_status =
      IsLoaded() ? Offliner::RequestStatus::CANCELED
                 : Offliner::RequestStatus::FAILED;
  // TODO(dougarnett): For failure, determine from final status if retry-able
  // and report different failure statuses if retry-able or not.
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
  if (!IsLoaded() && !IsIdle()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback_, Offliner::RequestStatus::CANCELED, nullptr));
  }
  state_ = State::IDLE;
}

}  // namespace offline_pages
