// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_SCREEN_LIST_H_
#define API_IMPL_SCREEN_LIST_H_

#include <vector>

#include "api/public/screen_info.h"

namespace openscreen {

class ScreenList {
 public:
  ScreenList();
  ~ScreenList();
  ScreenList(ScreenList&&) = delete;
  ScreenList& operator=(ScreenList&&) = delete;

  void OnScreenAdded(const ScreenInfo& info);

  // Returns true if |info.screen_id| matched an item in |screens_| and was
  // therefore changed, otherwise false.
  bool OnScreenChanged(const ScreenInfo& info);

  // Returns true if an item matching |info| was removed from |screens_|,
  // otherwise false.
  bool OnScreenRemoved(const ScreenInfo& info);

  // Returns true if |screens_| was not empty before this call, otherwise false.
  bool OnAllScreensRemoved();

  const std::vector<ScreenInfo>& screens() const { return screens_; }

 private:
  std::vector<ScreenInfo> screens_;
};

}  // namespace openscreen

#endif  // API_IMPL_SCREEN_LIST_H_
