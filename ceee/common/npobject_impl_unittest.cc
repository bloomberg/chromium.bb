// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for NPAPI Object Wrapper classes.

#include "ceee/common/npobject_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace {

// A bare bones implementation class which really just uses the default
// implementations of NpObjectImpl/NpObjectBase so we can test those
// wrapper classes for correct default NPAPI functionality.
class BareNpObject : public NpObjectImpl<BareNpObject> {
 public:
  explicit BareNpObject(NPP npp) : NpObjectImpl<BareNpObject>(npp) {
  }
  ~BareNpObject() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BareNpObject);
};

// A mock class which allows us to verify that the corresponding NPObject
// functions are called when a client calls functions through the NPClass
// struct's function pointers.
class MockNpObject : public NpObjectImpl<MockNpObject> {
 public:
  explicit MockNpObject(NPP npp) : NpObjectImpl<MockNpObject>(npp) {
  }
  ~MockNpObject() {
  }

  MOCK_METHOD0(Invalidate, void());
  MOCK_METHOD1(HasMethod, bool(NPIdentifier name));
  MOCK_METHOD4(Invoke, bool(NPIdentifier name, const NPVariant* args,
      uint32_t argCount, NPVariant* result));
  MOCK_METHOD3(InvokeDefault, bool(const NPVariant* args, uint32_t argCount,
      NPVariant* result));
  MOCK_METHOD1(HasProperty, bool(NPIdentifier name));
  MOCK_METHOD2(GetProperty, bool(NPIdentifier name, NPVariant* result));
  MOCK_METHOD2(SetProperty, bool(NPIdentifier name, const NPVariant* value));
  MOCK_METHOD1(RemoveProperty, bool(NPIdentifier name));
  MOCK_METHOD2(Enumeration, bool(NPIdentifier** value, uint32_t* count));
  MOCK_METHOD3(Construct, bool(const NPVariant* args, uint32_t argCount,
      NPVariant* result));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNpObject);
};

// Test the functions in the NPClass struct to ensure that the correct
// NP*FunctionPtr function defaults come out. We use
// https://developer.mozilla.org/en/NPClass as a rough guide for what default
// values should be.
TEST(NpApiWrappers, NpObjectDefaults) {
  // Grab the NPClass structure so we can access the functions that have been
  // hooked up by this particular implementation.
  NPClass* np_class = BareNpObject::object_class();
  ASSERT_TRUE(np_class != NULL);

  // Create an instance of our barebones class so we can use it in the function
  // calls from the NPClass struct. Also exercises allocation.
  NPObject* np_object = np_class->allocate(NULL, np_class);
  ASSERT_TRUE(np_object != NULL);

  EXPECT_GE(np_class->structVersion, (uint32_t)3);
  EXPECT_FALSE(np_class->hasMethod(np_object, NULL));
  EXPECT_FALSE(np_class->invoke(np_object, NULL, NULL, 0, NULL));
  EXPECT_FALSE(np_class->invokeDefault(np_object, NULL, 0, NULL));
  EXPECT_FALSE(np_class->hasProperty(np_object, NULL));
  EXPECT_FALSE(np_class->getProperty(np_object, NULL, NULL));
  EXPECT_FALSE(np_class->setProperty(np_object, NULL, NULL));
  EXPECT_FALSE(np_class->removeProperty(np_object, NULL));
  EXPECT_FALSE(np_class->enumerate(np_object, NULL, NULL));
  EXPECT_FALSE(np_class->construct(np_object, NULL, 0, NULL));

  // Invalidation and deallocation. No return values to check here - just
  // exercising the code. Deallocation should clean up the object for us.
  np_class->invalidate(np_object);
  np_class->deallocate(np_object);
  np_object = NULL;
}

// Uses a Mock class to ensure that the correct functions are being called
// on the NPAPI Object instance when they are called through the NPClass
// structure that our wrapper sets up.
TEST(NpApiWrappers, NpObjectCalls) {
  NPClass* np_class = MockNpObject::object_class();
  ASSERT_TRUE(np_class != NULL);
  MockNpObject np_object(NULL);

  EXPECT_CALL(np_object, HasMethod(NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->hasMethod(&np_object, NULL));

  EXPECT_CALL(np_object, Invoke(NULL, NULL, 0, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->invoke(&np_object, NULL, NULL, 0, NULL));

  EXPECT_CALL(np_object, InvokeDefault(NULL, 0, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->invokeDefault(&np_object, NULL, 0, NULL));

  EXPECT_CALL(np_object, HasProperty(NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->hasProperty(&np_object, NULL));

  EXPECT_CALL(np_object, GetProperty(NULL, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->getProperty(&np_object, NULL, NULL));

  EXPECT_CALL(np_object, SetProperty(NULL, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->setProperty(&np_object, NULL, NULL));

  EXPECT_CALL(np_object, RemoveProperty(NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->removeProperty(&np_object, NULL));

  EXPECT_CALL(np_object, Enumeration(NULL, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->enumerate(&np_object, NULL, NULL));

  EXPECT_CALL(np_object, Construct(NULL, 0, NULL))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(np_class->construct(&np_object, NULL, 0, NULL));
}

}  // namespace
