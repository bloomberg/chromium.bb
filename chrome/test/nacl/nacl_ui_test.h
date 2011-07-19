// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_NACL_UI_TEST_H_
#define CHROME_TEST_NACL_NACL_UI_TEST_H_
#pragma once

#include "chrome/test/nacl/nacl_test.h"

// The actual NaCl ui tests are hooked onto this, so that the base class
// can be reused without running tests that require loading a NaCl module.
class NaClUITest : public NaClTest {
 protected:
  NaClUITest();
  virtual ~NaClUITest();

 private:
  DISALLOW_COPY_AND_ASSIGN(NaClUITest);
};

#endif  // CHROME_TEST_NACL_NACL_UI_TEST_H_
