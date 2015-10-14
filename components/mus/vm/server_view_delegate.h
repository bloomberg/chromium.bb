// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_VM_SERVER_VIEW_DELEGATE_H_
#define COMPONENTS_MUS_VM_SERVER_VIEW_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"

namespace mus {

class ServerView;
class SurfacesState;
struct ViewId;

class ServerViewDelegate {
 public:
  virtual SurfacesState* GetSurfacesState() = 0;

  virtual void OnScheduleViewPaint(const ServerView* view) = 0;

  // Returns the root of the view tree to which this |view| is attached. Returns
  // null if this view is not attached up through to a root view.
  virtual const ServerView* GetRootView(const ServerView* view) const = 0;

 protected:
  virtual ~ServerViewDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_VM_SERVER_VIEW_DELEGATE_H_
