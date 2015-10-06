// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/find_controller.h"

#include "base/bind.h"
#include "components/web_view/find_controller_delegate.h"
#include "components/web_view/frame.h"

namespace web_view {

FindController::FindController(FindControllerDelegate* delegate)
    : delegate_(delegate), current_find_request_(-1), weak_ptr_factory_(this) {}

FindController::~FindController() {}

void FindController::Find(int32_t request_id, const mojo::String& search_text) {
  // TODO(erg): While this deals with multiple frames, it does not deal with
  // going forward or backwards. To do that, we'll have to port all frame
  // traversal and focusing concepts from blink::WebFrame to mojo::Frame.

  // TODO(erg): This isn't great and causes flashes on character
  // entry. However, it's needed for now because the internals of TextFinder
  // still track the entire state of the blink frame tree, and if there are any
  // frames that have marked text, doing a find clears the results of all
  // frames _except_ for the first frame that it finds a result on.
  StopFinding();

  // TODO(erg): This cheap method does not traverse in the order that blink
  // does.
  pending_find_frames_ = delegate_->GetAllFrames();

  current_find_request_ = request_id;
  returned_find_data_.clear();

  // Prime the continue loop.
  OnContinueFinding(request_id, search_text, false);
}

void FindController::StopFinding() {
  // Don't report any callbacks that we get after this.
  current_find_request_ = -1;

  for (Frame* f : delegate_->GetAllFrames())
    f->StopFinding(true);
}

void FindController::OnFindInFrameCountUpdated(int32_t request_id,
                                               Frame* frame,
                                               int32_t count,
                                               bool final_update) {
  if (request_id != current_find_request_)
    return;

  auto it = returned_find_data_.find(frame);
  if (it == returned_find_data_.end()) {
    NOTREACHED();
    return;
  }

  it->second.count = count;
  it->second.final_update = final_update;

  int merged_count = 0;
  bool merged_final_update = true;
  for (auto const& data : returned_find_data_) {
    merged_count += data.second.count;
    merged_final_update = merged_final_update && data.second.final_update;
  }

  // We can now take the individual FindInFrame messages and construct a
  // FindInPage message.
  delegate_->GetWebViewClient()->FindInPageMatchCountUpdated(
      request_id, merged_count, merged_final_update);
}

void FindController::OnFindInPageSelectionUpdated(
    int32_t request_id,
    Frame* frame,
    int32_t active_match_ordinal) {
  if (request_id != current_find_request_)
    return;

  // TODO(erg): This is the one that's really hard. To give an accurate count
  // here, we need to have all the results for frames that are before the Frame
  // that contains the selected match so we can add their sums together.
  //
  // Thankfully, we don't have to worry about this now. Since there aren't
  // back/forward controls yet, active_match_ordinal will always be 1.
  delegate_->GetWebViewClient()->FindInPageSelectionUpdated(
      request_id, active_match_ordinal);
}

void FindController::DidDestroyFrame(Frame* frame) {
  auto it =
      find(pending_find_frames_.begin(), pending_find_frames_.end(), frame);
  if (it != pending_find_frames_.end())
    pending_find_frames_.erase(it);
}

void FindController::OnContinueFinding(int32_t request_id,
                                       const mojo::String& search_text,
                                       bool found) {
  if (!found && !pending_find_frames_.empty()) {
    // No match found, search on the next frame.
    Frame* next_frame = pending_find_frames_.front();
    pending_find_frames_.pop_front();
    next_frame->Find(
        request_id, search_text,
        base::Bind(&FindController::OnContinueFinding,
                   weak_ptr_factory_.GetWeakPtr(), request_id, search_text));

    // TODO(erg): This doesn't deal with wrapping around the document at the
    // end when there are multiple frames.
    return;
  }

  pending_find_frames_.clear();

  // We either found a match or we got the final rejection. Either way, we
  // alert our caller.

  // If nothing is found, set result to "0 of 0", otherwise, set it to
  // "-1 of 1" to indicate that we found at least one item, but we don't know
  // yet what is active.
  int ordinal = found ? -1 : 0;     // -1 here means, we might know more later.
  int match_count = found ? 1 : 0;  // 1 here means possibly more coming.

  // If we find no matches then this will be our last status update.
  // Otherwise the scoping effort will send more results.
  bool final_status_update = !found;

  // Send priming messages.
  delegate_->GetWebViewClient()->FindInPageSelectionUpdated(request_id,
                                                            ordinal);
  delegate_->GetWebViewClient()->FindInPageMatchCountUpdated(
      request_id, match_count, final_status_update);

  // TODO(erg): This doesn't iterate in the same order as the current code
  // because we don't have the correct iteration primitives.
  std::deque<Frame*> frames = delegate_->GetAllFrames();
  for (Frame* f : frames) {
    f->StopHighlightingFindResults();

    if (found) {
      MatchData& match_data = returned_find_data_[f];
      match_data.count = 0;
      match_data.final_update = false;
      f->HighlightFindResults(request_id, search_text, true);
    }
  }
}

}  // namespace web_view
