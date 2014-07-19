// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SYSTEM_HOTKEY_MAP_H_
#define CHROME_BROWSER_UI_COCOA_SYSTEM_HOTKEY_MAP_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "base/macros.h"

struct SystemHotkey;

// Maintains a listing of all OSX user modifiable hotkeys. e.g. (cmd + `)
class SystemHotkeyMap {
 public:
  SystemHotkeyMap();
  ~SystemHotkeyMap();

  // Parses the property list data commonly stored at
  // ~/Library/Preferences/com.apple.symbolichotkeys.plist
  // Returns false on encountering an irrecoverable error.
  // Can be called multiple times. Only the results from the most recent
  // invocation are stored.
  bool ParseData(NSData* data);

  // Whether the hotkey has been reserved by the user.
  bool IsHotkeyReserved(int key_code, int modifiers);

 private:
  // Create at least one record of a hotkey that is reserved by the user.
  // Certain system hotkeys automatically reserve multiple key combinations.
  void ReserveHotkey(int key_code, int modifiers, NSString* system_effect);

  // Create a record of a hotkey that is reserved by the user.
  void ReserveHotkey(int key_code, int modifiers);

  std::vector<SystemHotkey> system_hotkeys_;

  DISALLOW_COPY_AND_ASSIGN(SystemHotkeyMap);
};

#endif  // CHROME_BROWSER_UI_COCOA_SYSTEM_HOTKEY_MAP_H_
