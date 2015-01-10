// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides class that serves as a public interface for a
// derived object that can size a guest.

// This class currently only serves as a base class for BrowserPluginGuest, and
// its API can only be accessed by a BrowserPluginGuestDelegate. This module
// will go away once the migration to RenderFrame architecture has completed
// (http://crbug.com/330264).

#ifndef CONTENT_PUBLIC_BROWSER_GUEST_SIZER_H_
#define CONTENT_PUBLIC_BROWSER_GUEST_SIZER_H_

#include "ui/gfx/geometry/size.h"

namespace content {

class GuestSizer {
 public:
  virtual void SizeContents(const gfx::Size& new_size) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GUEST_SIZER_H_
