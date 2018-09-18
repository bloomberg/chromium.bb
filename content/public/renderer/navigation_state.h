// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H_
#define CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// TODO(dgozman): Remove this class, once public DocumentState
// does not reference it.
class CONTENT_EXPORT NavigationState {
 public:
  virtual ~NavigationState();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H
