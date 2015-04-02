// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/shell/application_manager/application_manager.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/types.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_client_factory.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_delegate.h"
#include "third_party/mojo_services/src/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo_services/src/window_manager/public/interfaces/window_manager.mojom.h"

using mojo::ApplicationImpl;
using mojo::Id;
using mojo::View;

namespace window_manager {
namespace {

const char kTestServiceURL[] = "mojo:test_url";

void EmptyResultCallback(bool result) {}

class TestWindowManagerObserver : public mojo::WindowManagerObserver {
 public:
  using NodeIdCallback = base::Callback<void(Id)>;

  explicit TestWindowManagerObserver(
      mojo::InterfaceRequest<mojo::WindowManagerObserver> observer_request)
      : binding_(this, observer_request.Pass()) {}
  ~TestWindowManagerObserver() override {}

  void set_focus_changed_callback(const NodeIdCallback& callback) {
    focus_changed_callback_ = callback;
  }
  void set_active_window_changed_callback(const NodeIdCallback& callback) {
    active_window_changed_callback_ = callback;
  }

 private:
  // Overridden from mojo::WindowManagerObserver:
  void OnCaptureChanged(Id new_capture_node_id) override {}
  void OnFocusChanged(Id focused_node_id) override {
    if (!focus_changed_callback_.is_null())
      focus_changed_callback_.Run(focused_node_id);
  }
  void OnActiveWindowChanged(Id active_window) override {
    if (!active_window_changed_callback_.is_null())
      active_window_changed_callback_.Run(active_window);
  }

  NodeIdCallback focus_changed_callback_;
  NodeIdCallback active_window_changed_callback_;
  mojo::Binding<WindowManagerObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerObserver);
};

class TestApplicationLoader : public mojo::shell::ApplicationLoader,
                              public mojo::ApplicationDelegate,
                              public mojo::ViewManagerDelegate {
 public:
  typedef base::Callback<void(View*)> RootAddedCallback;

  explicit TestApplicationLoader(const RootAddedCallback& root_added_callback)
      : root_added_callback_(root_added_callback) {}
  ~TestApplicationLoader() override {}

 private:
  // Overridden from mojo::shell::ApplicationLoader:
  void Load(
      const GURL& url,
      mojo::InterfaceRequest<mojo::Application> application_request) override {
    ASSERT_TRUE(application_request.is_pending());
    scoped_ptr<ApplicationImpl> app(
        new ApplicationImpl(this, application_request.Pass()));
    apps_.push_back(app.release());
  }

  // Overridden from mojo::ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    view_manager_client_factory_.reset(
        new mojo::ViewManagerClientFactory(app->shell(), this));
  }

  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override {
    root_added_callback_.Run(root);
  }
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override {}

  RootAddedCallback root_added_callback_;

  ScopedVector<ApplicationImpl> apps_;
  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

}  // namespace

class WindowManagerApiTest : public testing::Test {
 public:
  WindowManagerApiTest() {}
  ~WindowManagerApiTest() override {}

 protected:
  Id WaitForEmbed() {
    Id id;
    base::RunLoop run_loop;
    root_added_callback_ = base::Bind(&WindowManagerApiTest::OnEmbed,
                                      base::Unretained(this), &id, &run_loop);
    run_loop.Run();
    return id;
  }

  Id WaitForFocusChange() {
    Id new_focused;
    base::RunLoop run_loop;
    window_manager_observer()->set_focus_changed_callback(
        base::Bind(&WindowManagerApiTest::OnFocusChanged,
                   base::Unretained(this), &new_focused, &run_loop));
    run_loop.Run();
    return new_focused;
  }

  Id WaitForActiveWindowChange() {
    Id new_active;
    base::RunLoop run_loop;
    window_manager_observer()->set_active_window_changed_callback(
        base::Bind(&WindowManagerApiTest::OnActiveWindowChanged,
                   base::Unretained(this), &new_active, &run_loop));
    run_loop.Run();
    return new_active;
  }

  Id OpenWindow() {
    return OpenWindowWithURL(kTestServiceURL);
  }

  Id OpenWindowWithURL(const std::string& url) {
    base::RunLoop run_loop;
    window_manager_->Embed(url, nullptr, nullptr);
    run_loop.Run();
    return WaitForEmbed();
  }

  TestWindowManagerObserver* window_manager_observer() {
    return window_manager_observer_.get();
  }

  mojo::WindowManagerPtr window_manager_;

 private:
  // Overridden from testing::Test:
  void SetUp() override {
    test_helper_.reset(new mojo::shell::ShellTestHelper);
    test_helper_->Init();
    test_helper_->AddURLMapping(GURL("mojo:window_manager"),
                                GURL("mojo:core_window_manager"));
    test_helper_->SetLoaderForURL(
        scoped_ptr<mojo::shell::ApplicationLoader>(
            new TestApplicationLoader(base::Bind(
                &WindowManagerApiTest::OnRootAdded, base::Unretained(this)))),
        GURL(kTestServiceURL));
    ConnectToWindowManager2();
  }
  void TearDown() override {}

  void ConnectToWindowManager2() {
    test_helper_->application_manager()->ConnectToService(
        GURL("mojo:window_manager"), &window_manager_);
    base::RunLoop connect_loop;
    mojo::WindowManagerObserverPtr observer;
    window_manager_observer_.reset(
        new TestWindowManagerObserver(GetProxy(&observer)));

    window_manager_->GetFocusedAndActiveViews(
        observer.Pass(),
        base::Bind(&WindowManagerApiTest::GotFocusedAndActiveViews,
                   base::Unretained(this)));
    connect_loop.Run();

    // The RunLoop above ensures the connection to the window manager completes.
    // Without this the ApplicationManager would load the window manager twice.
    test_helper_->application_manager()->ConnectToService(
        GURL("mojo:core_window_manager"), &window_manager_);
  }

  void GotFocusedAndActiveViews(uint32_t, uint32_t, uint32_t) {}

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

  void OnFocusChanged(Id* new_focused,
                      base::RunLoop* run_loop,
                      Id focused_node_id) {
    *new_focused = focused_node_id;
    run_loop->Quit();
  }

  void OnActiveWindowChanged(Id* new_active,
                             base::RunLoop* run_loop,
                             Id active_node_id) {
    *new_active = active_node_id;
    run_loop->Quit();
  }

  scoped_ptr<mojo::shell::ShellTestHelper> test_helper_;
  scoped_ptr<TestWindowManagerObserver> window_manager_observer_;
  TestApplicationLoader::RootAddedCallback root_added_callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApiTest);
};

// TODO(sky): resolve this. Temporarily disabled as ApplicationManager ends up
// loading windowmanager twice because of the mapping of window_manager to
// core_window_manager.
TEST_F(WindowManagerApiTest, DISABLED_FocusAndActivateWindow) {
  Id first_window = OpenWindow();
  window_manager_->FocusWindow(first_window, base::Bind(&EmptyResultCallback));
  Id id = WaitForFocusChange();
  EXPECT_EQ(id, first_window);

  Id second_window = OpenWindow();
  window_manager_->ActivateWindow(second_window,
                                  base::Bind(&EmptyResultCallback));
  id = WaitForActiveWindowChange();
  EXPECT_EQ(id, second_window);
}

}  // namespace window_manager
