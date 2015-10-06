// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_
#define COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
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
  void Find(int32_t request_id, const mojo::String& search_text);

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
                         const mojo::String& search_text,
                         bool found);

  // Our owner.
  FindControllerDelegate* delegate_;

  // A list of Frames which we have not sent a Find() command to. Initialized
  // in Find(), and read from OnContinueFinding().
  std::deque<Frame*> pending_find_frames_;

  // Current find session number. We keep track of this to prevent recording
  // stale callbacks.
  int current_find_request_;

  // The current callback data from various frames.
  std::map<Frame*, MatchData> returned_find_data_;

  base::WeakPtrFactory<FindController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FindController);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FIND_CONTROLLER_H_
