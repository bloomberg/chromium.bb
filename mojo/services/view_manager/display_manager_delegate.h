// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_DELEGATE_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace service {

class MOJO_VIEW_MANAGER_EXPORT DisplayManagerDelegate {
 public:
  // Invoked when the WindowTreeHost is ready.
  virtual void OnDisplayManagerWindowTreeHostCreated() = 0;

 protected:
  virtual ~DisplayManagerDelegate() {}
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_DELEGATE_H_
