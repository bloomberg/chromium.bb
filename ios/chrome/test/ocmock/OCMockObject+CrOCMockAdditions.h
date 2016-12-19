// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_CROCMOCKADDITIONS_H_
#define IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_CROCMOCKADDITIONS_H_

#import "third_party/ocmock/OCMock/OCMock.h"

@interface OCMockObject (CrOCMockAdditions)

- (BOOL)isStubbed:(SEL)selector;

- (void)removeStub:(SEL)selector;

@end

#endif  // IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_CROCMOCKADDITIONS_H_
