// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_DELEGATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_DELEGATE_H_

#include <string>

#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mojo {

class View;
class ViewTreeConnection;

// Interface implemented by an application using the view manager.
//
// Each call to OnEmbed() results in a new ViewTreeConnection and new root View.
// ViewTreeConnection is deleted by any of the following:
// . If the root of the connection is destroyed. This happens if the owner
//   of the root Embed()s another app in root, or the owner explicitly deletes
//   root.
// . The connection to the view manager is lost.
// . Explicitly by way of calling delete.
//
// When the ViewTreeConnection is deleted all views are deleted (and observers
// notified). This is followed by notifying the delegate by way of
// OnConnectionLost().
class ViewTreeDelegate {
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

  // Called from the destructor of ViewTreeConnection after all the Views have
  // been destroyed. |connection| is no longer valid after this call.
  virtual void OnConnectionLost(ViewTreeConnection* connection) = 0;

 protected:
  virtual ~ViewTreeDelegate() {}
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_DELEGATE_H_
