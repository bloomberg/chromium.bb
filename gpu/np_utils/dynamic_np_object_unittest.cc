// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "gpu/np_utils/dynamic_np_object.h"
#include "gpu/np_utils/np_browser_stub.h"
#include "gpu/np_utils/np_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace np_utils {

class NPDynamicNPObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    object_ = NPCreateObject<DynamicNPObject>(NULL);
  }

  StubNPBrowser stub_browser_;
  NPObjectPointer<DynamicNPObject> object_;
};

TEST_F(NPDynamicNPObjectTest, HasPropertyReturnsFalseForMissingProperty) {
  EXPECT_FALSE(NPHasProperty(NULL, object_, "missing"));
}

TEST_F(NPDynamicNPObjectTest, GetPropertyReturnsFalseForMissingProperty) {
  int32 r;
  EXPECT_FALSE(NPGetProperty(NULL, object_, "missing", &r));
}

TEST_F(NPDynamicNPObjectTest, CanSetProperty) {
  EXPECT_TRUE(NPSetProperty(NULL, object_, "foo", 7));
  int32 r;
  EXPECT_TRUE(NPHasProperty(NULL, object_, "foo"));
  EXPECT_TRUE(NPGetProperty(NULL, object_, "foo", &r));
  EXPECT_EQ(7, r);
}

TEST_F(NPDynamicNPObjectTest, CanRemoveProperty) {
  EXPECT_TRUE(NPSetProperty(NULL, object_, "foo", 7));
  EXPECT_TRUE(NPHasProperty(NULL, object_, "foo"));
  EXPECT_FALSE(NPRemoveProperty(NULL, object_, "foo"));
  EXPECT_FALSE(NPHasProperty(NULL, object_, "foo"));
  int32 r;
  EXPECT_FALSE(NPGetProperty(NULL, object_, "foo", &r));
}

TEST_F(NPDynamicNPObjectTest, CanEnumerateProperties) {
  EXPECT_TRUE(NPSetProperty(NULL, object_, "foo", 7));

  NPIdentifier* names;
  uint32 num_names;
  EXPECT_TRUE(object_->_class->enumerate(object_.Get(), &names, &num_names));

  EXPECT_EQ(1, num_names);
  EXPECT_EQ(NPBrowser::get()->GetStringIdentifier("foo"), names[0]);

  NPBrowser::get()->MemFree(names);
}

// Properties should not be
TEST_F(NPDynamicNPObjectTest, InvalidateNullsObjectProperties) {
  EXPECT_EQ(1, object_->referenceCount);
  {
    EXPECT_TRUE(NPSetProperty(NULL, object_, "foo", object_));
    EXPECT_TRUE(NPHasProperty(NULL, object_, "foo"));
    object_->_class->invalidate(object_.Get());
    EXPECT_TRUE(NPHasProperty(NULL, object_, "foo"));
    NPObjectPointer<DynamicNPObject> r;
    EXPECT_TRUE(NPGetProperty(NULL, object_, "foo", &r));
    EXPECT_TRUE(NULL == r.Get());
  }
  // Invalidate did not release object
  EXPECT_EQ(2, object_->referenceCount);
  NPBrowser::get()->ReleaseObject(object_.Get());
}
}  // namespace np_utils
