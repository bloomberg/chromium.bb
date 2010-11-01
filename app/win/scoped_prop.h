// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_WIN_SCOPED_PROP_H_
#define APP_WIN_SCOPED_PROP_H_
#pragma once

#include <windows.h>

#include "base/logging.h"

namespace app {
namespace win {

// ScopedProp is a wrapper around SetProp. Use ScopedProp rather than SetProp as
// it handles errors conditions for you and forces you to think about
// cleanup. ScopedProp must be destroyed before the window is destroyed, else
// you're going to leak a property, which could lead to failure to set a
// property later on.
class ScopedProp {
 public:
  // Registers the key value pair for the specified window. ScopedProp does not
  // maintain the value, just the key/value pair.
  ScopedProp(HWND hwnd, const std::wstring& key, HANDLE data);
  ~ScopedProp();

  const std::wstring& key() const { return key_; }

 private:
  HWND hwnd_;
  const std::wstring key_;

  DISALLOW_COPY_AND_ASSIGN(ScopedProp);
};

}  // namespace win
}  // namespace app

#endif  // APP_WIN_SCOPED_PROP_H_
