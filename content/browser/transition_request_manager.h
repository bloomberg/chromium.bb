// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_

#include <set>
#include <utility>

#include "base/basictypes.h"
#include "content/common/content_export.h"

template <typename T>
struct DefaultSingletonTraits;

namespace content {

// TransitionRequestManager is used to handle bookkeeping for transition
// requests and responses.
//
// TransitionRequestManager is a singleton and should only be accessed on the IO
// thread.
//
class TransitionRequestManager {
 public:
  // Returns the singleton instance.
  CONTENT_EXPORT static TransitionRequestManager* GetInstance();

  // Returns whether the RenderFrameHost specified by the given IDs currently
  // has a pending transition request. If so, we will have to delay the
  // response until the embedder resumes the request.
  bool HasPendingTransitionRequest(int process_id, int render_frame_id);

  // Sets whether the RenderFrameHost specified by the given IDs currently has a
  // pending transition request.
  CONTENT_EXPORT void SetHasPendingTransitionRequest(int process_id,
                                                     int render_frame_id,
                                                     bool has_pending);

 private:
  friend struct DefaultSingletonTraits<TransitionRequestManager>;
  typedef std::set<std::pair<int, int> > RenderFrameSet;

  TransitionRequestManager();
  ~TransitionRequestManager();

  // Set of (render_process_host_id, render_frame_id) pairs of all
  // RenderFrameHosts that have pending transition requests. Used to pass
  // information to the CrossSiteResourceHandler without doing a round-trip
  // between IO->UI->IO threads.
  RenderFrameSet pending_transition_frames_;

  DISALLOW_COPY_AND_ASSIGN(TransitionRequestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_
