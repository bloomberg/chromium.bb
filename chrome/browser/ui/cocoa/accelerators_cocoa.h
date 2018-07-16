// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// This class maintains a map of command_ids to Accelerator objects (see
// chrome/app/chrome_command_ids.h). Currently, this only lists the commands
// that are used in the App menu.
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
  typedef std::vector<ui::Accelerator> AcceleratorVector;
  typedef AcceleratorMap::const_iterator const_iterator;

  const_iterator const begin() { return accelerators_.begin(); }
  const_iterator const end() { return accelerators_.end(); }

  // Returns NULL if there is no accelerator for the command.
  const ui::Accelerator* GetAcceleratorForCommand(int command_id);
  // Searches the list of accelerators without a command_id for an accelerator
  // that matches the given |key_equivalent| and |modifiers|.
  const ui::Accelerator* GetAcceleratorForHotKey(NSString* key_equivalent,
                                                 NSUInteger modifiers) const;

  // Returns the singleton instance.
  static AcceleratorsCocoa* GetInstance();

  // TODO(erikchen): This shouldn't be necessary because ui::Accelerator has
  // almost the same constructor and contains the same information. Remove this.
  // https://crbug.com/846893.
  // Create a cross platform accelerator given a cross platform |key_code| and
  // the |cocoa_modifiers|.
  static ui::Accelerator AcceleratorFromKeyCode(ui::KeyboardCode key_code,
                                                NSUInteger cocoa_modifiers);

 private:
  friend struct base::DefaultSingletonTraits<AcceleratorsCocoa>;
  FRIEND_TEST_ALL_PREFIXES(AcceleratorsCocoaBrowserTest,
                           MappingAcceleratorsInMainMenu);

  AcceleratorsCocoa();
  ~AcceleratorsCocoa();

  // A map from command_id to Accelerator. The accelerator is fully filled out,
  // and its platform_accelerator is also fully filled out.
  // Contains accelerators from both the app menu and the main menu.
  AcceleratorMap accelerators_;
  // A list of accelerators used in the main menu that have no associated
  // command_id. The accelerator is fully filled out, and its
  // platform_accelerator is also fully filled out.
  AcceleratorVector accelerator_vector_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorsCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
