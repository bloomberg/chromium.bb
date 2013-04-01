// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_VIEW_TYPE_H_
#define EXTENSIONS_COMMON_VIEW_TYPE_H_

namespace extensions {

// Icky RTTI used by a few systems to distinguish the host type of a given
// WebContents.
//
// TODO(aa): Remove this and teach those systems to keep track of their own
// data.
enum ViewType {
  VIEW_TYPE_INVALID,
  VIEW_TYPE_APP_SHELL,
  VIEW_TYPE_BACKGROUND_CONTENTS,
  VIEW_TYPE_EXTENSION_BACKGROUND_PAGE,
  VIEW_TYPE_EXTENSION_DIALOG,
  VIEW_TYPE_EXTENSION_INFOBAR,
  VIEW_TYPE_EXTENSION_POPUP,
  // TODO(jam): remove this once http://crbug.com/137297 is fixed and HTML5
  // notifications don't use WebContents.
  VIEW_TYPE_NOTIFICATION,
  VIEW_TYPE_PANEL,
  VIEW_TYPE_TAB_CONTENTS,
};

// Constant strings corresponding to the Type enumeration values.  Used
// when converting JS arguments.
extern const char kViewTypeAll[];
extern const char kViewTypeAppShell[];
extern const char kViewTypeBackgroundPage[];
extern const char kViewTypeExtensionDialog[];
extern const char kViewTypeInfobar[];
extern const char kViewTypeNotification[];
extern const char kViewTypePanel[];
extern const char kViewTypePopup[];
extern const char kViewTypeTabContents[];

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VIEW_TYPE_H_
