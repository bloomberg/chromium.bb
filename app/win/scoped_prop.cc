// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/win/scoped_prop.h"

#include "base/win_util.h"

namespace app {

namespace win {

ScopedProp::ScopedProp(HWND hwnd, const std::wstring& key, HANDLE data)
    : hwnd_(hwnd),
      key_(key) {
  BOOL result = SetProp(hwnd, key.c_str(), data);
  // Failure to set a propery on a window is typically fatal. It means someone
  // is going to ask for the property and get NULL. So, rather than crash later
  // on when someone expects a non-NULL value we crash here in hopes of
  // diagnosing the failure.
  CHECK(result) << win_util::FormatLastWin32Error();
}

ScopedProp::~ScopedProp() {
  DCHECK(IsWindow(hwnd_));
  RemoveProp(hwnd_, key_.c_str());
}


}  // namespace win

}  // namespace app
