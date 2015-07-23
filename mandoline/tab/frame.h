// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_H_
#define MANDOLINE_TAB_FRAME_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mandoline {

class FrameTree;
class FrameTreeClient;
class FrameUserData;

enum class ViewOwnership {
  OWNS_VIEW,
  DOESNT_OWN_VIEW,
};

// Frame represents an embedding in a frame. Frames own their children.
// Frames automatically delete themself if the View the frame is associated
// with is deleted.
//
// In general each Frame has a View. When a new Frame is created by a client
// there may be a small amount of time where the View is not yet known to us
// (separate pipes are used for the view and frame, resulting in undefined
// message ordering). In this case the view is null and will be set once we
// see the view (OnTreeChanged()).
//
// When the FrameTreeClient creates a new Frame there is no associated
// FrameTreeClient for the child Frame.
class Frame : public mojo::ViewObserver, public FrameTreeServer {
 public:
  Frame(FrameTree* tree,
        mojo::View* view,
        uint32_t id,
        ViewOwnership view_ownership,
        FrameTreeClient* frame_tree_client,
        scoped_ptr<FrameUserData> user_data);
  ~Frame() override;

  void Init(Frame* parent);

  // Walks the View tree starting at |view| going up returning the first
  // Frame that is associated with |view|. For example, if |view|
  // has a Frame associated with it, then that is returned. Otherwise
  // this checks view->parent() and so on.
  static Frame* FindFirstFrameAncestor(mojo::View* view);

  FrameTree* tree() { return tree_; }

  Frame* parent() { return parent_; }
  const Frame* parent() const { return parent_; }

  mojo::View* view() { return view_; }
  const mojo::View* view() const { return view_; }

  uint32_t id() const { return id_; }

  // Finds the descendant with the specified id.
  Frame* FindFrame(uint32_t id) {
    return const_cast<Frame*>(const_cast<const Frame*>(this)->FindFrame(id));
  }
  const Frame* FindFrame(uint32_t id) const;

  bool HasAncestor(const Frame* frame) const;

  FrameUserData* user_data() { return user_data_.get(); }

  const std::vector<Frame*>& children() { return children_; }

  const mojo::String& name() const { return name_; }

  // Returns true if this Frame or any child Frame is loading.
  bool IsLoading() const;

  // Returns the sum total of loading progress from this Frame and all of its
  // children, as well as the number of Frames accumulated.
  double GatherProgress(int* frame_count) const;

 private:
  friend class FrameTree;

  void SetView(mojo::View* view);

  // Adds this to |frames| and recurses through the children calling the
  // same function.
  void BuildFrameTree(std::vector<const Frame*>* frames) const;

  void Add(Frame* node);
  void Remove(Frame* node);

  // The implementation of the various FrameTreeServer functions that take
  // frame_id call into these.
  void LoadingStartedImpl();
  void LoadingStoppedImpl();
  void ProgressChangedImpl(double progress);
  void SetFrameNameImpl(const mojo::String& name);

  // Returns the Frame whose id is |frame_id|. Returns nullptr if |frame_id| is
  // not from the same connection as this.
  Frame* FindTargetFrame(uint32_t frame_id);

  // Notifies the client and all descendants as appropriate.
  void NotifyAdded(const Frame* source, const Frame* added_node);
  void NotifyRemoved(const Frame* source, const Frame* removed_node);
  void NotifyFrameNameChanged(const Frame* source);

  // mojo::ViewObserver:
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnViewDestroying(mojo::View* view) override;

  // FrameTreeServer:
  void PostMessageEventToFrame(uint32_t frame_id,
                               MessageEventPtr event) override;
  void LoadingStarted(uint32_t frame_id) override;
  void LoadingStopped(uint32_t frame_id) override;
  void ProgressChanged(uint32_t frame_id, double progress) override;
  void SetFrameName(uint32_t frame_id, const mojo::String& name) override;
  void OnCreatedFrame(uint32_t parent_id, uint32_t frame_id) override;

  FrameTree* const tree_;
  // WARNING: this may be null. See class description for details.
  mojo::View* view_;
  const uint32_t id_;
  Frame* parent_;
  ViewOwnership view_ownership_;
  std::vector<Frame*> children_;
  scoped_ptr<FrameUserData> user_data_;

  // WARNING: this may be null. See class description for details.
  FrameTreeClient* frame_tree_client_;

  bool loading_;
  double progress_;

  mojo::String name_;

  mojo::Binding<FrameTreeServer> frame_tree_server_binding_;

  DISALLOW_COPY_AND_ASSIGN(Frame);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_H_
