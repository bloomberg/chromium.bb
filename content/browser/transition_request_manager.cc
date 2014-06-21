// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/transition_request_manager.h"

#include "base/memory/singleton.h"
#include "content/public/browser/browser_thread.h"

namespace content {

bool TransitionRequestManager::HasPendingTransitionRequest(
    int process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(process_id, render_frame_id);
  return (pending_transition_frames_.find(key) !=
          pending_transition_frames_.end());
}

void TransitionRequestManager::SetHasPendingTransitionRequest(
    int process_id,
    int render_frame_id,
    bool has_pending) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(process_id, render_frame_id);
  if (has_pending) {
    pending_transition_frames_.insert(key);
  } else {
    pending_transition_frames_.erase(key);
  }
}

TransitionRequestManager::TransitionRequestManager() {
}

TransitionRequestManager::~TransitionRequestManager() {
}

// static
TransitionRequestManager* TransitionRequestManager::GetInstance() {
  return Singleton<TransitionRequestManager>::get();
}

}  // namespace content
