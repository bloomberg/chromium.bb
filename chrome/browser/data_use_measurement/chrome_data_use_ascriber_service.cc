// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

data_use_measurement::ChromeDataUseAscriber* InitOnIOThread(
    IOThread* io_thread) {
  DCHECK(io_thread);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return io_thread->globals()->data_use_ascriber.get();
}

}  // namespace

namespace data_use_measurement {

ChromeDataUseAscriberService::ChromeDataUseAscriberService()
    : ascriber_(nullptr), is_initialized_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Skip IO thread initialization if there is no IO thread. This check is
  // required because unit tests that do no set up an IO thread can cause this
  // code to execute.
  if (!g_browser_process->io_thread()) {
    is_initialized_ = true;
    return;
  }

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&InitOnIOThread, g_browser_process->io_thread()),
      base::Bind(&ChromeDataUseAscriberService::SetDataUseAscriber,
                 base::Unretained(this)));
}

ChromeDataUseAscriberService::~ChromeDataUseAscriberService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ChromeDataUseAscriberService::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_initialized_) {
    pending_frames_queue_.push_back(render_frame_host);
    return;
  }

  if (!ascriber_)
    return;

  int parent_render_process_id = -1;
  int parent_render_frame_id = -1;
  if (render_frame_host->GetParent()) {
    parent_render_process_id =
        render_frame_host->GetParent()->GetProcess()->GetID();
    parent_render_frame_id = render_frame_host->GetParent()->GetRoutingID();
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeDataUseAscriber::RenderFrameCreated,
                 base::Unretained(ascriber_),
                 render_frame_host->GetProcess()->GetID(),
                 render_frame_host->GetRoutingID(), parent_render_process_id,
                 parent_render_frame_id));
}

void ChromeDataUseAscriberService::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_initialized_) {
    // While remove() is a O(n) operation, the pending queue is not expected
    // to have a significant number of elements.
    DCHECK_GE(50u, pending_frames_queue_.size());
    pending_frames_queue_.remove(render_frame_host);
    return;
  }

  if (!ascriber_)
    return;

  int parent_render_frame_id = -1;
  int parent_render_process_id = -1;

  if (render_frame_host->GetParent()) {
    parent_render_process_id =
        render_frame_host->GetParent()->GetProcess()->GetID();
    parent_render_frame_id = render_frame_host->GetParent()->GetRoutingID();
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeDataUseAscriber::RenderFrameDeleted,
                 base::Unretained(ascriber_),
                 render_frame_host->GetProcess()->GetID(),
                 render_frame_host->GetRoutingID(), parent_render_process_id,
                 parent_render_frame_id));
}

void ChromeDataUseAscriberService::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!ascriber_)
    return;
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeDataUseAscriber::DidStartMainFrameNavigation,
                 base::Unretained(ascriber_), navigation_handle->GetURL(),
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetMainFrame()->GetRoutingID(),
                 navigation_handle));
}

void ChromeDataUseAscriberService::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  if (!ascriber_)
    return;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ChromeDataUseAscriber::ReadyToCommitMainFrameNavigation,
          base::Unretained(ascriber_), navigation_handle->GetURL(),
          navigation_handle->GetGlobalRequestID(),
          web_contents->GetRenderProcessHost()->GetID(),
          web_contents->GetMainFrame()->GetRoutingID(),
          !navigation_handle->HasCommitted() || navigation_handle->IsSamePage(),
          navigation_handle));
}

void ChromeDataUseAscriberService::SetDataUseAscriber(
    ChromeDataUseAscriber* ascriber) {
  DCHECK(!is_initialized_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ascriber_ = ascriber;
  is_initialized_ = true;

  for (auto& it : pending_frames_queue_) {
    RenderFrameCreated(it);
  }
  pending_frames_queue_.clear();
}

}  // namespace data_use_measurement
