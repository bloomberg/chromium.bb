// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_NACL_SANDBOX_TEST_H_
#define CHROME_TEST_NACL_NACL_SANDBOX_TEST_H_
#pragma once

#include "chrome/test/nacl/nacl_test.h"

class FilePath;

// This class implements integration tests for Native Client.
// Specifically, this tests that the sandbox is active by the time a
// NaCl module is loaded. To force a NaCl module to be loaded,
// it will navigate to  a page containing a simple NaCl module.
// The NaCl module itself won't exercise the sandbox, but a DLL will be
// loaded into the same process that does exercise the sandbox.
class NaClSandboxTest : public NaClTest {
 protected:
  NaClSandboxTest();
  virtual ~NaClSandboxTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(NaClSandboxTest);
};

#endif  // CHROME_TEST_NACL_NACL_SANDBOX_TEST_H_
