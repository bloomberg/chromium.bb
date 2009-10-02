// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_VIEW_TYPES_H_
#define CHROME_COMMON_VIEW_TYPES_H_

#include "base/basictypes.h"

// Indicates different types of views
class ViewType {
 public:
  enum Type {
    INVALID,
    TAB_CONTENTS,
    EXTENSION_TOOLSTRIP,
    EXTENSION_MOLE,
    EXTENSION_BACKGROUND_PAGE,
    EXTENSION_POPUP,
    DEV_TOOLS_UI,
    INTERSTITIAL_PAGE,
  };

 private:
  // This class is for scoping only, so you shouldn't create an instance of it.
  ViewType() {}

  DISALLOW_COPY_AND_ASSIGN(ViewType);
};

#endif  // CHROME_COMMON_VIEW_TYPES_H_
