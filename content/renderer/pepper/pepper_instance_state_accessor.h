// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"

namespace content {

// This interface provides functions for querying instance state for resource
// host implementations. It allows us to mock out some of these interactions
// for testing, as well as limit the dependencies on the webkit layer.
class PepperInstanceStateAccessor {
 public:
  virtual ~PepperInstanceStateAccessor() {}

  // Returns true if the given instance is valid for the host.
  virtual bool IsValidInstance(PP_Instance instance) = 0;

  // Returns true if the given instance is considered to be currently
  // processing a user gesture or the plugin module has the "override user
  // gesture" flag set (in which case it can always do things normally
  // restricted by user gestures). Returns false if the instance is invalid or
  // if there is no current user gesture.
  virtual bool HasUserGesture(PP_Instance instance) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_H_
