// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_TREE_H_
#define COMPONENTS_WEB_VIEW_FRAME_TREE_H_

#include "base/time/time.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/web_view/frame.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "url/gurl.h"

namespace mojo {
class String;
}

namespace web_view {

class FrameTreeDelegate;
class FrameUserData;

namespace mojom {
class FrameClient;
}

// FrameTree manages the set of Frames that comprise a single url. FrameTree
// owns the root Frame and each Frame owns its children. Frames are
// automatically deleted and removed from the tree if the corresponding view is
// deleted. This happens if the creator of the view deletes it (say an iframe is
// destroyed).
class FrameTree {
 public:
  // |view| is the view to do the initial embedding in. It is assumed |view|
  // outlives FrameTree.
  // |client_properties| is the client properties for the root frame.
  // |root_app_id| is a unique identifier of the app providing |root_client|.
  // See Frame for details on app id's.
  FrameTree(uint32_t root_app_id,
            mus::Window* view,
            mojo::ViewTreeClientPtr view_tree_client,
            FrameTreeDelegate* delegate,
            mojom::FrameClient* root_client,
            scoped_ptr<FrameUserData> user_data,
            const Frame::ClientPropertyMap& client_properties,
            base::TimeTicks navigation_start_time);
  ~FrameTree();

  const Frame* root() const { return root_; }
  Frame* root() { return root_; }
  uint32_t change_id() const { return change_id_; }

 private:
  friend class Frame;

  // Creates a new Frame parented to |parent|. The Frame is considered shared in
  // that it is sharing the FrameClient/Frame of |parent|. There may or may not
  // be a View identified by |frame_id| yet. See Frame for details.
  Frame* CreateChildFrame(Frame* parent,
                          mojo::InterfaceRequest<mojom::Frame> frame_request,
                          mojom::FrameClientPtr client,
                          uint32_t frame_id,
                          uint32_t app_id,
                          const Frame::ClientPropertyMap& client_properties);

  // Increments the change id, returning the new value.
  uint32_t AdvanceChangeID();

  void LoadingStateChanged();
  void TitleChanged(const mojo::String& title);
  void DidCommitProvisionalLoad(Frame* source);
  void DidNavigateLocally(Frame* source, const GURL& url);
  void ClientPropertyChanged(const Frame* source,
                             const mojo::String& name,
                             const mojo::Array<uint8_t>& value);

  mus::Window* view_;

  FrameTreeDelegate* delegate_;

  // |root_| is owned by this, but a raw pointer as during destruction we still
  // want access to it.
  Frame* root_;

  double progress_;

  uint32_t change_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_TREE_H_
