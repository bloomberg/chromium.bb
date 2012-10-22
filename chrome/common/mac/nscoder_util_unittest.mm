// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/common/mac/nscoder_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

typedef PlatformTest NSCoderStdStringTest;

const char* testStrings[] = {
  "Arf",
  "",
  "This is working™",
  "古池や蛙飛込む水の音\nふるいけやかわずとびこむみずのおと",
  "ἀγεωμέτρητος μηδεὶς εἰσίτω",
  "Bang!\t\n"
};

TEST_F(NSCoderStdStringTest, encodeDecode) {
  for (size_t i = 0; i < arraysize(testStrings); ++i) {
    NSMutableData *data = [NSMutableData data];

    scoped_nsobject<NSKeyedArchiver> archiver(
        [[NSKeyedArchiver alloc] initForWritingWithMutableData:data]);
    nscoder_util::EncodeString(archiver, @"test", testStrings[i]);
    [archiver finishEncoding];

    scoped_nsobject<NSKeyedUnarchiver> unarchiver(
        [[NSKeyedUnarchiver alloc] initForReadingWithData:data]);
    const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

    EXPECT_EQ(decoded, testStrings[i]);
  }
}

TEST_F(NSCoderStdStringTest, decodeEmpty) {
  NSMutableData *data = [NSMutableData data];

  scoped_nsobject<NSKeyedArchiver> archiver(
      [[NSKeyedArchiver alloc] initForWritingWithMutableData:data]);
  [archiver finishEncoding];

  scoped_nsobject<NSKeyedUnarchiver> unarchiver(
      [[NSKeyedUnarchiver alloc] initForReadingWithData:data]);
  const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

  EXPECT_EQ(decoded, "");
}

}  // namespace
