// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_MOCK_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_MOCK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/component_updater/component_patcher.h"

namespace base {
class FilePath;
}

namespace component_updater {

class MockComponentPatcher : public ComponentPatcher {
 public:
  MockComponentPatcher() {}
  virtual ComponentUnpacker::Error Patch(PatchType patch_type,
                                         const base::FilePath& input_file,
                                         const base::FilePath& patch_file,
                                         const base::FilePath& output_file,
                                         int* error) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(MockComponentPatcher);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_MOCK_H_
