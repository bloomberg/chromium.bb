// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const char kTestServiceURL[] = "mojo:test_url";

void EmptyResultCallback(bool result) {}

// Callback from Embed(). |result| is the result of the Embed() call and
// |run_loop| the nested RunLoop.
void ResultCallback(bool* result_cache, base::RunLoop* run_loop, bool result) {
  *result_cache = result;
  run_loop->Quit();
}

// Responsible for establishing the initial ViewManagerService connection.
// Blocks until result is determined.
bool InitEmbed(ViewManagerInitService* view_manager_init,
               const std::string& url) {
  bool result = false;
  base::RunLoop run_loop;
  ServiceProviderPtr sp;
  BindToProxy(new ServiceProviderImpl, &sp);
  view_manager_init->Embed(url, sp.Pass(),
                           base::Bind(&ResultCallback, &result, &run_loop));
  run_loop.Run();
  return result;
}

class TestWindowManagerClient : public WindowManagerClient {
 public:
  typedef base::Callback<void(Id, Id)>
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
      Id old_capture_node_id,
      Id new_capture_node_id) MOJO_OVERRIDE {
  }
  virtual void OnFocusChanged(
      Id old_focused_node_id,
      Id new_focused_node_id) MOJO_OVERRIDE {
    if (!focus_changed_callback_.is_null())
      focus_changed_callback_.Run(old_focused_node_id, new_focused_node_id);
  }
  virtual void OnActiveWindowChanged(
      Id old_active_window,
      Id new_active_window) MOJO_OVERRIDE {
    if (!active_window_changed_callback_.is_null())
      active_window_changed_callback_.Run(old_active_window, new_active_window);
  }

  base::RunLoop* run_loop_;
  TwoNodeCallback focus_changed_callback_;
  TwoNodeCallback active_window_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerClient);
};

class TestApplicationLoader : public ApplicationLoader,
                              public ApplicationDelegate,
                              public ViewManagerDelegate {
 public:
  typedef base::Callback<void(View*)> RootAddedCallback;

  explicit TestApplicationLoader(const RootAddedCallback& root_added_callback)
      : root_added_callback_(root_added_callback) {}
  virtual ~TestApplicationLoader() {}

 private:
  // Overridden from ApplicationLoader:
  virtual void Load(ApplicationManager* application_manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) MOJO_OVERRIDE {
    ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
    if (!shell_handle.is_valid())
      return;
    scoped_ptr<ApplicationImpl> app(
        new ApplicationImpl(this, shell_handle.Pass()));
    apps_.push_back(app.release());
  }
  virtual void OnApplicationError(ApplicationManager* application_manager,
                                  const GURL& url) MOJO_OVERRIDE {}

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) MOJO_OVERRIDE {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(
      ViewManager* view_manager,
      View* root,
      ServiceProviderImpl* exported_services,
      scoped_ptr<ServiceProvider> imported_services) MOJO_OVERRIDE {
    root_added_callback_.Run(root);
  }
  virtual void OnViewManagerDisconnected(
      ViewManager* view_manager) MOJO_OVERRIDE {
  }

  RootAddedCallback root_added_callback_;

  ScopedVector<ApplicationImpl> apps_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

}  // namespace

class WindowManagerApiTest : public testing::Test {
 public:
  WindowManagerApiTest() {}
  virtual ~WindowManagerApiTest() {}

 protected:
  typedef std::pair<Id, Id> TwoIds;

  Id WaitForEmbed() {
    Id id;
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

  Id OpenWindow() {
    return OpenWindowWithURL(kTestServiceURL);
  }

  Id OpenWindowWithURL(const std::string& url) {
    InitEmbed(view_manager_init_.get(), url);
    return WaitForEmbed();
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
        scoped_ptr<ApplicationLoader>(new TestApplicationLoader(base::Bind(
            &WindowManagerApiTest::OnRootAdded, base::Unretained(this)))),
        GURL(kTestServiceURL));
    test_helper_.application_manager()->ConnectToService(
        GURL("mojo:mojo_view_manager"), &view_manager_init_);
    ASSERT_TRUE(InitEmbed(view_manager_init_.get(),
                          "mojo:mojo_core_window_manager"));
    ConnectToWindowManager();
  }
  virtual void TearDown() MOJO_OVERRIDE {}

  void ConnectToWindowManager() {
    test_helper_.application_manager()->ConnectToService(
        GURL("mojo:mojo_core_window_manager"), &window_manager_);
    base::RunLoop connect_loop;
    window_manager_client_.reset(new TestWindowManagerClient(&connect_loop));
    window_manager_.set_client(window_manager_client());
    connect_loop.Run();
  }

  void OnRootAdded(View* root) {
    if (!root_added_callback_.is_null())
      root_added_callback_.Run(root);
  }

  void OnEmbed(Id* root_id,
               base::RunLoop* loop,
               View* root) {
    *root_id = root->id();
    loop->Quit();
  }

  void OnFocusChanged(TwoIds* old_and_new,
                      base::RunLoop* run_loop,
                      Id old_focused_node_id,
                      Id new_focused_node_id) {
    DCHECK(old_and_new);
    old_and_new->first = old_focused_node_id;
    old_and_new->second = new_focused_node_id;
    run_loop->Quit();
  }

  void OnActiveWindowChanged(TwoIds* old_and_new,
                             base::RunLoop* run_loop,
                             Id old_focused_node_id,
                             Id new_focused_node_id) {
    DCHECK(old_and_new);
    old_and_new->first = old_focused_node_id;
    old_and_new->second = new_focused_node_id;
    run_loop->Quit();
  }

  shell::ShellTestHelper test_helper_;
  ViewManagerInitServicePtr view_manager_init_;
  scoped_ptr<TestWindowManagerClient> window_manager_client_;
  TestApplicationLoader::RootAddedCallback root_added_callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApiTest);
};

TEST_F(WindowManagerApiTest, FocusAndActivateWindow) {
  Id first_window = OpenWindow();
  window_manager_->FocusWindow(first_window,
                               base::Bind(&EmptyResultCallback));
  TwoIds ids = WaitForFocusChange();
  EXPECT_TRUE(ids.first == 0);
  EXPECT_EQ(ids.second, first_window);

  Id second_window = OpenWindow();
  window_manager_->ActivateWindow(second_window,
                                  base::Bind(&EmptyResultCallback));
  ids = WaitForActiveWindowChange();
  EXPECT_EQ(ids.first, first_window);
  EXPECT_EQ(ids.second, second_window);
}

}  // namespace mojo
