// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_H_
#define MANDOLINE_TAB_FRAME_H_

#include <vector>

#include "base/basictypes.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

namespace mandoline {

class FrameTree;

enum class ViewOwnership {
  OWNS_VIEW,
  DOESNT_OWN_VIEW,
};

// Frame represents an embedding in a frame. Frames own their children.
// Frames automatically delete themself if the View the frame is associated
// with is deleted.
class Frame : public mojo::ViewObserver {
 public:
  Frame(FrameTree* tree,
        mojo::View* view,
        ViewOwnership view_ownership,
        mojo::InterfaceRequest<mojo::ServiceProvider>* services,
        mojo::ServiceProviderPtr* exposed_services);
  ~Frame() override;

  // Walks the View tree starting at |view| going up returning the first
  // Frame that is associated with a view. For example, if |view|
  // has a Frame associated with it, then that is returned. Otherwise
  // this checks view->parent() and so on.
  static Frame* FindFirstFrameAncestor(mojo::View* view);

  FrameTree* tree() { return tree_; }

  Frame* parent() { return parent_; }

  mojo::View* view() { return view_; }

  void Add(Frame* node);
  void Remove(Frame* node);

 private:
  // mojo::ViewObserver:
  void OnViewDestroying(mojo::View* view) override;

  FrameTree* const tree_;
  mojo::View* const view_;
  Frame* parent_;
  ViewOwnership view_ownership_;
  mojo::ServiceProviderImpl exposed_services_;
  mojo::ServiceProviderPtr services_;
  std::vector<Frame*> children_;

  DISALLOW_COPY_AND_ASSIGN(Frame);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_H_
