// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_VIEW_TYPES_H_
#define CHROME_COMMON_CHROME_VIEW_TYPES_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/view_types.h"

namespace chrome {

// Indicates different types of views
class ViewType {
 public:
  enum {
    VIEW_TYPE_CHROME_START = content::ViewType::VIEW_TYPE_CONTENT_END,

    BACKGROUND_CONTENTS,
    EXTENSION_BACKGROUND_PAGE,
    EXTENSION_POPUP,
    EXTENSION_INFOBAR,
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

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_VIEW_TYPES_H_
