// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_
#define CONTENT_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_

namespace mac_plugin_interposing {

// Swizzles methods we need to watch in order to manage process and window
// focus correctly.
void SetUpCocoaInterposing();

}  // namespace MacPluginInterpose

#endif  // CONTENT_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_
