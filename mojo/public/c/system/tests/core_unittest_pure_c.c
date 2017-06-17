// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef __cplusplus
#error "This file should be compiled as C, not C++."
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Include all the header files that are meant to be compilable as C. Start with
// core.h, since it's the most important one.
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/macros.h"

// The joys of the C preprocessor....
#define STRINGIFY(x) #x
#define STRINGIFY2(x) STRINGIFY(x)
#define FAILURE(message) \
  __FILE__ "(" STRINGIFY2(__LINE__) "): Failure: " message

// Makeshift gtest.
#define EXPECT_EQ(a, b)                                                  \
  do {                                                                   \
    if ((a) != (b))                                                      \
      return FAILURE(STRINGIFY(a) " != " STRINGIFY(b) " (expected ==)"); \
  } while (0)
#define EXPECT_NE(a, b)                                                  \
  do {                                                                   \
    if ((a) == (b))                                                      \
      return FAILURE(STRINGIFY(a) " == " STRINGIFY(b) " (expected !=)"); \
  } while (0)

// This function exists mainly to be compiled and linked. We do some cursory
// checks and call it from a unit test, to make sure that link problems aren't
// missed due to deadstripping. Returns null on success and a string on failure
// (describing the failure).
const char* MinimalCTest(void) {
  // MSVS before 2013 *really* only supports C90: All variables must be declared
  // at the top. (MSVS 2013 is more reasonable.)
  MojoTimeTicks ticks;
  MojoHandle handle0, handle1;

  ticks = MojoGetTimeTicksNow();
  EXPECT_NE(ticks, 0);

  handle0 = MOJO_HANDLE_INVALID;
  EXPECT_NE(MOJO_RESULT_OK, MojoClose(handle0));

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(handle0, NULL));

  handle1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(NULL, &handle0, &handle1));

  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(42, NULL, &message));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessageNew(handle0, message, MOJO_WRITE_DATA_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessageNew(handle1, &message,
                                               MOJO_READ_MESSAGE_FLAG_NONE));
  uintptr_t context;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReleaseMessageContext(message, &context));
  EXPECT_EQ(42, context);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handle0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handle1));

  return NULL;
}
