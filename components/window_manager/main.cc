// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/window_manager/window_manager_app.h"
#include "components/window_manager/window_manager_delegate.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

// ApplicationDelegate implementation file for WindowManager users (e.g.
// core window manager tests) that do not want to provide their own
// ApplicationDelegate::Create().

using mojo::View;
using mojo::ViewManager;

namespace window_manager {

class DefaultWindowManager : public mojo::ApplicationDelegate,
                             public mojo::ViewManagerDelegate,
                             public WindowManagerDelegate {
 public:
  DefaultWindowManager()
      : window_manager_app_(new WindowManagerApp(this, this)),
        root_(nullptr),
        window_offset_(10) {
  }
  ~DefaultWindowManager() override {}

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* impl) override {
    window_manager_app_->Initialize(impl);
    tracing_.Initialize(impl);
  }
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    window_manager_app_->ConfigureIncomingConnection(connection);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override {
    root_ = root;
  }
  void OnViewManagerDisconnected(ViewManager* view_manager) override {}

  // Overridden from WindowManagerDelegate:
  void Embed(const mojo::String& url,
             mojo::InterfaceRequest<mojo::ServiceProvider> services,
             mojo::ServiceProviderPtr exposed_services) override {
    DCHECK(root_);
    View* view = root_->view_manager()->CreateView();
    root_->AddChild(view);

    mojo::Rect rect;
    rect.x = rect.y = window_offset_;
    rect.width = rect.height = 100;
    view->SetBounds(rect);
    window_offset_ += 10;

    view->SetVisible(true);
    view->Embed(url, services.Pass(), exposed_services.Pass());
  }

  scoped_ptr<WindowManagerApp> window_manager_app_;

  View* root_;
  int window_offset_;
  mojo::TracingImpl tracing_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DefaultWindowManager);
};

}  // namespace window_manager

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(
      new window_manager::DefaultWindowManager);
  return runner.Run(shell_handle);
}
