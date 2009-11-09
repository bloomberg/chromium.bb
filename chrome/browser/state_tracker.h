// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATE_TRACKER_H_
#define CHROME_BROWSER_STATE_TRACKER_H_

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"

// StateTracker maintains a circular character buffer. It is intended for use in
// tracking down crashes. As various events occur invoke Append. When you're
// ready to crash, invoke Crash, which copies the state onto
// the state and CHECKs.
// The '!' in the array indicates the position the next character is inserted.
// The string of characters before the '!' gives the most recent strings that
// were appended.
class StateTracker {
 public:
  StateTracker();

  // Appends |text|.
  void Append(const std::string& text);

  // Copies the appended text onto the stack and CHECKs.
  void Crash();

 private:
  // Current index into the string content is inserted at.
  size_t index_;

  // The content.
  scoped_array<char> content_;

  DISALLOW_COPY_AND_ASSIGN(StateTracker);
};

#endif  // CHROME_BROWSER_STATE_TRACKER_H_
