// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_FIND_BAR_DELEGATE_H_
#define MANDOLINE_UI_DESKTOP_UI_FIND_BAR_DELEGATE_H_

#include <string>

namespace mandoline {

class FindBarDelegate {
 public:
  virtual ~FindBarDelegate() {}

  virtual void OnDoFind(const std::string& find) = 0;

  virtual void OnHideFindBar() = 0;
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_FIND_BAR_DELEGATE_H_
