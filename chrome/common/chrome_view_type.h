// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_VIEW_TYPE_H_
#define CHROME_COMMON_CHROME_VIEW_TYPE_H_
#pragma once

#include "content/public/common/view_type.h"

namespace chrome {

// Indicates different types of views.
enum ViewType {
  VIEW_TYPE_CHROME_START = content::VIEW_TYPE_CONTENT_END,

  VIEW_TYPE_BACKGROUND_CONTENTS,
  VIEW_TYPE_EXTENSION_BACKGROUND_PAGE,
  VIEW_TYPE_EXTENSION_POPUP,
  VIEW_TYPE_EXTENSION_INFOBAR,
  VIEW_TYPE_NOTIFICATION,
  VIEW_TYPE_EXTENSION_DIALOG,
};

// Constant strings corresponding to the Type enumeration values.  Used
// when converting JS arguments.
extern const char kViewTypeTabContents[];
extern const char kViewTypeBackgroundPage[];
extern const char kViewTypePopup[];
extern const char kViewTypeInfobar[];
extern const char kViewTypeNotification[];
extern const char kViewTypeExtensionDialog[];
extern const char kViewTypeAll[];

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_VIEW_TYPE_H_
