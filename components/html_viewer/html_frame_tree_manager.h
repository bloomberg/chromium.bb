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
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"

namespace blink {
class WebView;
}

namespace mojo {
class ApplicationImpl;
class View;
}

namespace html_viewer {

class DocumentResourceWaiter;
class GlobalState;
class HTMLFrame;
class HTMLFrameDelegate;

// HTMLFrameTreeManager is responsible for managing the frames that comprise a
// document. Some of the frames may be remote. HTMLFrameTreeManager updates its
// state in response to changes from the FrameTreeServer, as well as changes
// from the underlying frames. The frame tree has at least one local frame
// that is backed by a mojo::View.
class HTMLFrameTreeManager {
 public:
  // Creates a new HTMLFrame. The caller owns the return value and must call
  // Close() when done.
  static HTMLFrame* CreateFrameAndAttachToTree(
      GlobalState* global_state,
      mojo::ApplicationImpl* app,
      mojo::View* view,
      scoped_ptr<DocumentResourceWaiter> resource_waiter,
      HTMLFrameDelegate* delegate);

  GlobalState* global_state() { return global_state_; }

  blink::WebView* GetWebView();

 private:
  friend class HTMLFrame;
  using TreeMap = std::map<uint32_t, HTMLFrameTreeManager*>;
  using ChangeIDSet = std::set<uint32_t>;

  explicit HTMLFrameTreeManager(GlobalState* global_state);
  ~HTMLFrameTreeManager();

  void Init(HTMLFrameDelegate* delegate,
            mojo::View* local_view,
            const mojo::Array<mandoline::FrameDataPtr>& frame_data,
            uint32_t change_id);

  // Creates a Frame per FrameData element in |frame_data|. Returns the root.
  HTMLFrame* BuildFrameTree(
      HTMLFrameDelegate* delegate,
      const mojo::Array<mandoline::FrameDataPtr>& frame_data,
      uint32_t local_frame_id,
      mojo::View* local_view);

  // Returns this HTMLFrameTreeManager from |instances_|.
  void RemoveFromInstances();

  // Invoked when a Frame is destroyed.
  void OnFrameDestroyed(HTMLFrame* frame);

  // Call before applying a structure change from the server. |source| is the
  // the source of the change, and |change_id| the id from the server. Returns
  // true if the change should be applied, false otherwise.
  bool PrepareForStructureChange(HTMLFrame* source, uint32_t change_id);

  // Each HTMLFrame delegates FrameTreeClient methods to the
  // HTMLFrameTreeManager the frame is in. HTMLFrameTreeManager only responds
  // to changes from the |local_root_| (this is because each FrameTreeClient
  // sees the same change, and a change only need be processed once).
  void ProcessOnFrameAdded(HTMLFrame* source,
                           uint32_t change_id,
                           mandoline::FrameDataPtr frame_data);
  void ProcessOnFrameRemoved(HTMLFrame* source,
                             uint32_t change_id,
                             uint32_t frame_id);
  void ProcessOnFrameClientPropertyChanged(HTMLFrame* source,
                                           uint32_t frame_id,
                                           const mojo::String& name,
                                           mojo::Array<uint8_t> new_data);

  static TreeMap* instances_;

  GlobalState* global_state_;

  HTMLFrame* root_;

  // The |local_root_| is the HTMLFrame that is the highest frame that is
  // local.
  HTMLFrame* local_root_;

  uint32_t change_id_;

  // When a frame is locally removed the id is added here. When the server
  // notifies us we remove from this set. This is done to ensure an add/remove
  // followed by notification from the server doesn't attempt to add the frame
  // back that was locally removed.
  ChangeIDSet pending_remove_ids_;

  // If true, we're in ProcessOnFrameRemoved().
  bool in_process_on_frame_removed_;

  base::WeakPtrFactory<HTMLFrameTreeManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLFrameTreeManager);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_H_
