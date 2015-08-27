// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_TREE_HOST_CONNECTION_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_TREE_HOST_CONNECTION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_tree.mojom.h"
#include "components/view_manager/public/interfaces/view_tree_host.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {

class ApplicationConnection;
class ApplicationImpl;
class ViewTreeDelegate;

// ViewTreeHostConnection is used to establish the initial connection to the
// view manager. Use it when you know the view manager is not running and you're
// app is going to be the first one to contact it.
// TODO(beng): determine if we want to change this to be more of a simple
//             wrapper around a host.
class ViewTreeHostConnection {
 public:
  // |host_client| may be null.
  ViewTreeHostConnection(ApplicationImpl* app,
                         ViewTreeDelegate* delegate,
                         ViewTreeHostClient* host_client);
  ~ViewTreeHostConnection();

  // Returns the ViewTreeHost. This is only valid if |host_client| was supplied
  // to the constructor.
  ViewTreeHost* host() { return host_.get(); }

 private:
  ViewTreeHostPtr host_;
  scoped_ptr<Binding<ViewTreeHostClient>> host_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeHostConnection);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_TREE_HOST_CONNECTION_H_
