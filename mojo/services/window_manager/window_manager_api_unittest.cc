// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const char kTestServiceURL[] = "mojo:test_url";

void EmptyResultCallback(bool result) {}

// Callback from EmbedRoot(). |result| is the result of the
// Embed() call and |run_loop| the nested RunLoop.
void ResultCallback(bool* result_cache, base::RunLoop* run_loop, bool result) {
  *result_cache = result;
  run_loop->Quit();
}

// Responsible for establishing the initial ViewManagerService connection.
// Blocks until result is determined.
bool EmbedRoot(view_manager::ViewManagerInitService* view_manager_init,
               const std::string& url) {
  bool result = false;
  base::RunLoop run_loop;
  view_manager_init->EmbedRoot(url,
                               base::Bind(&ResultCallback, &result, &run_loop));
  run_loop.Run();
  return result;
}

void OpenWindowCallback(view_manager::Id* id,
                        base::RunLoop* run_loop,
                        view_manager::Id window_id) {
  *id = window_id;
  run_loop->Quit();
}

view_manager::Id OpenWindow(WindowManagerService* window_manager) {
  base::RunLoop run_loop;
  view_manager::Id id;
  window_manager->OpenWindow(
      base::Bind(&OpenWindowCallback, &id, &run_loop));
  run_loop.Run();
  return id;
}

view_manager::Id OpenWindowWithURL(WindowManagerService* window_manager,
                                   const std::string& url) {
  base::RunLoop run_loop;
  view_manager::Id id;
  window_manager->OpenWindowWithURL(
      url,
      base::Bind(&OpenWindowCallback, &id, &run_loop));
  run_loop.Run();
  return id;
}

class TestWindowManagerClient : public WindowManagerClient {
 public:
  typedef base::Callback<void(view_manager::Id, view_manager::Id)>
      TwoNodeCallback;

  explicit TestWindowManagerClient(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}
  virtual ~TestWindowManagerClient() {}

  void set_focus_changed_callback(const TwoNodeCallback& callback) {
    focus_changed_callback_ = callback;
  }
  void set_active_window_changed_callback(const TwoNodeCallback& callback) {
    active_window_changed_callback_ = callback;
  }

 private:
  // Overridden from WindowManagerClient:
  virtual void OnWindowManagerReady() MOJO_OVERRIDE {
    run_loop_->Quit();
  }
  virtual void OnCaptureChanged(
      view_manager::Id old_capture_node_id,
      view_manager::Id new_capture_node_id) MOJO_OVERRIDE {
  }
  virtual void OnFocusChanged(
      view_manager::Id old_focused_node_id,
      view_manager::Id new_focused_node_id) MOJO_OVERRIDE {
    if (!focus_changed_callback_.is_null())
      focus_changed_callback_.Run(old_focused_node_id, new_focused_node_id);
  }
  virtual void OnActiveWindowChanged(
      view_manager::Id old_active_window,
      view_manager::Id new_active_window) MOJO_OVERRIDE {
    if (!active_window_changed_callback_.is_null())
      active_window_changed_callback_.Run(old_active_window, new_active_window);
  }

  base::RunLoop* run_loop_;
  TwoNodeCallback focus_changed_callback_;
  TwoNodeCallback active_window_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerClient);
};

class TestServiceLoader : public ServiceLoader,
                          public ApplicationDelegate,
                          public view_manager::ViewManagerDelegate {
 public:
  typedef base::Callback<void(view_manager::Node*)> RootAddedCallback;

  explicit TestServiceLoader(const RootAddedCallback& root_added_callback)
      : root_added_callback_(root_added_callback) {}
  virtual ~TestServiceLoader() {}

 private:
  // Overridden from ServiceLoader:
  virtual void LoadService(ServiceManager* service_manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) MOJO_OVERRIDE {
    scoped_ptr<ApplicationImpl> app(
        new ApplicationImpl(this, shell_handle.Pass()));
    apps_.push_back(app.release());
  }
  virtual void OnServiceError(ServiceManager* service_manager,
                              const GURL& url) MOJO_OVERRIDE {
  }

  // Overridden from ApplicationDelegate:
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) MOJO_OVERRIDE {
    view_manager::ViewManager::ConfigureIncomingConnection(connection, this);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) MOJO_OVERRIDE {
    root_added_callback_.Run(root);
  }
  virtual void OnViewManagerDisconnected(
      view_manager::ViewManager* view_manager) MOJO_OVERRIDE {
  }

  RootAddedCallback root_added_callback_;

  ScopedVector<ApplicationImpl> apps_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceLoader);
};

}  // namespace

class WindowManagerApiTest : public testing::Test {
 public:
  WindowManagerApiTest() {}
  virtual ~WindowManagerApiTest() {}

 protected:
  typedef std::pair<view_manager::Id, view_manager::Id> TwoIds;

  view_manager::Id WaitForEmbed() {
    view_manager::Id id;
    base::RunLoop run_loop;
    root_added_callback_ = base::Bind(&WindowManagerApiTest::OnEmbed,
                                      base::Unretained(this), &id, &run_loop);
    run_loop.Run();
    return id;
  }

  TwoIds WaitForFocusChange() {
    TwoIds old_and_new;
    base::RunLoop run_loop;
    window_manager_client()->set_focus_changed_callback(
        base::Bind(&WindowManagerApiTest::OnFocusChanged,
                   base::Unretained(this), &old_and_new, &run_loop));
    run_loop.Run();
    return old_and_new;
  }

  TwoIds WaitForActiveWindowChange() {
    TwoIds old_and_new;
    base::RunLoop run_loop;
    window_manager_client()->set_active_window_changed_callback(
        base::Bind(&WindowManagerApiTest::OnActiveWindowChanged,
                   base::Unretained(this), &old_and_new, &run_loop));
    run_loop.Run();
    return old_and_new;
  }

  TestWindowManagerClient* window_manager_client() {
    return window_manager_client_.get();
  }

  WindowManagerServicePtr window_manager_;

 private:
  // Overridden from testing::Test:
  virtual void SetUp() MOJO_OVERRIDE {
    test_helper_.Init();
    test_helper_.SetLoaderForURL(
        scoped_ptr<ServiceLoader>(new TestServiceLoader(
            base::Bind(&WindowManagerApiTest::OnRootAdded,
                       base::Unretained(this)))),
        GURL(kTestServiceURL));
    test_helper_.service_manager()->ConnectToService(
        GURL("mojo:mojo_view_manager"),
        &view_manager_init_);
    ASSERT_TRUE(EmbedRoot(view_manager_init_.get(),
                          "mojo:mojo_core_window_manager"));
    ConnectToWindowManager();
  }
  virtual void TearDown() MOJO_OVERRIDE {}

  void ConnectToWindowManager() {
    test_helper_.service_manager()->ConnectToService(
        GURL("mojo:mojo_core_window_manager"),
        &window_manager_);
    base::RunLoop connect_loop;
    window_manager_client_.reset(new TestWindowManagerClient(&connect_loop));
    window_manager_.set_client(window_manager_client());
    connect_loop.Run();
  }

  void OnRootAdded(view_manager::Node* root) {
    if (!root_added_callback_.is_null())
      root_added_callback_.Run(root);
  }

  void OnEmbed(view_manager::Id* root_id,
               base::RunLoop* loop,
               view_manager::Node* root) {
    *root_id = root->id();
    loop->Quit();
  }

  void OnFocusChanged(TwoIds* old_and_new,
                      base::RunLoop* run_loop,
                      view_manager::Id old_focused_node_id,
                      view_manager::Id new_focused_node_id) {
    DCHECK(old_and_new);
    old_and_new->first = old_focused_node_id;
    old_and_new->second = new_focused_node_id;
    run_loop->Quit();
  }

  void OnActiveWindowChanged(TwoIds* old_and_new,
                             base::RunLoop* run_loop,
                             view_manager::Id old_focused_node_id,
                             view_manager::Id new_focused_node_id) {
    DCHECK(old_and_new);
    old_and_new->first = old_focused_node_id;
    old_and_new->second = new_focused_node_id;
    run_loop->Quit();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;
  view_manager::ViewManagerInitServicePtr view_manager_init_;
  scoped_ptr<TestWindowManagerClient> window_manager_client_;
  TestServiceLoader::RootAddedCallback root_added_callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApiTest);
};

TEST_F(WindowManagerApiTest, OpenWindow) {
  OpenWindow(window_manager_.get());
  view_manager::Id created_node =
      OpenWindowWithURL(window_manager_.get(), kTestServiceURL);
  view_manager::Id embed_node = WaitForEmbed();
  EXPECT_EQ(created_node, embed_node);
}

TEST_F(WindowManagerApiTest, FocusAndActivateWindow) {
  view_manager::Id first_window = OpenWindow(window_manager_.get());
  window_manager_->FocusWindow(first_window,
                               base::Bind(&EmptyResultCallback));
  TwoIds ids = WaitForFocusChange();
  EXPECT_TRUE(ids.first == 0);
  EXPECT_EQ(ids.second, first_window);

  view_manager::Id second_window = OpenWindow(window_manager_.get());
  window_manager_->ActivateWindow(second_window,
                                  base::Bind(&EmptyResultCallback));
  ids = WaitForActiveWindowChange();
  EXPECT_EQ(ids.first, first_window);
  EXPECT_EQ(ids.second, second_window);
}

}  // namespace mojo
