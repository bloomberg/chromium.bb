// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "mojo/examples/wm_flow/app/embedder.mojom.h"
#include "mojo/examples/wm_flow/embedded/embeddee.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace examples {

namespace {

class EmbeddeeImpl : public mojo::InterfaceImpl<Embeddee> {
 public:
  EmbeddeeImpl() {}
  virtual ~EmbeddeeImpl() {}

 private:
  // Overridden from Embeddee:
  virtual void HelloBack(const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(EmbeddeeImpl);
};

}  // namespace

class WMFlowEmbedded : public mojo::ApplicationDelegate,
                       public mojo::ViewManagerDelegate {
 public:
  WMFlowEmbedded()
      : view_manager_client_factory_(this) {}
  virtual ~WMFlowEmbedded() {}

 private:
  // Overridden from Application:
  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
  }
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::ViewManager* view_manager,
      mojo::Node* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> imported_services) MOJO_OVERRIDE {
    mojo::View* view =
        mojo::View::Create(view_manager);
    root->SetActiveView(view);
    view->SetColor(SK_ColorMAGENTA);

    exported_services->AddService(&embeddee_factory_);
    mojo::ConnectToService(imported_services.get(), &embedder_);
    embedder_->HelloWorld(base::Bind(&WMFlowEmbedded::HelloWorldAck,
                                     base::Unretained(this)));
  }
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) MOJO_OVERRIDE {}

  void HelloWorldAck() {
    printf("HelloWorld() ack'ed\n");
  }

  mojo::ViewManagerClientFactory view_manager_client_factory_;
  EmbedderPtr embedder_;
  mojo::InterfaceFactoryImpl<EmbeddeeImpl> embeddee_factory_;

  DISALLOW_COPY_AND_ASSIGN(WMFlowEmbedded);
};

}  // namespace examples

namespace mojo {

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::WMFlowEmbedded;
}

}  // namespace
