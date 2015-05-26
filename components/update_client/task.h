// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/update_client/update_client.h"

namespace update_client {

class Task;

// Defines an abstraction for a unit of work done by the update client.
// Each invocation of the update client API results in a task being created and
// run. In most cases, a task corresponds to a set of CRXs, which are updated
// together.
class Task {
 public:
  virtual ~Task() {}

  virtual void Run() = 0;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_H_
