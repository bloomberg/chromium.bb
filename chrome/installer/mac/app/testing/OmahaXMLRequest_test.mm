// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/installer/mac/app/OmahaXMLRequest.h"

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(OmahaXMLRequestTest, CreateReturnsValidXML) {
  base::scoped_nsobject<NSXMLDocument> xml_body_(
      [OmahaXMLRequest createXMLRequestBody]);
  ASSERT_TRUE(xml_body_);

  NSString* requestDTDLocation = [[[[NSBundle mainBundle] bundlePath]
      stringByAppendingPathComponent:
          @"../../chrome/installer/mac/app/testing/requestCheck.dtd"]
      stringByResolvingSymlinksInPath];
  NSData* requestDTDData = [NSData dataWithContentsOfFile:requestDTDLocation];
  ASSERT_TRUE(requestDTDData);

  NSError* error;
  NSXMLDTD* requestXMLChecker =
      [[NSXMLDTD alloc] initWithData:requestDTDData options:0 error:&error];
  [requestXMLChecker setName:@"request"];
  [xml_body_ setDTD:requestXMLChecker];
  EXPECT_TRUE([xml_body_ validateAndReturnError:&error]);
}

}  // namespace
