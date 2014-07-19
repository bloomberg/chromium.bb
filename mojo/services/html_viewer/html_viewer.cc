// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/html_viewer/blink_platform_impl.h"
#include "mojo/services/html_viewer/html_document_view.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace mojo {

class HTMLViewer;

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit NavigatorImpl(ApplicationConnection* connection,
                         HTMLViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigator:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE;

  HTMLViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class HTMLViewer : public ApplicationDelegate,
                   public view_manager::ViewManagerDelegate {
 public:
  HTMLViewer() : application_impl_(NULL), document_view_(NULL) {
  }
  virtual ~HTMLViewer() {
    blink::shutdown();
  }

  void Load(URLResponsePtr response) {
    // Need to wait for OnRootAdded.
    response_ = response.Pass();
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
    connection->AddService<NavigatorImpl>(this);
    view_manager::ViewManager::ConfigureIncomingConnection(connection, this);
    return true;
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    document_view_ = new HTMLDocumentView(
        application_impl_->ConnectToApplication("mojo://mojo_window_manager/")->
            GetServiceProvider(), view_manager);
    document_view_->AttachToNode(root);
    MaybeLoad();
  }
  virtual void OnViewManagerDisconnected(
      view_manager::ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  void MaybeLoad() {
    if (document_view_ && response_)
      document_view_->Load(response_.Pass());
  }

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;
  ApplicationImpl* application_impl_;

  // TODO(darin): Figure out proper ownership of this instance.
  HTMLDocumentView* document_view_;
  URLResponsePtr response_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

void NavigatorImpl::Navigate(
    uint32_t node_id,
    navigation::NavigationDetailsPtr navigation_details,
    navigation::ResponseDetailsPtr response_details) {
  viewer_->Load(response_details->response.Pass());
}

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new HTMLViewer;
}

}
