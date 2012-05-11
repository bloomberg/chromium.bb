// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_TEST_SUITE_H_
#define CONTENT_TEST_CONTENT_TEST_SUITE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"
#include "base/win/scoped_com_initializer.h"

#if defined(USE_AURA)
namespace aura {
namespace test {
class TestAuraInitializer;
}  // namespace test
}  // namespace aura
#endif

class ContentTestSuite : public base::TestSuite {
 public:
  ContentTestSuite(int argc, char** argv);
  virtual ~ContentTestSuite();

 protected:
  virtual void Initialize() OVERRIDE;

 private:
  base::win::ScopedCOMInitializer com_initializer_;

#if defined(USE_AURA)
  scoped_ptr<aura::test::TestAuraInitializer> aura_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentTestSuite);
};

#endif  // CONTENT_TEST_CONTENT_TEST_SUITE_H_
