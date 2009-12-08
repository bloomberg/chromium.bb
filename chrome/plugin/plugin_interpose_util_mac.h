// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_
#define CHROME_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_

#include <Carbon/Carbon.h>

#import "base/basictypes.h"

namespace mac_plugin_interposing {

// Swizzles methods we need to watch in order to manage process and window
// focus correctly.
void SetUpCocoaInterposing();

// Brings the plugin process to the front so that the user can see its windows.
void SwitchToPluginProcess();

// Sends a message to the browser process to inform it that the given window
// has been brought forward.
void NotifyBrowserOfPluginSelectWindow(uint32 window_id, CGRect bounds,
                                       bool modal);

// Sends a message to the browser process to inform it that the given window
// has been shown.
void NotifyBrowserOfPluginShowWindow(uint32 window_id, CGRect bounds,
                                     bool modal);

// Sends a message to the browser process to inform it that the given window
// has been hidden, and switches focus back to the browser process if there are
// no remaining plugin windows.
void NotifyBrowserOfPluginHideWindow(uint32 window_id, CGRect bounds);

}  // namespace MacPluginInterpose

#endif  // CHROME_PLUGIN_PLUGIN_INTERPOSE_UTIL_MAC_H_
