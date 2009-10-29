// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection.h"

#include "base/gfx/size.h"
#include "base/logging.h"

Balloon* BalloonCollectionImpl::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  // TODO(johnnyg): http://crbug.com/23954.  Part of future Linux support.
  NOTIMPLEMENTED();
  return NULL;
}

bool BalloonCollectionImpl::Layout::RefreshSystemMetrics() {
  // TODO(johnnyg): http://crbug.com/23954.  Part of future Linux support.
  NOTIMPLEMENTED();
  return false;
}
