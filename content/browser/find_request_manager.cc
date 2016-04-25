// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/find_request_manager.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"

namespace content {

// static
const int FindRequestManager::kInvalidId = -1;

FindRequestManager::FindRequestManager(WebContentsImpl* web_contents)
    : contents_(web_contents),
      current_session_id_(kInvalidId),
      number_of_matches_(0),
      active_match_ordinal_(0) {}

FindRequestManager::~FindRequestManager() {}

void FindRequestManager::Find(int request_id,
                              const base::string16& search_text,
                              const blink::WebFindOptions& options) {
  // Every find request must have a unique ID, and these IDs must strictly
  // increase so that newer requests always have greater IDs than older
  // requests.
  DCHECK_GT(request_id, current_request_.id);
  DCHECK_GT(request_id, current_session_id_);

  FindRequest request(request_id, search_text, options);

  if (options.findNext) {
    // This is a find next operation.

    // This implies that there is an ongoing find session with the same search
    // text.
    DCHECK_GE(current_session_id_, 0);
    DCHECK_EQ(request.search_text, current_request_.search_text);

    current_request_ = request;
  } else {
    // This is an initial find operation.
    Reset(request);
  }

  SendFindIPC(request, contents_->GetMainFrame());
}

void FindRequestManager::StopFinding(StopFindAction action) {
  SendStopFindingIPC(action, contents_->GetMainFrame());
  current_session_id_ = kInvalidId;
}

void FindRequestManager::OnFindReply(RenderFrameHost* rfh,
                                     int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  // Ignore stale replies from abandoned find sessions.
  if (current_session_id_ == kInvalidId || request_id < current_session_id_)
    return;

  // Update the stored results.
  number_of_matches_ = number_of_matches;
  selection_rect_ = selection_rect;
  active_match_ordinal_ = active_match_ordinal;

  NotifyFindReply(request_id, final_update);
}

#if defined(OS_ANDROID)
void FindRequestManager::ActivateNearestFindResult(float x,
                                                   float y) {
  if (current_session_id_ == kInvalidId)
    return;

  auto rfh = contents_->GetMainFrame();
  rfh->Send(new InputMsg_ActivateNearestFindResult(
      rfh->GetRoutingID(), current_session_id_, x, y));
}

void FindRequestManager::RequestFindMatchRects(int current_version) {
  match_rects_.request_version = current_version;
  SendFindMatchRectsIPC(contents_->GetMainFrame());
}

void FindRequestManager::OnFindMatchRectsReply(
    RenderFrameHost* rfh,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  contents_->NotifyFindMatchRectsReply(version, rects, active_rect);
}
#endif

void FindRequestManager::Reset(const FindRequest& initial_request) {
  current_session_id_ = initial_request.id;
  current_request_ = initial_request;
  number_of_matches_ = 0;
  active_match_ordinal_ = 0;
  selection_rect_ = gfx::Rect();
#if defined(OS_ANDROID)
  match_rects_ = FindMatchRectsState();
#endif
}

void FindRequestManager::SendFindIPC(const FindRequest& request,
                                     RenderFrameHost* rfh) {
  rfh->Send(new FrameMsg_Find(rfh->GetRoutingID(), request.id,
                              request.search_text, request.options));
}

void FindRequestManager::SendStopFindingIPC(StopFindAction action,
                                            RenderFrameHost* rfh) const {
  rfh->Send(new FrameMsg_StopFinding(rfh->GetRoutingID(), action));
}

void FindRequestManager::NotifyFindReply(int request_id,
                                         bool final_update) const {
  if (request_id == kInvalidId) {
    NOTREACHED();
    return;
  }

  contents_->NotifyFindReply(request_id, number_of_matches_, selection_rect_,
                             active_match_ordinal_, final_update);
}

#if defined(OS_ANDROID)
void FindRequestManager::SendFindMatchRectsIPC(RenderFrameHost* rfh) {
  rfh->Send(new FrameMsg_FindMatchRects(rfh->GetRoutingID(),
                                        match_rects_.request_version));
}
#endif

}  // namespace content
