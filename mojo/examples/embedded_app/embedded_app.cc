// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace mojo {
namespace examples {
class EmbeddedApp;

class NavigatorImpl : public InterfaceImpl<Navigator> {
 public:
  explicit NavigatorImpl(EmbeddedApp* app) : app_(app) {}

 private:
  virtual void Navigate(
      uint32 node_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) OVERRIDE;

  EmbeddedApp* app_;
  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class EmbeddedApp
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ViewObserver,
      public NodeObserver {
 public:
  EmbeddedApp()
      : navigator_factory_(this),
        view_manager_(NULL),
        view_manager_client_factory_(this) {
    url::AddStandardScheme("mojo");
  }
  virtual ~EmbeddedApp() {}

  void SetNodeColor(uint32 node_id, SkColor color) {
    pending_node_colors_[node_id] = color;
    ProcessPendingNodeColor(node_id);
  }

 private:

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    // TODO(aa): Weird for embeddee to talk to embedder by URL. Seems like
    // embedder should be able to specify the SP embeddee receives, then
    // communication can be anonymous.
    app->ConnectToService("mojo:mojo_window_manager", &navigator_host_);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&view_manager_client_factory_);
    connection->AddService(&navigator_factory_);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       Node* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    View* view = View::Create(view_manager);
    view->AddObserver(this);
    root->SetActiveView(view);
    root->AddObserver(this);

    roots_[root->id()] = root;
    ProcessPendingNodeColor(root->id());
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == EVENT_TYPE_MOUSE_RELEASED) {
      if (event->flags & EVENT_FLAGS_LEFT_MOUSE_BUTTON) {
        NavigationDetailsPtr nav_details(NavigationDetails::New());
        nav_details->request->url =
            "http://www.aaronboodman.com/z_dropbox/test.html";
        navigator_host_->RequestNavigate(view->node()->id(),
                                         TARGET_SOURCE_NODE,
                                         nav_details.Pass());
      }
    }
  }

  // Overridden from NodeObserver:
  virtual void OnNodeActiveViewChanged(Node* node,
                                       View* old_view,
                                       View* new_view) OVERRIDE {
    if (new_view == 0)
      views_to_reap_[node] = old_view;
  }
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    DCHECK(roots_.find(node->id()) != roots_.end());
    roots_.erase(node->id());
    std::map<Node*, View*>::const_iterator it = views_to_reap_.find(node);
    if (it != views_to_reap_.end())
      it->second->Destroy();
  }

  void ProcessPendingNodeColor(uint32 node_id) {
    RootMap::iterator root = roots_.find(node_id);
    if (root == roots_.end())
      return;

    PendingNodeColors::iterator color = pending_node_colors_.find(node_id);
    if (color == pending_node_colors_.end())
      return;

    root->second->active_view()->SetColor(color->second);
    pending_node_colors_.erase(color);
  }

  InterfaceFactoryImplWithContext<NavigatorImpl, EmbeddedApp>
      navigator_factory_;

  ViewManager* view_manager_;
  NavigatorHostPtr navigator_host_;
  std::map<Node*, View*> views_to_reap_;
  ViewManagerClientFactory view_manager_client_factory_;

  typedef std::map<Id, Node*> RootMap;
  RootMap roots_;

  // We can receive navigations for nodes we don't have yet.
  typedef std::map<uint32, SkColor> PendingNodeColors;
  PendingNodeColors pending_node_colors_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApp);
};

void NavigatorImpl::Navigate(uint32 node_id,
                             NavigationDetailsPtr navigation_details,
                             ResponseDetailsPtr response_details) {
  GURL url(navigation_details->request->url.To<std::string>());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is invalid.";
    return;
  }
  // TODO(aa): Verify new URL is same origin as current origin.
  SkColor color = 0x00;
  if (!base::HexStringToUInt(url.path().substr(1), &color)) {
    LOG(ERROR) << "Invalid URL, path not convertible to integer";
    return;
  }
  app_->SetNodeColor(node_id, color);
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::EmbeddedApp;
}

}  // namespace mojo
