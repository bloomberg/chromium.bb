// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple interface for something that persists state.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_STATE_WRITER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_STATE_WRITER_H_
#pragma once

#include <string>

namespace sync_notifier {

class StateWriter {
 public:
  virtual ~StateWriter() {}

  virtual void WriteState(const std::string& state) = 0;
};

}  // sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_STATE_WRITER_H_
