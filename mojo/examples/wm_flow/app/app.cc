// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace examples {
namespace {
void ConnectCallback(bool success) {}

const SkColor kColors[] = { SK_ColorRED, SK_ColorGREEN, SK_ColorYELLOW };

}  // namespace

// This app starts its life via Connect() rather than by being embed, so it does
// not start with a connection to the ViewManager service. It has to obtain a
// connection by connecting to the ViewManagerInit service and asking to be
// embed without a node context.
class WMFlowApp : public mojo::ApplicationDelegate,
                  public mojo::ViewManagerDelegate {
 public:
  WMFlowApp() : embed_count_(0), view_manager_client_factory_(this) {}
  virtual ~WMFlowApp() {}

 private:
  // Overridden from Application:
  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    mojo::ViewManagerInitServicePtr init_svc;
    app->ConnectToService("mojo:mojo_view_manager", &init_svc);
    init_svc->Embed("mojo:mojo_wm_flow_app", base::Bind(&ConnectCallback));
    init_svc->Embed("mojo:mojo_wm_flow_app", base::Bind(&ConnectCallback));
    init_svc->Embed("mojo:mojo_wm_flow_app", base::Bind(&ConnectCallback));
  }
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  void OnConnect(bool success) {}

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(mojo::ViewManager* view_manager,
                       mojo::Node* root) MOJO_OVERRIDE {
    mojo::View* view =
        mojo::View::Create(view_manager);
    root->SetActiveView(view);
    view->SetColor(kColors[embed_count_++ % arraysize(kColors)]);
  }
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) MOJO_OVERRIDE {}

  int embed_count_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(WMFlowApp);
};

}  // namespace examples

namespace mojo {

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::WMFlowApp;
}

}  // namespace
