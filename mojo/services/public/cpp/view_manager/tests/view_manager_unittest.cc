// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_observer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace services {
namespace view_manager {

base::RunLoop* current_run_loop = NULL;

void DoRunLoop() {
  base::RunLoop run_loop;
  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = NULL;
}

void QuitRunLoop() {
  current_run_loop->Quit();
}

// ViewManager -----------------------------------------------------------------

class ViewManagerTest : public testing::Test,
                        public ViewManagerObserver {
 public:
  ViewManagerTest() : commit_count_(0) {}

 protected:
  ViewManager* view_manager_1() { return view_manager_1_.get(); }
  ViewManager* view_manager_2() { return view_manager_2_.get(); }

 private:
  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    test_helper_.Init();
    view_manager_1_.reset(new ViewManager(test_helper_.shell()));
    view_manager_2_.reset(new ViewManager(test_helper_.shell()));
    ViewManagerPrivate(view_manager_1_.get()).AddObserver(this);
    ViewManagerPrivate(view_manager_2_.get()).AddObserver(this);
  }

  // Overridden from ViewManagerObserver:
  virtual void OnCommit(ViewManager* manager) OVERRIDE {
    ++commit_count_;
  }
  virtual void OnCommitResponse(ViewManager* manager, bool success) OVERRIDE {
    if (--commit_count_ == 0)
      QuitRunLoop();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;
  scoped_ptr<ViewManager> view_manager_1_;
  scoped_ptr<ViewManager> view_manager_2_;
  int commit_count_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

TEST_F(ViewManagerTest, BuildNodeTree) {
  view_manager_1()->BuildNodeTree(base::Bind(&QuitRunLoop));
  DoRunLoop();

  scoped_ptr<ViewTreeNode> node1(new ViewTreeNode(view_manager_1()));
  view_manager_1()->tree()->AddChild(node1.get());
  DoRunLoop();

  view_manager_2()->BuildNodeTree(base::Bind(&QuitRunLoop));
  DoRunLoop();

  EXPECT_EQ(view_manager_2()->tree()->children().front()->id(), node1->id());
}

// TODO(beng): node hierarchy changed
// TODO(beng): node destruction

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
