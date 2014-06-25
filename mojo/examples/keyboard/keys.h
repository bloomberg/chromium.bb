// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_KEYBOARD_KEYS_H_
#define MOJO_EXAMPLES_KEYBOARD_KEYS_H_

#include <vector>

#include "base/basictypes.h"

namespace mojo {
namespace examples {

enum SpecialKey {
  SPECIAL_KEY_SHIFT = -1,
  SPECIAL_KEY_NUMERIC = -2,
  SPECIAL_KEY_ALPHA = -3,
};

struct Key {
  int keyboard_code() const {
    // Handling of keycodes differs between in windows and others.
#if defined(OS_WIN)
    return generated_code ? generated_code : display_code;
#else
    return display_code;
#endif
  }

  // Code used to get the value to display in the UI. This is either a
  // KeyboardCode or a SpecialKey.
  int display_code;

  // How much space (as a percentage) the key is to take up.
  float size;

  // Any ui::EventFlags that are required to produce the key.
  int event_flags;

  // If non-zero KeyboardCode to generate. If 0 use the |display_code|.
  int generated_code;
};

struct Row {
  float padding;
  const Key* keys;
  size_t num_keys;
};

// Returns the rows for a qwerty style keyboard. The returned values are owned
// by this object and should not be deleted.
std::vector<const Row*> GetQWERTYRows();

// Returns the rows for a numeric keyboard. The returned values are owned
// by this object and should not be deleted.
std::vector<const Row*> GetNumericRows();

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_KEYBOARD_KEYS_H_
