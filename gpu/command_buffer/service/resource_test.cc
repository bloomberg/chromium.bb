// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the ResourceMap.

#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/service/resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// Mock resource implementation that checks for leaks.
class ResourceMock : public Resource {
 public:
  ResourceMock() : Resource() {
    ++instance_count_;
  }
  virtual ~ResourceMock() {
    --instance_count_;
  }

  // Returns the instance count. The instance count is increased in the
  // constructor and decreased in the destructor, to track leaks. The reason is
  // that we can't mock the destructor, though we want to make sure the mock is
  // destroyed.
  static int instance_count() { return instance_count_; }
 private:
  static int instance_count_;
  DISALLOW_COPY_AND_ASSIGN(ResourceMock);
};
int ResourceMock::instance_count_ = 0;

// Test fixture for ResourceMap test. Creates a ResourceMap using a mock
// Resource, and checks for ResourceMock leaks.
class ResourceMapTest : public testing::Test {
 protected:
  typedef ResourceMap<ResourceMock> Map;
  virtual void SetUp() {
    instance_count_ = ResourceMock::instance_count();
    map_.reset(new Map());
  }
  virtual void TearDown() {
    CheckLeaks();
  }

  // Makes sure we didn't leak any ResourceMock object.
  void CheckLeaks() {
    EXPECT_EQ(instance_count_, ResourceMock::instance_count());
  }

  Map *map() const { return map_.get(); }
 private:
  int instance_count_;
  scoped_ptr<Map> map_;
};

TEST_F(ResourceMapTest, TestMap) {
  // check that initial mapping is empty.
  EXPECT_EQ(NULL, map()->Get(0));
  EXPECT_EQ(NULL, map()->Get(1));
  EXPECT_EQ(NULL, map()->Get(392));

  // create a new resource, assign it to an ID.
  ResourceMock *resource = new ResourceMock();
  map()->Assign(123, resource);
  EXPECT_EQ(resource, map()->Get(123));

  // Destroy the resource, making sure the object is deleted.
  EXPECT_EQ(true, map()->Destroy(123));
  EXPECT_EQ(false, map()->Destroy(123));  // destroying again should fail.
  resource = NULL;
  CheckLeaks();

  // create a new resource, add it to the map, and make sure it gets deleted
  // when we assign a new resource to that ID.
  resource = new ResourceMock();
  map()->Assign(1, resource);
  resource = new ResourceMock();
  map()->Assign(1, resource);
  EXPECT_EQ(resource, map()->Get(1));  // check that we have the new resource.
  EXPECT_EQ(true, map()->Destroy(1));
  CheckLeaks();

  // Adds 3 resources, then call DestroyAllResources().
  resource = new ResourceMock();
  map()->Assign(1, resource);
  resource = new ResourceMock();
  map()->Assign(2, resource);
  resource = new ResourceMock();
  map()->Assign(3, resource);
  map()->DestroyAllResources();
  EXPECT_EQ(NULL, map()->Get(1));
  EXPECT_EQ(NULL, map()->Get(2));
  EXPECT_EQ(NULL, map()->Get(3));
  CheckLeaks();
}

}  // namespace gpu
