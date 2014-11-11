// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_COMPONENT_CLD_DATA_HARNESS_H_
#define CHROME_BROWSER_TRANSLATE_COMPONENT_CLD_DATA_HARNESS_H_

#include "base/macros.h"
#include "chrome/browser/translate/cld_data_harness.h"

namespace test {

// Utility class that sets up a test harness suitable for injecting a
// component-updater-based CLD data file into the runtime. See CldDataHarness
// class for more details.
class ComponentCldDataHarness : public CldDataHarness {
 public:
  ComponentCldDataHarness() {}
  ~ComponentCldDataHarness() override;
  void Init() override;

 private:
  void ClearComponentDataFileState();
  void GetExtractedComponentDestination(base::FilePath*);
  void GetComponentDataFileDestination(base::FilePath*);
  void DeleteComponentTree();
  void CopyComponentTree();

  DISALLOW_COPY_AND_ASSIGN(ComponentCldDataHarness);
};

}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_COMPONENT_CLD_DATA_HARNESS_H_
