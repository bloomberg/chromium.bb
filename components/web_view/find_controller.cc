// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/find_controller.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/web_view/find_controller_delegate.h"
#include "components/web_view/frame.h"

namespace web_view {

FindController::FindController(FindControllerDelegate* delegate)
    : delegate_(delegate),
      find_request_id_counter_(0),
      current_find_request_id_(-1),
      frame_with_selection_(0),
      weak_ptr_factory_(this) {}

FindController::~FindController() {}

void FindController::Find(const std::string& in_search_string,
                          bool forward_direction) {
  TRACE_EVENT2("web_view", "FindController::Find",
               "search_string", in_search_string,
               "forward_direction", forward_direction);
  std::string search_string = in_search_string;
  // Remove the carriage return character, which generally isn't in web content.
  const char kInvalidChars[] = {'\r', 0};
  base::RemoveChars(search_string, kInvalidChars, &search_string);

  previous_find_text_ = find_text_;

  // TODO(erg): Do we need to something like |find_op_aborted_|?

  find_frames_in_order_ = delegate_->GetAllFrames();

  uint32_t starting_frame;
  bool continue_last_find =
      find_text_ == search_string || search_string.empty();
  if (!continue_last_find) {
    current_find_request_id_ = find_request_id_counter_++;
    frame_with_selection_ = 0;
    returned_find_data_.clear();
    starting_frame = find_frames_in_order_.front()->id();
  } else {
    starting_frame = frame_with_selection_;
  }

  if (!search_string.empty())
    find_text_ = search_string;

  LocalFindOptions options;
  options.forward = forward_direction;
  options.continue_last_find = continue_last_find;
  // Prime the continue loop.
  OnContinueFinding(current_find_request_id_, search_string, options,
                    starting_frame, 0u, false);
}

void FindController::StopFinding() {
  // Don't report any callbacks that we get after this.
  current_find_request_id_ = -1;
  frame_with_selection_ = 0;

  for (Frame* f : delegate_->GetAllFrames())
    f->StopFinding(true);
}

void FindController::OnFindInFrameCountUpdated(int32_t request_id,
                                               Frame* frame,
                                               int32_t count,
                                               bool final_update) {
  if (request_id != current_find_request_id_)
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
  if (request_id != current_find_request_id_)
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
      find(find_frames_in_order_.begin(), find_frames_in_order_.end(), frame);
  if (it != find_frames_in_order_.end())
    find_frames_in_order_.erase(it);
}

void FindController::OnContinueFinding(int32_t request_id,
                                       const std::string& search_string,
                                       LocalFindOptions options,
                                       uint32_t starting_frame,
                                       uint32_t current_frame,
                                       bool found) {
  TRACE_EVENT2("web_view", "FindController::OnContinueFinding",
               "request_id", request_id,
               "search_string", search_string);
  if (!found) {
    // So we need to figure out what the next frame to search is.
    Frame* next_frame =
        GetNextFrameToSearch(starting_frame, current_frame, options.forward);

    // If we have one more frame to search:
    if (next_frame) {
      bool wrap_within_frame = find_frames_in_order_.size() == 1;
      next_frame->Find(
          request_id, mojo::String::From(search_string),
          mojom::FindOptions::From(options), wrap_within_frame,
          base::Bind(&FindController::OnContinueFinding,
                     weak_ptr_factory_.GetWeakPtr(), request_id, search_string,
                     options, starting_frame, next_frame->id()));

      // TODO(erg): This doesn't deal with wrapping around the document at the
      // end when there are multiple frames and the last frame doesn't have a
      // result.
      return;
    }
  }

  frame_with_selection_ = found ? current_frame : 0;

  if (!options.continue_last_find) {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = found ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = found ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !found;

    // Send priming messages.
    delegate_->GetWebViewClient()->FindInPageSelectionUpdated(request_id,
                                                              ordinal);
    delegate_->GetWebViewClient()->FindInPageMatchCountUpdated(
        request_id, match_count, final_status_update);

    std::vector<Frame*> frames = delegate_->GetAllFrames();
    for (Frame* f : frames) {
      f->StopHighlightingFindResults();

      if (found) {
        MatchData& match_data = returned_find_data_[f];
        match_data.count = 0;
        match_data.final_update = false;
        f->HighlightFindResults(request_id, mojo::String::From(search_string),
                                mojom::FindOptions::From(options), true);
      }
    }
  }
}

Frame* FindController::GetNextFrameToSearch(uint32_t starting_frame,
                                            uint32_t current_frame,
                                            bool forward) {
  std::vector<Frame*>::iterator it;
  if (current_frame == 0u) {
    it = GetFrameIteratorById(starting_frame);
    CHECK(it != find_frames_in_order_.end());
    return *it;
  }

  Frame* candidate;
  it = GetFrameIteratorById(current_frame);
  if (it == find_frames_in_order_.end()) {
    // Our current frame has been deleted. There is nothing to be done now; we
    // will abort.
    return nullptr;
  }

  if (forward) {
    ++it;
    if (it == find_frames_in_order_.end())
      it = find_frames_in_order_.begin();
    candidate = *it;
  } else {
    if (it == find_frames_in_order_.begin()) {
      candidate = find_frames_in_order_.back();
    } else {
      --it;
      candidate = *it;
    }
  }

  // If we've looped around the entire frame tree, then we are done and return
  // null.
  if (candidate->id() == starting_frame)
    return nullptr;
  return candidate;
}

std::vector<Frame*>::iterator FindController::GetFrameIteratorById(
    uint32_t frame_id) {
  return find_if(find_frames_in_order_.begin(),
                 find_frames_in_order_.end(),
                 [&frame_id](Frame* f) {
                   return f->id() == frame_id;
                 });
}

}  // namespace web_view
