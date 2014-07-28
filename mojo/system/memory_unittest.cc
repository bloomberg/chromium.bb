// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "mojo/public/c/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

TEST(MemoryTest, Valid) {
  char my_char;
  int32_t my_int32;
  int64_t my_int64_array[5] = {};  // Zero initialize.

  UserPointer<char> my_char_ptr(&my_char);
  UserPointer<int32_t> my_int32_ptr(&my_int32);
  UserPointer<int64_t> my_int64_array_ptr(my_int64_array);

  // |UserPointer<>::IsNull()|:
  EXPECT_FALSE(my_char_ptr.IsNull());
  EXPECT_FALSE(my_int32_ptr.IsNull());
  EXPECT_FALSE(my_int64_array_ptr.IsNull());

  // |UserPointer<>::Put()| and |UserPointer<>::Get()|:
  my_char_ptr.Put('x');
  EXPECT_EQ('x', my_char);
  EXPECT_EQ('x', my_char_ptr.Get());
  my_int32_ptr.Put(123);
  EXPECT_EQ(123, my_int32);
  EXPECT_EQ(123, my_int32_ptr.Get());
  my_int64_array_ptr.Put(456);
  EXPECT_EQ(456, my_int64_array[0]);
  EXPECT_EQ(456, my_int64_array_ptr.Get());

  // |UserPointer<>::At()|, etc.:
  my_int64_array_ptr.At(3).Put(789);
  EXPECT_EQ(789, my_int64_array[3]);
  {
    // Copy construction:
    UserPointer<int64_t> other(my_int64_array_ptr.At(3));
    EXPECT_FALSE(other.IsNull());
    EXPECT_EQ(789, other.Get());

    // Assignment:
    other = my_int64_array_ptr;
    EXPECT_FALSE(other.IsNull());
    EXPECT_EQ(456, other.Get());

    // Assignment to |NullUserPointer()|:
    other = NullUserPointer();
    EXPECT_TRUE(other.IsNull());

    // |MakeUserPointer()|:
    other = MakeUserPointer(&my_int64_array[1]);
    other.Put(-123);
    EXPECT_EQ(-123, my_int64_array_ptr.At(1).Get());
  }

  // "const" |UserPointer<>|:
  {
    // Explicit constructor from |NullUserPointer()|:
    UserPointer<const char> other((NullUserPointer()));
    EXPECT_TRUE(other.IsNull());

    // Conversion to "const":
    other = my_char_ptr;
    EXPECT_EQ('x', other.Get());
  }

  // Default constructor:
  {
    UserPointer<int32_t> other;
    EXPECT_TRUE(other.IsNull());

    other = my_int32_ptr;
    other.Put(-456);
    EXPECT_EQ(-456, my_int32_ptr.Get());
  }

  // |UserPointer<>::CheckArray()|:
  my_int64_array_ptr.CheckArray(5);

  // |UserPointer<>::GetArray()|:
  {
    // From a "const" |UserPointer<>| (why not?):
    UserPointer<const int64_t> other(my_int64_array_ptr);
    int64_t array[3] = {1, 2, 3};
    other.At(1).GetArray(array, 3);
    EXPECT_EQ(-123, array[0]);
    EXPECT_EQ(0, array[1]);
    EXPECT_EQ(789, array[2]);
  }

  // |UserPointer<>::PutArray()|:
  {
    const int64_t array[2] = {654, 321};
    my_int64_array_ptr.At(3).PutArray(array, 2);
    EXPECT_EQ(0, my_int64_array[2]);
    EXPECT_EQ(654, my_int64_array[3]);
    EXPECT_EQ(321, my_int64_array[4]);
  }

  // |UserPointer<>::Reader|:
  {
    UserPointer<int64_t>::Reader reader(my_int64_array_ptr, 5);
    EXPECT_EQ(456, reader.GetPointer()[0]);
    EXPECT_EQ(321, reader.GetPointer()[4]);
  }

  // Non-const to const:
  {
    UserPointer<const int64_t>::Reader reader(my_int64_array_ptr.At(3), 1);
    const int64_t* ptr = reader.GetPointer();
    EXPECT_EQ(654, *ptr);
  }

  // |UserPointer<>::Writer|:
  {
    UserPointer<int64_t>::Writer writer(my_int64_array_ptr.At(2), 1);
    int64_t* ptr = writer.GetPointer();
    *ptr = 1234567890123LL;
    writer.Commit();
    EXPECT_EQ(1234567890123LL, my_int64_array[2]);
  }

  // |UserPointer<>::ReaderWriter|:
  {
    UserPointer<int32_t>::ReaderWriter reader_writer(my_int32_ptr, 1);
    int32_t* ptr = reader_writer.GetPointer();
    EXPECT_EQ(-456, *ptr);
    *ptr = 42;
    reader_writer.Commit();
    EXPECT_EQ(42, my_int32);
  }

  // |UserPointer<>::ReinterpretCast<>|:
  // (This assumes little-endian, etc.)
  {
    UserPointer<const char> other(my_int32_ptr.ReinterpretCast<char>());
    EXPECT_EQ(42, other.Get());
    EXPECT_EQ(0, other.At(1).Get());
    EXPECT_EQ(0, other.At(2).Get());
    EXPECT_EQ(0, other.At(3).Get());
  }

  // |UserPointer<>::GetPointerValue()|:
  {
    UserPointer<int32_t> other;
    EXPECT_EQ(0u, other.GetPointerValue());
    other = my_int32_ptr;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&my_int32), other.GetPointerValue());
  }
}

// TODO(vtl): Convert this test.
#if 0
TEST(MemoryTest, Invalid) {
  // Note: |VerifyUserPointer...()| are defined to be "best effort" checks (and
  // may always return true). Thus these tests of invalid cases only reflect the
  // current implementation.

  // These tests depend on |int32_t| and |int64_t| having nontrivial alignment.
  MOJO_COMPILE_ASSERT(MOJO_ALIGNOF(int32_t) != 1,
                      int32_t_does_not_have_to_be_aligned);
  MOJO_COMPILE_ASSERT(MOJO_ALIGNOF(int64_t) != 1,
                      int64_t_does_not_have_to_be_aligned);

  int32_t my_int32;
  int64_t my_int64;

  // |VerifyUserPointer|:

  EXPECT_FALSE(VerifyUserPointer<char>(NULL));
  EXPECT_FALSE(VerifyUserPointer<int32_t>(NULL));
  EXPECT_FALSE(VerifyUserPointer<int64_t>(NULL));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointer<int32_t>(reinterpret_cast<const int32_t*>(1)));
  EXPECT_FALSE(VerifyUserPointer<int64_t>(reinterpret_cast<const int64_t*>(1)));

  // |VerifyUserPointerWithCount|:

  EXPECT_FALSE(VerifyUserPointerWithCount<char>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(NULL, 1));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(
                   reinterpret_cast<const int32_t*>(1), 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(
                   reinterpret_cast<const int64_t*>(1), 1));

  // Count too big:
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(
                   &my_int32, std::numeric_limits<size_t>::max()));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(
                   &my_int64, std::numeric_limits<size_t>::max()));

  // |VerifyUserPointerWithSize|:

  EXPECT_FALSE(VerifyUserPointerWithSize<1>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(NULL, 4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 8));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(reinterpret_cast<const int32_t*>(1),
                                            1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(reinterpret_cast<const int32_t*>(1),
                                            4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            1));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            8));
}
#endif

}  // namespace
}  // namespace system
}  // namespace mojo
