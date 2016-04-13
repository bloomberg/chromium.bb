// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_

#include <set>
#include <vector>

#include "content/public/common/stop_find_action.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

class RenderFrameHost;
class WebContentsImpl;

// FindRequestManager manages all of the find-in-page requests/replies
// initiated/received through a WebContents. It coordinates searching across
// multiple (potentially out-of-process) frames, handles the aggregation of find
// results from each frame, and facilitates active match traversal. It is
// instantiated once per WebContents, and is owned by that WebContents.
//
// TODO(paulmeyer): FindRequestManager is currently incomplete and does not do
// all of these things yet, but will soon.
class FindRequestManager {
 public:
  explicit FindRequestManager(WebContentsImpl* web_contents);
  ~FindRequestManager();

  // Initiates a find operation for |search_text| with the options specified in
  // |options|. |request_id| uniquely identifies the find request.
  void Find(int request_id,
            const base::string16& search_text,
            const blink::WebFindOptions& options);

  // Stops the active find session and clears the general highlighting of the
  // matches. |action| determines whether the last active match (if any) will be
  // activated, cleared, or remain highlighted.
  void StopFinding(StopFindAction action);

  // Called when a reply is received from a frame with the results from a
  // find request.
  void OnFindReply(RenderFrameHost* rfh,
                   int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);

#if defined(OS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x,y) defined in
  // find-in-page coordinates.
  void ActivateNearestFindResult(float x, float y);

  // Requests the rects of the current find matches from the renderer process.
  void RequestFindMatchRects(int current_version);

  // Called when a reply is received in response to a request for find match
  // rects.
  void OnFindMatchRectsReply(RenderFrameHost* rfh,
                             int version,
                             const std::vector<gfx::RectF>& rects,
                             const gfx::RectF& active_rect);
#endif

 private:
  // An invalid ID. This value is invalid for any render process ID, render
  // frame ID, or find request ID.
  static const int kInvalidId;

  // The request data for a single find request.
  struct FindRequest {
    // The find request ID that uniquely identifies this find request.
    int id = kInvalidId;

    // The text that is being searched for in this find request.
    base::string16 search_text;

    // The set of find options in effect for this find request.
    blink::WebFindOptions options;

    FindRequest() = default;
    FindRequest(int id,
                const base::string16& search_text,
                const blink::WebFindOptions& options)
        : id(id), search_text(search_text), options(options) {}
  };

  // Send a find IPC containing the find request |request| to the RenderFrame
  // associated with |rfh|.
  void SendFindIPC(const FindRequest& request, RenderFrameHost* rfh);

  // Send a stop finding IPC to the RenderFrame associated with |rfh|.
  void SendStopFindingIPC(StopFindAction action, RenderFrameHost* rfh) const;

  // Reset all of the per-session state for a new find-in-page session.
  void Reset(const FindRequest& initial_request);

  // Send the find results (as they currently are) to the WebContents.
  void NotifyFindReply(int request_id, bool final_update) const;

#if defined(OS_ANDROID)
  // Request the latest find match rects from a frame.
  void SendFindMatchRectsIPC(RenderFrameHost* rfh);

  // State related to FindMatchRects requests.
  struct FindMatchRectsState {
    // The latest find match rects version known by the requester.
    int request_version = kInvalidId;
  } match_rects_;
#endif

  // The WebContents that owns this FindRequestManager.
  WebContentsImpl* const contents_;

  // The request ID of the initial find request in the current find-in-page
  // session, which uniquely identifies this session. Request IDs are included
  // in all find-related IPCs, which allows reply IPCs containing results from
  // previous sessions (with |request_id| < |current_session_id_|) to be easily
  // identified and ignored.
  int current_session_id_;

  // The current find request.
  FindRequest current_request_;

  // The total number of matches found in the current find-in-page session.
  int number_of_matches_;

  // The overall active match ordinal for the current find-in-page session.
  int active_match_ordinal_;

  // The rectangle around the active match, in screen coordinates.
  gfx::Rect selection_rect_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_
