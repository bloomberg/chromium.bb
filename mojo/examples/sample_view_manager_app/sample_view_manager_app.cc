// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "ui/gfx/canvas.h"

namespace mojo {
namespace examples {

class SampleApp : public Application {
 public:
  SampleApp() {}
  virtual ~SampleApp() {}

  virtual void Initialize() MOJO_OVERRIDE {
    view_manager_.reset(new view_manager::ViewManager(service_provider()));
    view_manager_->Init();
    view_manager::ViewTreeNode* node1 =
        view_manager::ViewTreeNode::Create(view_manager_.get());
    view_manager::ViewTreeNode* node11 =
        view_manager::ViewTreeNode::Create(view_manager_.get());
    node11->SetBounds(gfx::Rect(800, 600));

    view_manager::View* view11 =
        view_manager::View::Create(view_manager_.get());
    node11->SetActiveView(view11);
    view_manager_->tree()->AddChild(node1);
    node1->AddChild(node11);

    gfx::Canvas canvas(gfx::Size(800, 600), 1.0f, true);
    canvas.DrawColor(SK_ColorRED);
    view11->SetContents(
        skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(true));
  }

 private:
  // SampleApp creates a ViewManager and a trivial node hierarchy.
  scoped_ptr<view_manager::ViewManager> view_manager_;

  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
