// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/window_manager/public/interfaces/window_manager.mojom.h"
#include "mojo/application/application_test_base_chromium.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"

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

class WindowManagerApplicationTest : public test::ApplicationTestBase {
 public:
  WindowManagerApplicationTest() {}
  ~WindowManagerApplicationTest() override {}

 protected:
  // ApplicationTestBase:
  void SetUp() override {
    ApplicationTestBase::SetUp();
    application_impl()->ConnectToService("mojo:test_window_manager",
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

}  // namespace
}  // namespace mojo
