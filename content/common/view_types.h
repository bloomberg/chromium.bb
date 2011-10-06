// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_TYPES_H_
#define CONTENT_COMMON_VIEW_TYPES_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

// Indicates different types of views
class CONTENT_EXPORT ViewType {
 public:
  enum {
    INVALID,
    TAB_CONTENTS,
    INTERSTITIAL_PAGE,
    DEV_TOOLS_UI,

    VIEW_TYPE_CONTENT_END
  };

  // Use this type to clarify that a ViewType type should be used.
  typedef int Type;

 private:
  // This class is for scoping only, so you shouldn't create an instance of it.
  ViewType() {}

  DISALLOW_COPY_AND_ASSIGN(ViewType);
};

}  // namespace content

#endif  // CONTENT_COMMON_VIEW_TYPES_H_
