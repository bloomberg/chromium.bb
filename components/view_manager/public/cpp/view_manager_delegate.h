// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_DELEGATE_H_

#include <string>

#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mojo {

class View;
class ViewManager;

// Interface implemented by an application using the view manager.
//
// Each call to OnEmbed() results in a new ViewManager and new root View.
// ViewManager is deleted by any of the following:
// . If the root of the connection is destroyed. This happens if the owner
//   of the root Embed()s another app in root, or the owner explicitly deletes
//   root.
// . The connection to the viewmanager is lost.
// . Explicitly by way of calling delete.
//
// When the ViewManager is deleted all views are deleted (and observers
// notified). This is followed by notifying the delegate by way of
// OnViewManagerDestroyed().
class ViewManagerDelegate {
 public:
  // Called when the application implementing this interface is embedded at
  // |root|.
  //
  // |services| exposes the services offered by the embedder to the delegate.
  //
  // |exposed_services| is an object that the delegate can add services to
  // expose to the embedder.
  //
  // Note that if a different application is subsequently embedded at |root|,
  // the pipes connecting |services| and |exposed_services| to the embedder and
  // any services obtained from them are not broken and will continue to be
  // valid.
  virtual void OnEmbed(View* root) = 0;

  // Sent when another app is embedded in the same View as this connection.
  // Subsequently the root View and this object are destroyed (observers are
  // notified appropriately).
  virtual void OnUnembed();

  // Only invoked if the connection has been marked as an embed root. This
  // allows the delegate to disallow the embed (return false), or change
  // the ServiceProviders that would be exposed to the new client.
  //
  // This implementation returns true (allowing the embed), and does not alter
  // the supplied ServiceProviders.
  //
  // See the mojom for more details.
  virtual void OnEmbedForDescendant(View* view,
                                    URLRequestPtr request,
                                    ViewManagerClientPtr* client);

  // Called from the destructor of ViewManager after all the Views have been
  // destroyed. |view_manager| is no longer valid after this call.
  virtual void OnViewManagerDestroyed(ViewManager* view_manager) = 0;

 protected:
  virtual ~ViewManagerDelegate() {}
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_DELEGATE_H_
