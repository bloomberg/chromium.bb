// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_tab_helper.h"

#include <algorithm>

#include "chrome/common/automation_messages.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/size.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(AutomationTabHelper);

TabEventObserver::TabEventObserver() { }

TabEventObserver::~TabEventObserver() {
  for (size_t i = 0; i < event_sources_.size(); ++i) {
    if (event_sources_[i])
      event_sources_[i]->RemoveObserver(this);
  }
}

void TabEventObserver::StartObserving(AutomationTabHelper* tab_helper) {
  tab_helper->AddObserver(this);
  event_sources_.push_back(tab_helper->AsWeakPtr());
}

void TabEventObserver::StopObserving(AutomationTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
  EventSourceVector::iterator iter =
      std::find(event_sources_.begin(), event_sources_.end(), tab_helper);
  if (iter != event_sources_.end())
    event_sources_.erase(iter);
}

AutomationTabHelper::AutomationTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_loading_(false) {
}

AutomationTabHelper::~AutomationTabHelper() { }

void AutomationTabHelper::AddObserver(TabEventObserver* observer) {
  observers_.AddObserver(observer);
}

void AutomationTabHelper::RemoveObserver(TabEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AutomationTabHelper::SnapshotEntirePage() {
  Send(new AutomationMsg_SnapshotEntirePage(routing_id()));
}

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
void AutomationTabHelper::HeapProfilerDump(const std::string& reason) {
  Send(new AutomationMsg_HeapProfilerDump(routing_id(), reason));
}
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

bool AutomationTabHelper::has_pending_loads() const {
  return is_loading_ || !pending_client_redirects_.empty();
}

void AutomationTabHelper::DidStartLoading(
    content::RenderViewHost* render_view_host) {
  if (is_loading_) {
    // DidStartLoading is often called twice. Once when the renderer sends a
    // load start message, and once when the browser calls it directly as a
    // result of some user-initiated navigation.
    VLOG(1) << "Received DidStartLoading while loading already started.";
    return;
  }
  bool had_pending_loads = has_pending_loads();
  is_loading_ = true;
  if (!had_pending_loads) {
    FOR_EACH_OBSERVER(TabEventObserver, observers_,
                      OnFirstPendingLoad(web_contents()));
  }
}

void AutomationTabHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  if (!is_loading_) {
    LOG(WARNING) << "Received DidStopLoading while loading already stopped.";
    return;
  }
  is_loading_ = false;
  if (!has_pending_loads()) {
    FOR_EACH_OBSERVER(TabEventObserver, observers_,
                      OnNoMorePendingLoads(web_contents()));
  }
}

void AutomationTabHelper::RenderViewGone(base::TerminationStatus status) {
  OnTabOrRenderViewDestroyed(web_contents());
}

void AutomationTabHelper::WebContentsDestroyed(WebContents* web_contents) {
  OnTabOrRenderViewDestroyed(web_contents);
}

void AutomationTabHelper::OnTabOrRenderViewDestroyed(
    WebContents* web_contents) {
  if (has_pending_loads()) {
    is_loading_ = false;
    pending_client_redirects_.clear();
    FOR_EACH_OBSERVER(TabEventObserver, observers_,
                      OnNoMorePendingLoads(web_contents));
  }
}

void AutomationTabHelper::OnSnapshotEntirePageACK(
    bool success,
    const std::vector<unsigned char>& png_data,
    const std::string& error_msg) {
  FOR_EACH_OBSERVER(TabEventObserver, observers_,
                    OnSnapshotEntirePageACK(success, png_data, error_msg));
}

bool AutomationTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool msg_is_good = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AutomationTabHelper, message, msg_is_good)
    IPC_MESSAGE_HANDLER(AutomationMsg_SnapshotEntirePageACK,
                        OnSnapshotEntirePageACK)
    IPC_MESSAGE_HANDLER(AutomationMsg_WillPerformClientRedirect,
                        OnWillPerformClientRedirect)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidCompleteOrCancelClientRedirect,
                        OnDidCompleteOrCancelClientRedirect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  if (!msg_is_good) {
    LOG(ERROR) << "Failed to deserialize an IPC message";
  }
  return handled;
}

void AutomationTabHelper::OnWillPerformClientRedirect(
    int64 frame_id, double delay_seconds) {
  // Ignore all non-zero delays.
  // TODO(kkania): Handle timed redirects.
  if (delay_seconds > 0) {
    LOG(WARNING) << "Ignoring timed redirect scheduled for " << delay_seconds
                 << " seconds later. Will not wait for the redirect to occur";
    return;
  }

  bool first_pending_load = !has_pending_loads();
  pending_client_redirects_.insert(frame_id);
  if (first_pending_load) {
    FOR_EACH_OBSERVER(TabEventObserver, observers_,
                      OnFirstPendingLoad(web_contents()));
  }
}

void AutomationTabHelper::OnDidCompleteOrCancelClientRedirect(int64 frame_id) {
  std::set<int64>::iterator iter =
      pending_client_redirects_.find(frame_id);
  // It is possible that we did not track the redirect becasue it had a non-zero
  // delay. See the comment in |OnWillPerformClientRedirect|.
  if (iter != pending_client_redirects_.end()) {
    pending_client_redirects_.erase(iter);
    if (!has_pending_loads()) {
      FOR_EACH_OBSERVER(TabEventObserver, observers_,
                        OnNoMorePendingLoads(web_contents()));
    }
  }
}
