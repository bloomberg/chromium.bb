// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/web_view/public/interfaces/frame.mojom.h"

namespace blink {
class WebView;
}

namespace mus {
class Window;
}

namespace html_viewer {

class DocumentResourceWaiter;
class GlobalState;
class HTMLFrame;
class HTMLFrameDelegate;
class HTMLFrameTreeManagerObserver;

// HTMLFrameTreeManager is responsible for managing the frames that comprise a
// document. Some of the frames may be remote. HTMLFrameTreeManager updates its
// state in response to changes from the server Frame, as well as changes
// from the underlying frames. The frame tree has at least one local frame
// that is backed by a mus::Window.
class HTMLFrameTreeManager {
 public:
  // Returns a new HTMLFrame or null if a HTMLFrame does not need to be created.
  // If this returns non-null the caller owns the return value and must call
  // Close() when done.
  static HTMLFrame* CreateFrameAndAttachToTree(
      GlobalState* global_state,
      mus::Window* window,
      scoped_ptr<DocumentResourceWaiter> resource_waiter,
      HTMLFrameDelegate* delegate);

  // Returns the HTMLFrameTreeManager with the specified root id.
  static HTMLFrameTreeManager* FindFrameTreeWithRoot(uint32_t root_frame_id);

  GlobalState* global_state() { return global_state_; }

  blink::WebView* GetWebView();

  // Ever increasing value that is used to identify the state the tree is in.
  // That is, the value increases every time a structural change is made to
  // the tree, eg add or remove.
  uint32_t change_id() const { return change_id_; }

  void AddObserver(HTMLFrameTreeManagerObserver* observer);
  void RemoveObserver(HTMLFrameTreeManagerObserver* observer);

 private:
  friend class HTMLFrame;
  class ChangeIdAdvancedNotifier;
  using TreeMap = std::map<uint32_t, HTMLFrameTreeManager*>;
  using ChangeIDSet = std::set<uint32_t>;

  explicit HTMLFrameTreeManager(GlobalState* global_state);
  ~HTMLFrameTreeManager();

  void Init(HTMLFrameDelegate* delegate,
            mus::Window* local_window,
            const mojo::Array<web_view::mojom::FrameDataPtr>& frame_data,
            uint32_t change_id);

  // Creates a Frame per FrameData element in |frame_data|. Returns the root.
  HTMLFrame* BuildFrameTree(
      HTMLFrameDelegate* delegate,
      const mojo::Array<web_view::mojom::FrameDataPtr>& frame_data,
      uint32_t local_frame_id,
      mus::Window* local_window);

  // Returns this HTMLFrameTreeManager from |instances_|.
  void RemoveFromInstances();

  // Invoked when a Frame is destroyed.
  void OnFrameDestroyed(HTMLFrame* frame);

  // Call before applying a structure change from the server. |source| is the
  // the source of the change, and |change_id| the id from the server. Returns
  // true if the change should be applied, false otherwise.
  bool PrepareForStructureChange(HTMLFrame* source, uint32_t change_id);

  // Each HTMLFrame delegates FrameClient methods to the HTMLFrameTreeManager
  // the frame is in. HTMLFrameTreeManager only responds to changes from the
  // |local_frame_| (this is because each FrameClient sees the same change, and
  // a change only need be processed once).
  void ProcessOnFrameAdded(HTMLFrame* source,
                           uint32_t change_id,
                           web_view::mojom::FrameDataPtr frame_data);
  void ProcessOnFrameRemoved(HTMLFrame* source,
                             uint32_t change_id,
                             uint32_t frame_id);
  void ProcessOnFrameClientPropertyChanged(HTMLFrame* source,
                                           uint32_t frame_id,
                                           const mojo::String& name,
                                           mojo::Array<uint8_t> new_data);

  // Finds a new local frame which satisfies:
  // - it is not a descendant of |local_frame_|;
  // - it is the highest local frame or one of the highest local frames if
  //   there are multiple.
  HTMLFrame* FindNewLocalFrame();

  static TreeMap* instances_;

  GlobalState* global_state_;

  HTMLFrame* root_;

  // The |local_frame_| is the HTMLFrame that is the highest frame that is
  // local. Please note that it is not necessarily the ancestor of all local
  // frames.
  HTMLFrame* local_frame_;

  uint32_t change_id_;

  // When a frame is locally removed the id is added here. When the server
  // notifies us we remove from this set. This is done to ensure an add/remove
  // followed by notification from the server doesn't attempt to add the frame
  // back that was locally removed.
  ChangeIDSet pending_remove_ids_;

  // If true, we're in ProcessOnFrameRemoved().
  bool in_process_on_frame_removed_;

  base::ObserverList<HTMLFrameTreeManagerObserver> observers_;

  base::WeakPtrFactory<HTMLFrameTreeManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLFrameTreeManager);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_H_
