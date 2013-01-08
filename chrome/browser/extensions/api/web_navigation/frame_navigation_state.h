// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "googleurl/src/gurl.h"

namespace content {
class RenderViewHost;
}

namespace extensions {

// Tracks the navigation state of all frames in a given tab currently known to
// the webNavigation API. It is mainly used to track in which frames an error
// occurred so no further events for this frame are being sent.
class FrameNavigationState {
 public:
  // A frame is uniquely identified by its frame ID and the RVH it's in.
  struct FrameID {
    FrameID();
    FrameID(int64 frame_num, content::RenderViewHost* render_view_host);

    bool operator<(const FrameID& other) const;
    bool operator==(const FrameID& other) const;
    bool operator!=(const FrameID& other) const;

    int64 frame_num;
    content::RenderViewHost* render_view_host;
  };
  typedef std::set<FrameID>::const_iterator const_iterator;

  FrameNavigationState();
  ~FrameNavigationState();

  // Use these to iterate over all frame IDs known by this object.
  const_iterator begin() const { return frame_ids_.begin(); }
  const_iterator end() const { return frame_ids_.end(); }

  // True if navigation events for the given frame can be sent.
  bool CanSendEvents(FrameID frame_id) const;

  // True if in general webNavigation events may be sent for the given URL.
  bool IsValidUrl(const GURL& url) const;

  // Starts to track a frame identified by its |frame_id| showing the URL |url|.
  void TrackFrame(FrameID frame_id,
                  FrameID parent_frame_id,
                  const GURL& url,
                  bool is_main_frame,
                  bool is_error_page,
                  bool is_iframe_srcdoc);

  // Stops tracking all frames but the frame with |id_to_skip| for a given
  // RenderViewHost.
  void StopTrackingFramesInRVH(content::RenderViewHost* render_view_host,
                               FrameID id_to_skip);

  // Update the URL associated with a given frame.
  void UpdateFrame(FrameID frame_id, const GURL& url);

  // Returns true if |frame_id| is a known frame.
  bool IsValidFrame(FrameID frame_id) const;

  // Returns the URL corresponding to a tracked frame given by its |frame_id|.
  GURL GetUrl(FrameID frame_id) const;

  // True if the frame given by its |frame_id| is a main frame of its tab.
  // There might be multiple uncomitted main frames.
  bool IsMainFrame(FrameID frame_id) const;

  // Returns the frame ID of the last comitted main frame, or -1 if the frame
  // ID is not known.
  FrameID GetMainFrameID() const;

  // Get the parent frame ID (or an invalid ID, if |frame_id| is a main frame).
  FrameID GetParentFrameID(FrameID frame_id) const;

  // Marks a frame as in an error state, i.e. the onErrorOccurred event was
  // fired for this frame, and no further events should be sent for it.
  void SetErrorOccurredInFrame(FrameID frame_id);

  // True if the frame is marked as being in an error state.
  bool GetErrorOccurredInFrame(FrameID frame_id) const;

  // Marks a frame as having finished its last navigation, i.e. the onCompleted
  // event was fired for this frame.
  void SetNavigationCompleted(FrameID frame_id);

  // True if the frame is currently not navigating.
  bool GetNavigationCompleted(FrameID frame_id) const;

  // Marks a frame as having committed its navigation, i.e. the onCommitted
  // event was fired for this frame.
  void SetNavigationCommitted(FrameID frame_id);

  // True if the frame has committed its navigation.
  bool GetNavigationCommitted(FrameID frame_id) const;

  // Marks a frame as redirected by the server.
  void SetIsServerRedirected(FrameID frame_id);

  // True if the frame was redirected by the server.
  bool GetIsServerRedirected(FrameID frame_id) const;

#ifdef UNIT_TEST
  static void set_allow_extension_scheme(bool allow_extension_scheme) {
    allow_extension_scheme_ = allow_extension_scheme;
  }
#endif

 private:
  struct FrameState {
    FrameState();

    bool error_occurred;  // True if an error has occurred in this frame.
    bool is_main_frame;  // True if this is a main frame.
    bool is_iframe_srcdoc;  // True if the frame is displaying its srcdoc.
    bool is_navigating;  // True if there is a navigation going on.
    bool is_committed;  // True if the navigation is already committed.
    bool is_server_redirected;  // True if a server redirect happened.
    int64 parent_frame_num;
    GURL url;  // URL of this frame.
  };
  typedef std::map<FrameID, FrameState> FrameIdToStateMap;

  // Tracks the state of known frames.
  FrameIdToStateMap frame_state_map_;

  // Set of all known frames.
  std::set<FrameID> frame_ids_;

  // The id of the last comitted main frame.
  FrameID main_frame_id_;

  // If true, also allow events from chrome-extension:// URLs.
  static bool allow_extension_scheme_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
