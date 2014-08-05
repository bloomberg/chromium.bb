// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/html_viewer/blink_platform_impl.h"
#include "mojo/services/html_viewer/html_document_view.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace mojo {

class HTMLViewer;

class NavigatorImpl : public InterfaceImpl<Navigator> {
 public:
  explicit NavigatorImpl(HTMLViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from Navigator:
  virtual void Navigate(
      uint32_t node_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) OVERRIDE;

  HTMLViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class HTMLViewer : public ApplicationDelegate, public ViewManagerDelegate {
 public:
  HTMLViewer()
      : application_impl_(NULL),
        document_view_(NULL),
        navigator_factory_(this),
        view_manager_client_factory_(this) {}
  virtual ~HTMLViewer() {
    blink::shutdown();
  }

  void Load(ResponseDetailsPtr response_details) {
    // Need to wait for OnEmbed.
    response_details_ = response_details.Pass();
    MaybeLoad();
  }

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    application_impl_ = app;
    blink_platform_impl_.reset(new BlinkPlatformImpl(app));
    blink::initialize(blink_platform_impl_.get());
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    connection->AddService(&navigator_factory_);
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager, Node* root) OVERRIDE {
    document_view_ = new HTMLDocumentView(
        application_impl_->ConnectToApplication("mojo://mojo_window_manager/")->
            GetServiceProvider(),
        view_manager);
    document_view_->AttachToNode(root);
    MaybeLoad();
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  void MaybeLoad() {
    if (document_view_ && response_details_)
      document_view_->Load(response_details_->response.Pass());
  }

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;
  ApplicationImpl* application_impl_;

  // TODO(darin): Figure out proper ownership of this instance.
  HTMLDocumentView* document_view_;
  ResponseDetailsPtr response_details_;
  InterfaceFactoryImplWithContext<NavigatorImpl, HTMLViewer> navigator_factory_;
  ViewManagerClientFactory view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

void NavigatorImpl::Navigate(
    uint32_t node_id,
    NavigationDetailsPtr navigation_details,
    ResponseDetailsPtr response_details) {
  viewer_->Load(response_details.Pass());
}

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new HTMLViewer;
}

}
