// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

// WindowActivityTracker is an interface that can be implememented to report
// whether the user is actively interacting with UI.
class CONTENT_EXPORT WindowActivityTracker {
 public:
  virtual ~WindowActivityTracker() {}

  // Returns true if UI interaction is active.
  virtual bool IsUiInteractionActive() const = 0;

  // Resets any previous UI activity tracked.
  virtual void Reset() = 0;

  // Returns a weak pointer.
  virtual base::WeakPtr<WindowActivityTracker> GetWeakPtr() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_H_
