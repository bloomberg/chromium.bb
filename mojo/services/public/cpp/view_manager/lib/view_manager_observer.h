// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_OBSERVER_H_

namespace mojo {
namespace services {
namespace view_manager {

class ViewManagerObserver {
 public:
  // Called when a commit is made to the service.
  virtual void OnCommit(ViewManager* manager) = 0;

  // Called when a response was received to a prior commit.
  virtual void OnCommitResponse(ViewManager* manager, bool success) = 0;

 protected:
  virtual ~ViewManagerObserver() {}
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_OBSERVER_H_
