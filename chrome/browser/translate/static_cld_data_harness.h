// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_STATIC_CLD_DATA_HARNESS_H_
#define CHROME_BROWSER_TRANSLATE_STATIC_CLD_DATA_HARNESS_H_

#include "chrome/browser/translate/cld_data_harness.h"

namespace test {

class StaticCldDataHarness : public CldDataHarness {
 public:
  StaticCldDataHarness();
  virtual ~StaticCldDataHarness();
  virtual void Init() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StaticCldDataHarness);
};

}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_STATIC_CLD_DATA_HARNESS_H_
