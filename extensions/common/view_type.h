// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_VIEW_TYPE_H_
#define EXTENSIONS_COMMON_VIEW_TYPE_H_

#include <string>

namespace extensions {

// Icky RTTI used by a few systems to distinguish the host type of a given
// WebContents.
//
// TODO(aa): Remove this and teach those systems to keep track of their own
// data.
enum ViewType {
  VIEW_TYPE_INVALID,
  VIEW_TYPE_APP_WINDOW,
  VIEW_TYPE_BACKGROUND_CONTENTS,
  VIEW_TYPE_COMPONENT,  // For custom parts of Chrome if no other type applies.
  VIEW_TYPE_EXTENSION_BACKGROUND_PAGE,
  VIEW_TYPE_EXTENSION_DIALOG,
  VIEW_TYPE_EXTENSION_GUEST,
  VIEW_TYPE_EXTENSION_POPUP,
  VIEW_TYPE_LAUNCHER_PAGE,
  VIEW_TYPE_PANEL,
  VIEW_TYPE_TAB_CONTENTS,
  VIEW_TYPE_LAST = VIEW_TYPE_TAB_CONTENTS
};

// Matches the |view_type| to the corresponding ViewType, and populates
// |view_type_out|. Returns true if a match is found.
bool GetViewTypeFromString(const std::string& view_type,
                           ViewType* view_type_out);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VIEW_TYPE_H_
