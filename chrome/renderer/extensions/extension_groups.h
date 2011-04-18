// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_GROUPS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_GROUPS_H_
#pragma once

// A set of extension groups for use with WebKit::registerExtension and
// WebFrame::ExecuteScriptInNewWorld to control which extensions get loaded
// into which contexts.
enum ExtensionGroups {
  // Use this to mark extensions to be loaded into content scripts only.
  EXTENSION_GROUP_CONTENT_SCRIPTS = 1,
};


#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_GROUPS_H_
