// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace examples {

class MediaViewer;

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit NavigatorImpl(MediaViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigate:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE;

  MediaViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class MediaViewer : public Application,
                    public view_manager::ViewManagerDelegate {
 public:
  MediaViewer() : content_node_(NULL),
                  view_manager_(NULL) {
    handler_map_["image/png"] = "mojo:mojo_png_viewer";
  }

  virtual ~MediaViewer() {}

  void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) {
    // TODO(yzshen): This shouldn't be needed once FIFO is ready.
    if (!view_manager_) {
      pending_navigate_request_.reset(new PendingNavigateRequest);
      pending_navigate_request_->node_id = node_id;
      pending_navigate_request_->navigation_details = navigation_details.Pass();
      pending_navigate_request_->response_details = response_details.Pass();

      return;
    }

    // TODO(yzshen): provide media control UI.
    std::string handler = GetHandlerForContentType(
        response_details->response->mime_type);
    if (handler.empty())
      return;

    content_node_->Embed(handler);

    if (navigation_details) {
      navigation::NavigatorPtr navigator;
      ConnectTo(handler, &navigator);
      navigator->Navigate(content_node_->id(), navigation_details.Pass(),
                          response_details.Pass());
    }
  }

 private:
  typedef std::map<std::string, std::string> HandlerMap;

  struct PendingNavigateRequest {
    uint32_t node_id;
    navigation::NavigationDetailsPtr navigation_details;
    navigation::ResponseDetailsPtr response_details;
  };

  // Overridden from Application:
  virtual void Initialize() OVERRIDE {
    AddService<NavigatorImpl>(this);
    view_manager::ViewManager::Create(this, this);
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    view_manager_ = view_manager;
    content_node_ = view_manager::Node::Create(view_manager_);
    root->AddChild(content_node_);

    gfx::Rect bounds(root->bounds().size());
    content_node_->SetBounds(bounds);

    if (pending_navigate_request_) {
      scoped_ptr<PendingNavigateRequest> request(
          pending_navigate_request_.release());

      Navigate(request->node_id, request->navigation_details.Pass(),
               request->response_details.Pass());
    }
  }

  std::string GetHandlerForContentType(const std::string& content_type) {
    HandlerMap::const_iterator it = handler_map_.find(content_type);
    return it != handler_map_.end() ? it->second : std::string();
  }

  view_manager::Node* content_node_;
  view_manager::ViewManager* view_manager_;
  HandlerMap handler_map_;
  scoped_ptr<PendingNavigateRequest> pending_navigate_request_;

  DISALLOW_COPY_AND_ASSIGN(MediaViewer);
};

void NavigatorImpl::Navigate(
    uint32_t node_id,
    navigation::NavigationDetailsPtr navigation_details,
    navigation::ResponseDetailsPtr response_details) {
  viewer_->Navigate(node_id, navigation_details.Pass(),
                    response_details.Pass());
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::MediaViewer;
}

}  // namespace mojo
