// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_

#include <map>

#include "ui/base/accelerators/accelerator.h"

template <typename T> struct DefaultSingletonTraits;

// This class maintains a map of command_ids to Accelerator objects (see
// chrome/app/chrome_command_ids.h). Currently, this only lists the commands
// that are used in the Wrench menu.
//
// It is recommended that this class be used as a singleton so that the key map
// isn't created multiple places.
//
//   #import "base/memory/singleton.h"
//   ...
//   AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
//   return keymap->GetAcceleratorForCommand(IDC_COPY);
//
class AcceleratorsCocoa {
 public:
  typedef std::map<int, ui::Accelerator> AcceleratorMap;

  // Returns NULL if there is no accelerator for the command.
  const ui::Accelerator* GetAcceleratorForCommand(int command_id);

  // Returns the singleton instance.
  static AcceleratorsCocoa* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AcceleratorsCocoa>;

  AcceleratorsCocoa();
  ~AcceleratorsCocoa();

  AcceleratorMap accelerators_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorsCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
