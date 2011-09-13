// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_TYPES_H_
#define CONTENT_COMMON_VIEW_TYPES_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

// Indicates different types of views
class CONTENT_EXPORT ViewType {
 public:
  enum Type {
    INVALID,
    BACKGROUND_CONTENTS,
    TAB_CONTENTS,
    EXTENSION_BACKGROUND_PAGE,
    EXTENSION_POPUP,
    EXTENSION_INFOBAR,
    DEV_TOOLS_UI,
    INTERSTITIAL_PAGE,
    NOTIFICATION,
    EXTENSION_DIALOG,
  };

  // Constant strings corresponding to the Type enumeration values.  Used
  // when converting JS arguments.
  static const char* const kTabContents;
  static const char* const kBackgroundPage;
  static const char* const kPopup;
  static const char* const kInfobar;
  static const char* const kNotification;
  static const char* const kExtensionDialog;
  static const char* const kAll;

 private:
  // This class is for scoping only, so you shouldn't create an instance of it.
  ViewType() {}

  DISALLOW_COPY_AND_ASSIGN(ViewType);
};

#endif  // CONTENT_COMMON_VIEW_TYPES_H_
