// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MULTI_DISPLAY_MANAGER_TEST_API_H_
#define ASH_TEST_MULTI_DISPLAY_MANAGER_TEST_API_H_

#include <string>

#include "base/basictypes.h"

namespace ash {
namespace internal {
class MultiDisplayManager;
}  // internal

namespace test {

class MultiDisplayManagerTestApi {
 public:
  explicit MultiDisplayManagerTestApi(
      internal::MultiDisplayManager* display_manager);
  virtual ~MultiDisplayManagerTestApi();

  // Update the display configuration as given in |display_specs|. The format of
  // |display_spec| is a list of comma separated spec for each displays. Please
  // refer to the comment in |aura::DisplayManager::CreateDisplayFromSpec| for
  // the format of the display spec.
  void UpdateDisplay(const std::string& display_specs);

 private:
  internal::MultiDisplayManager* display_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(MultiDisplayManagerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_MULTI_DISPLAY_MANAGER_TEST_API_H_
