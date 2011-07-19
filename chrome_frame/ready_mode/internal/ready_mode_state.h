// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_STATE_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_STATE_H_
#pragma once

// Allows the UI element to signal the user's response to a Ready Mode prompt.
class ReadyModeState {
 public:
  virtual ~ReadyModeState() {}

  // Indicates that the user has temporarily declined the product.
  virtual void TemporarilyDeclineChromeFrame() = 0;

  // Indicates that the user has permanently declined the product.
  virtual void PermanentlyDeclineChromeFrame() = 0;

  // Indicates that the user has accepted the product.
  virtual void AcceptChromeFrame() = 0;
};  // class ReadyModeState

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_STATE_H_
