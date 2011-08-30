// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_TEST_SUITE_H_
#define AURA_TEST_SUITE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

class AuraTestSuite : public base::TestSuite {
 public:
  AuraTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
};

#endif  // AURA_TEST_SUITE_H_
