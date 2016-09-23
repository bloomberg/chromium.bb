// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_GUEST_MODE_H_
#define CONTENT_PUBLIC_RENDERER_GUEST_MODE_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT GuestMode {
 public:
  // Returns true if guests could be using cross process frames.
  static bool UseCrossProcessFramesForGuests();

 private:
  GuestMode();

  DISALLOW_COPY_AND_ASSIGN(GuestMode);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_GUEST_MODE_H_
