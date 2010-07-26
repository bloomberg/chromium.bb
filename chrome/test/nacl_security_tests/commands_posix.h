// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_SECURITY_TESTS_COMMANDS_POSIX_H_
#define CHROME_TEST_NACL_SECURITY_TESTS_COMMANDS_POSIX_H_
#pragma once

// TODO(jvoung): factor out the SboxTestResult from
// sandbox/tests/common/controller.h
// to make it OS independent.

namespace sandbox {

#define SEVERITY_INFO_FLAGS   0x40000000
#define SEVERITY_ERROR_FLAGS  0xC0000000
#define CUSTOMER_CODE         0x20000000
#define SBOX_TESTS_FACILITY   0x05B10000

enum SboxTestResult {
  SBOX_TEST_FIRST_RESULT = 8998,
  SBOX_TEST_SUCCEEDED,
  SBOX_TEST_PING_OK,
  SBOX_TEST_FIRST_INFO = SBOX_TEST_FIRST_RESULT | SEVERITY_INFO_FLAGS,
  SBOX_TEST_DENIED,     // Access was denied.
  SBOX_TEST_NOT_FOUND,  // The resource was not found.
  SBOX_TEST_FIRST_ERROR = SBOX_TEST_FIRST_RESULT | SEVERITY_ERROR_FLAGS,
  SBOX_TEST_INVALID_PARAMETER,
  SBOX_TEST_FAILED_TO_RUN_TEST,
  SBOX_TEST_FAILED_TO_EXECUTE_COMMAND,
  SBOX_TEST_TIMED_OUT,
  SBOX_TEST_FAILED,
  SBOX_TEST_LAST_RESULT
};

// Sandbox access tests for Mac and Linux
// (mimic'ing "sandbox/tests/validation_tests/commands.h")

SboxTestResult TestOpenReadFile(const char* path);

SboxTestResult TestOpenWriteFile(const char* path);

SboxTestResult TestCreateProcess(const char* path);

SboxTestResult TestConnect(const char* url);

// Dummy test that returns SBOX_TEST_SUCCEEDED
// (so it fails, because everything should be denied).
SboxTestResult TestDummyFails();

}  // namespace sandbox

#endif  // CHROME_TEST_NACL_SECURITY_TESTS_COMMANDS_POSIX_H_

