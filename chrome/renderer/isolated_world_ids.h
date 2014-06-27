// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ISOLATED_WORLD_IDS_H_
#define CHROME_RENDERER_ISOLATED_WORLD_IDS_H_

namespace chrome {

enum IsolatedWorldIDs {
  // Chrome can not use ID 0 for isolated world.
  ISOLATED_WORLD_ID_GLOBAL = 0,
  // Isolated world ID for Chrome Translate.
  ISOLATED_WORLD_ID_TRANSLATE,

  // Numbers for isolated worlds for extensions are set in
  // extensions/renderer/script_injection.cc, and are are greater than or equal
  // to this number.
  ISOLATED_WORLD_ID_EXTENSIONS
};

}  // namespace chrome

#endif  // CHROME_RENDERER_ISOLATED_WORLD_IDS_H_
