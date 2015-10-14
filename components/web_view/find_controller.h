// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_
#define COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "components/web_view/local_find_options.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"

namespace web_view {

class FindControllerDelegate;
class Frame;

// Contains all the find code used by WebViewImpl.
class FindController {
 public:
  FindController(FindControllerDelegate* delegate);
  ~FindController();

  // Starts a find session looking for |search_text|. This method will first
  // scan through frames one by one to look for the first instance of
  // |search_text|, and return the data through
  // OnFindInPageSelectionUpdated(). If found, it will highlight all instances
  // of the text and report the final total count through
  // FindInPageMatchCountUpdated().
  void Find(const std::string& search_text, bool forward_direction);

  // Unhighlights all find instances on the page.
  void StopFinding();

  void OnFindInFrameCountUpdated(int32_t request_id,
                                 Frame* frame,
                                 int32_t count,
                                 bool final_update);
  void OnFindInPageSelectionUpdated(int32_t request_id,
                                    Frame* frame,
                                    int32_t active_match_ordinal);

  void DidDestroyFrame(Frame* frame);

 private:
  struct MatchData {
    int count;
    bool final_update;
  };

  // Callback method invoked by Find().
  void OnContinueFinding(int32_t request_id,
                         const std::string& search_string,
                         LocalFindOptions options,
                         uint32_t starting_frame,
                         uint32_t current_frame,
                         bool found);

  // Returns the next Frame that we want to Find() on. This method will start
  // iterating, forward or backwards, from current_frame, and will return
  // nullptr once it has gone entirely around the framelist and has returned to
  // starting_frame. If |current_frame| doesn't exist, returns nullptr to
  // terminate the search process.
  Frame* GetNextFrameToSearch(uint32_t starting_frame,
                              uint32_t current_frame,
                              bool forward);

  // Returns an iterator into |find_frames_in_order_|. Implementation detail of
  // GetNextFrameToSearch().
  std::vector<Frame*>::iterator GetFrameIteratorById(uint32_t frame_id);

  // Our owner.
  FindControllerDelegate* delegate_;

  // A list of Frames in traversal order to be searched.
  std::vector<Frame*> find_frames_in_order_;

  // Monotonically increasing find id.
  int find_request_id_counter_;

  // Current find session number. We keep track of this to prevent recording
  // stale callbacks.
  int current_find_request_id_;

  // The current string we are/just finished searching for. This is used to
  // figure out if this is a Find or a FindNext operation (FindNext should not
  // increase the request id).
  std::string find_text_;

  // The string we searched for before |find_text_|.
  std::string previous_find_text_;

  // The current callback data from various frames.
  std::map<Frame*, MatchData> returned_find_data_;

  // We keep track of the current frame with the selection.
  uint32_t frame_with_selection_;

  base::WeakPtrFactory<FindController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FindController);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_
