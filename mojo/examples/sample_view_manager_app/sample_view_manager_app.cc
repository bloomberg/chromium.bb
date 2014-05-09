// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define SAMPLE_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define SAMPLE_APP_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class SampleApp : public Application {
 public:
  explicit SampleApp(MojoHandle shell_handle)
      : Application(shell_handle) {
    view_manager_.reset(new services::view_manager::ViewManager(shell()));
    view_manager_->Init();
    services::view_manager::ViewTreeNode* node1 =
        services::view_manager::ViewTreeNode::Create(view_manager_.get());
    services::view_manager::ViewTreeNode* node11 =
        services::view_manager::ViewTreeNode::Create(view_manager_.get());
    node1->AddChild(node11);
  }

  virtual ~SampleApp() {
  }

 private:
  // SampleApp creates a ViewManager and a trivial node hierarchy.
  scoped_ptr<services::view_manager::ViewManager> view_manager_;

  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;

  mojo::examples::SampleApp app(shell_handle);
  loop.Run();
  return MOJO_RESULT_OK;
}
