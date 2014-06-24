// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_COMPONENT_CLD_DATA_HARNESS_H_
#define CHROME_BROWSER_TRANSLATE_COMPONENT_CLD_DATA_HARNESS_H_

#include "base/macros.h"
#include "chrome/browser/translate/cld_data_harness.h"

namespace test {

class ComponentCldDataHarness : public CldDataHarness {
 public:
  ComponentCldDataHarness();
  virtual ~ComponentCldDataHarness();
  virtual void Init() OVERRIDE;

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
