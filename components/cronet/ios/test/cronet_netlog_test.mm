// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

void StartCronetIfNecessary();

class NetLogTest : public ::testing::Test {
 protected:
  NetLogTest() {}
  ~NetLogTest() override {}

  void SetUp() override { StartCronetIfNecessary(); }
};

TEST(NetLogTest, OpenFile) {
  bool netlog_started =
      [Cronet startNetLogToFile:@"cronet_netlog.json" logBytes:YES];
  [Cronet stopNetLog];

  EXPECT_TRUE(netlog_started);
}

TEST(NetLogTest, CreateFile) {
  NSString* filename = [[[NSProcessInfo processInfo] globallyUniqueString]
      stringByAppendingString:@"_netlog.json"];
  bool netlog_started = [Cronet startNetLogToFile:filename logBytes:YES];
  [Cronet stopNetLog];

  bool file_created = [[NSFileManager defaultManager]
      fileExistsAtPath:[Cronet getNetLogPathForFile:filename]];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet getNetLogPathForFile:filename]
                 error:nil];

  EXPECT_TRUE(netlog_started);
  EXPECT_TRUE(file_created);
}

TEST(NetLogTest, NonExistantDir) {
  NSString* notdir = [[[NSProcessInfo processInfo] globallyUniqueString]
      stringByAppendingString:@"/netlog.json"];
  bool netlog_started = [Cronet startNetLogToFile:notdir logBytes:NO];

  EXPECT_FALSE(netlog_started);
}

TEST(NetLogTest, ExistantDir) {
  NSString* dir = [[NSProcessInfo processInfo] globallyUniqueString];

  bool dir_created = [[NSFileManager defaultManager]
            createDirectoryAtPath:[Cronet getNetLogPathForFile:dir]
      withIntermediateDirectories:NO
                       attributes:nil
                            error:nil];

  bool netlog_started =
      [Cronet startNetLogToFile:[dir stringByAppendingString:@"/netlog.json"]
                       logBytes:NO];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet
                           getNetLogPathForFile:
                               [dir stringByAppendingString:@"/netlog.json"]]
                 error:nil];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet getNetLogPathForFile:dir]
                 error:nil];

  EXPECT_TRUE(dir_created);
  EXPECT_TRUE(netlog_started);
}

TEST(NetLogTest, EmptyFilename) {
  bool netlog_started = [Cronet startNetLogToFile:@"" logBytes:NO];

  EXPECT_FALSE(netlog_started);
}

TEST(NetLogTest, AbsoluteFilename) {
  bool netlog_started =
      [Cronet startNetLogToFile:@"/home/netlog.json" logBytes:NO];

  EXPECT_FALSE(netlog_started);
}
}
