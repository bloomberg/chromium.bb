// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_
#pragma once

#include <stddef.h>

#include "ui/base/keycodes/keyboard_codes.h"

// This contains the list of accelerators for the Aura implementation.
namespace browser {

struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

// The list of accelerators.
extern const AcceleratorMapping kAcceleratorMap[];

// The numbers of elements in kAcceleratorMap.
extern const size_t kAcceleratorMapLength;

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_
