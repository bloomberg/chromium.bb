// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_test_base.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/system/macros.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_client_factory.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_delegate.h"
#include "third_party/mojo_services/src/window_manager/public/interfaces/window_manager.mojom.h"

namespace mojo {
namespace {

// TestApplication's view is embedded by the window manager.
class TestApplication : public ApplicationDelegate, public ViewManagerDelegate {
 public:
  TestApplication() : root_(nullptr) {}
  ~TestApplication() override {}

  View* root() const { return root_; }

  void set_embed_callback(const base::Closure& callback) {
    embed_callback_ = callback;
  }

 private:
  // ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // ViewManagerDelegate:
  void OnEmbed(View* root,
               InterfaceRequest<ServiceProvider> services,
               ServiceProviderPtr exposed_services) override {
    root_ = root;
    embed_callback_.Run();
  }
  void OnViewManagerDisconnected(ViewManager* view_manager) override {}

  View* root_;
  base::Closure embed_callback_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestApplication);
};

class TestWindowManagerObserver : public WindowManagerObserver {
 public:
  explicit TestWindowManagerObserver(
      InterfaceRequest<WindowManagerObserver> observer_request)
      : binding_(this, observer_request.Pass()) {}
  ~TestWindowManagerObserver() override {}

 private:
  // Overridden from WindowManagerClient:
  void OnCaptureChanged(Id new_capture_node_id) override {}
  void OnFocusChanged(Id focused_node_id) override {}
  void OnActiveWindowChanged(Id active_window) override {}

  Binding<WindowManagerObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerObserver);
};

class WindowManagerApplicationTest : public test::ApplicationTestBase {
 public:
  WindowManagerApplicationTest() {}
  ~WindowManagerApplicationTest() override {}

 protected:
  // ApplicationTestBase:
  void SetUp() override {
    ApplicationTestBase::SetUp();
    application_impl()->ConnectToService("mojo:window_manager",
                                         &window_manager_);
  }
  ApplicationDelegate* GetApplicationDelegate() override {
    return &test_application_;
  }

  void EmbedApplicationWithURL(const std::string& url) {
    window_manager_->Embed(url, nullptr, nullptr);

    base::RunLoop run_loop;
    test_application_.set_embed_callback(run_loop.QuitClosure());
    run_loop.Run();
  }

  WindowManagerPtr window_manager_;
  TestApplication test_application_;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowManagerApplicationTest);
};

TEST_F(WindowManagerApplicationTest, Embed) {
  EXPECT_EQ(nullptr, test_application_.root());
  EmbedApplicationWithURL(application_impl()->url());
  EXPECT_NE(nullptr, test_application_.root());
}

struct BoolCallback {
  BoolCallback(bool* bool_value, base::RunLoop* run_loop)
      : bool_value(bool_value), run_loop(run_loop) {}

  void Run(bool value) const {
    *bool_value = value;
    run_loop->Quit();
  }

  bool* bool_value;
  base::RunLoop* run_loop;
};

TEST_F(WindowManagerApplicationTest, SetCaptureFailsFromNonVM) {
  EmbedApplicationWithURL(application_impl()->url());
  bool callback_value = true;
  base::RunLoop run_loop;
  window_manager_->SetCapture(test_application_.root()->id(),
                              BoolCallback(&callback_value, &run_loop));
  run_loop.Run();
  // This call only succeeds for WindowManager connections from the ViewManager.
  EXPECT_FALSE(callback_value);
}

TEST_F(WindowManagerApplicationTest, FocusWindowFailsFromNonVM) {
  EmbedApplicationWithURL(application_impl()->url());
  bool callback_value = true;
  base::RunLoop run_loop;
  window_manager_->FocusWindow(test_application_.root()->id(),
                               BoolCallback(&callback_value, &run_loop));
  run_loop.Run();
  // This call only succeeds for WindowManager connections from the ViewManager.
  EXPECT_FALSE(callback_value);
}

TEST_F(WindowManagerApplicationTest, ActivateWindowFailsFromNonVM) {
  EmbedApplicationWithURL(application_impl()->url());
  bool callback_value = true;
  base::RunLoop run_loop;
  window_manager_->ActivateWindow(test_application_.root()->id(),
                                  BoolCallback(&callback_value, &run_loop));
  run_loop.Run();
  // This call only succeeds for WindowManager connections from the ViewManager.
  EXPECT_FALSE(callback_value);
}

struct FocusedAndActiveViewsCallback {
  FocusedAndActiveViewsCallback(uint32* capture_view_id,
                                uint32* focused_view_id,
                                uint32* active_view_id,
                                base::RunLoop* run_loop)
      : capture_view_id(capture_view_id),
        focused_view_id(focused_view_id),
        active_view_id(active_view_id),
        run_loop(run_loop) {
  }

  void Run(uint32 capture, uint32 focused, uint32 active) const {
    *capture_view_id = capture;
    *focused_view_id = focused;
    *active_view_id = active;
    run_loop->Quit();
  }

  uint32* capture_view_id;
  uint32* focused_view_id;
  uint32* active_view_id;
  base::RunLoop* run_loop;
};

TEST_F(WindowManagerApplicationTest, GetFocusedAndActiveViewsFailsWithoutFC) {
  EmbedApplicationWithURL(application_impl()->url());
  uint32 capture_view_id = static_cast<uint32>(-1);
  uint32 focused_view_id = static_cast<uint32>(-1);
  uint32 active_view_id = static_cast<uint32>(-1);
  base::RunLoop run_loop;

  WindowManagerObserverPtr observer;
  scoped_ptr<TestWindowManagerObserver> window_manager_observer(
      new TestWindowManagerObserver(GetProxy(&observer)));

  window_manager_->GetFocusedAndActiveViews(
      observer.Pass(),
      FocusedAndActiveViewsCallback(&capture_view_id,
                                    &focused_view_id,
                                    &active_view_id,
                                    &run_loop));
  run_loop.Run();
  // This call fails if the WindowManager does not have a FocusController.
  EXPECT_EQ(0u, capture_view_id);
  EXPECT_EQ(0u, focused_view_id);
  EXPECT_EQ(0u, active_view_id);
}

// TODO(msw): Write tests exercising other WindowManager functionality.

}  // namespace
}  // namespace mojo
