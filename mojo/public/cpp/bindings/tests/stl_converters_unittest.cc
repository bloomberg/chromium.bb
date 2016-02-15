// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/stl_converters.h"

#include <map>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

using STLConvertersTest = testing::Test;

TEST_F(STLConvertersTest, AvoidUnnecessaryCopies) {
  Array<CopyableType> mojo_array(1);
  std::vector<CopyableType> stl_vector = UnwrapToSTLType(std::move(mojo_array));
  ASSERT_EQ(1u, stl_vector.size());
  ASSERT_FALSE(stl_vector[0].copied());
  Array<CopyableType> mojo_array2 = WrapSTLType(std::move(stl_vector));
  ASSERT_EQ(1u, mojo_array2.size());
  ASSERT_FALSE(mojo_array2[0].copied());

  Map<int32_t, CopyableType> mojo_map;
  mojo_map.insert(42, CopyableType());
  mojo_map[42].ResetCopied();
  std::map<int32_t, CopyableType> stl_map =
      UnwrapToSTLType(std::move(mojo_map));
  ASSERT_EQ(1u, stl_map.size());
  ASSERT_FALSE(stl_map[42].copied());
  Map<int32_t, CopyableType> mojo_map2 = WrapSTLType(std::move(stl_map));
  ASSERT_EQ(1u, mojo_map2.size());
  ASSERT_FALSE(mojo_map2[42].copied());
}

TEST_F(STLConvertersTest, RecursiveConversion) {
  Array<Map<String, Array<int32_t>>> mojo_obj(2);
  mojo_obj[0] = nullptr;
  mojo_obj[1]["hello"].push_back(123);
  mojo_obj[1]["hello"].push_back(456);

  std::vector<std::map<std::string, std::vector<int32_t>>> stl_obj =
      UnwrapToSTLType(std::move(mojo_obj));

  ASSERT_EQ(2u, stl_obj.size());
  ASSERT_TRUE(stl_obj[0].empty());
  ASSERT_EQ(1u, stl_obj[1].size());
  ASSERT_EQ(2u, stl_obj[1]["hello"].size());
  ASSERT_EQ(123, stl_obj[1]["hello"][0]);
  ASSERT_EQ(456, stl_obj[1]["hello"][1]);

  Array<Map<String, Array<int32_t>>> mojo_obj2 =
      WrapSTLType(std::move(stl_obj));

  ASSERT_EQ(2u, mojo_obj2.size());
  ASSERT_EQ(0u, mojo_obj2[0].size());
  // The null flag has been lost when converted to std::map.
  ASSERT_FALSE(mojo_obj2[0].is_null());
  ASSERT_EQ(1u, mojo_obj2[1].size());
  ASSERT_EQ(2u, mojo_obj2[1]["hello"].size());
  ASSERT_EQ(123, mojo_obj2[1]["hello"][0]);
  ASSERT_EQ(456, mojo_obj2[1]["hello"][1]);
}

TEST_F(STLConvertersTest, StopsAtMojoStruct) {
  Array<NamedRegionPtr> mojo_obj(1);
  mojo_obj[0] = NamedRegion::New();
  mojo_obj[0]->name = "hello";
  mojo_obj[0]->rects.resize(3);

  std::vector<NamedRegionPtr> stl_obj = UnwrapToSTLType(std::move(mojo_obj));

  ASSERT_EQ(1u, stl_obj.size());
  ASSERT_EQ("hello", stl_obj[0]->name);
  ASSERT_EQ(3u, stl_obj[0]->rects.size());

  Array<NamedRegionPtr> mojo_obj2 = WrapSTLType(std::move(stl_obj));

  ASSERT_EQ(1u, mojo_obj2.size());
  ASSERT_EQ("hello", mojo_obj2[0]->name);
  ASSERT_EQ(3u, mojo_obj2[0]->rects.size());
}

}  // namespace test
}  // namespace mojo
