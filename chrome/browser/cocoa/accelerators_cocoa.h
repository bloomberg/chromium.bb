// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_ACCELERATORS_COCOA_H_
#define CHROME_BROWSER_COCOA_ACCELERATORS_COCOA_H_
#pragma once

#include <map>

#include "app/menus/accelerator_cocoa.h"

// This class maintains a map of command_ids to AcceleratorCocoa objects (see
// chrome/app/chrome_dll_resource.h). Currently, this only lists the commands
// that are used in the Wrench menu.
//
// It is recommended that this class be used as a singleton so that the key map
// isn't created multiple places.
//
//   #import "base/singleton.h"
//   ...
//   AcceleratorsCocoa* keymap = Singleton<AcceleratorsCocoa>::get();
//   return keymap->GetAcceleratorForCommand(IDC_COPY);
//
class AcceleratorsCocoa {
 public:
  AcceleratorsCocoa();
  ~AcceleratorsCocoa() {}

  typedef std::map<int, menus::AcceleratorCocoa> AcceleratorCocoaMap;

  // Returns NULL if there is no accelerator for the command.
  const menus::AcceleratorCocoa* GetAcceleratorForCommand(int command_id);

 private:
  AcceleratorCocoaMap accelerators_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorsCocoa);
};

#endif  // CHROME_BROWSER_COCOA_ACCELERATORS_COCOA_H_
