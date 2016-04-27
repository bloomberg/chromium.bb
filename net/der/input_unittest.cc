// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace der {
namespace test {

const uint8_t kInput[] = {'t', 'e', 's', 't'};
const uint8_t kInput2[] = {'t', 'e', 'a', 'l'};

TEST(InputTest, Equals) {
  Input test(kInput);
  Input test2(kInput);
  EXPECT_EQ(test, test2);

  uint8_t input_copy[arraysize(kInput)] = {0};
  memcpy(input_copy, kInput, arraysize(kInput));
  Input test_copy(input_copy);
  EXPECT_EQ(test, test_copy);

  Input test_truncated(kInput, arraysize(kInput) - 1);
  EXPECT_NE(test, test_truncated);
  EXPECT_NE(test_truncated, test);
}

TEST(InputTest, LessThan) {
  Input test(kInput);
  EXPECT_FALSE(test < test);

  Input test2(kInput2);
  EXPECT_FALSE(test < test2);
  EXPECT_TRUE(test2 < test);

  Input test_truncated(kInput, arraysize(kInput) - 1);
  EXPECT_FALSE(test < test_truncated);
  EXPECT_TRUE(test_truncated < test);
}

TEST(InputTest, AsString) {
  Input input(kInput);
  std::string expected_string(reinterpret_cast<const char*>(kInput),
                              arraysize(kInput));
  EXPECT_EQ(expected_string, input.AsString());
}

TEST(InputTest, StaticArray) {
  Input input(kInput);
  EXPECT_EQ(arraysize(kInput), input.Length());

  Input input2(kInput);
  EXPECT_EQ(input, input2);
}

TEST(ByteReaderTest, NoReadPastEnd) {
  ByteReader reader(Input(nullptr, 0));
  uint8_t data;
  EXPECT_FALSE(reader.ReadByte(&data));
}

TEST(ByteReaderTest, ReadToEnd) {
  uint8_t out;
  ByteReader reader((Input(kInput)));
  for (size_t i = 0; i < arraysize(kInput); ++i) {
    ASSERT_TRUE(reader.ReadByte(&out));
    ASSERT_EQ(kInput[i], out);
  }
  EXPECT_FALSE(reader.ReadByte(&out));
}

TEST(ByteReaderTest, PartialReadFails) {
  Input out;
  ByteReader reader((Input(kInput)));
  EXPECT_FALSE(reader.ReadBytes(5, &out));
}

TEST(ByteReaderTest, HasMore) {
  Input out;
  ByteReader reader((Input(kInput)));

  ASSERT_TRUE(reader.HasMore());
  ASSERT_TRUE(reader.ReadBytes(arraysize(kInput), &out));
  ASSERT_FALSE(reader.HasMore());
}

TEST(ByteReaderTest, ReadToMark) {
  uint8_t out;
  Input input(kInput);
  ByteReader reader(input);

  // Read 2 bytes from the reader and then set a mark.
  ASSERT_TRUE(reader.ReadByte(&out));
  ASSERT_TRUE(reader.ReadByte(&out));
  Mark mark = reader.NewMark();

  // Reset the reader and check that we can read to a mark previously set.
  reader = ByteReader(input);
  Input marked_data;
  ASSERT_TRUE(reader.ReadToMark(mark, &marked_data));
}

TEST(ByteReaderTest, CantReadToWrongMark) {
  Input out;
  Input in1(kInput);

  const uint8_t in2_bytes[] = {'T', 'E', 'S', 'T'};
  Input in2(in2_bytes);
  ByteReader reader1(in1);
  ByteReader reader2(in2);
  ASSERT_TRUE(reader1.ReadBytes(2, &out));
  ASSERT_TRUE(reader2.ReadBytes(2, &out));
  Mark mark1 = reader1.NewMark();
  Mark mark2 = reader2.NewMark();
  reader1 = ByteReader(in1);
  reader2 = ByteReader(in2);

  // It is not possible to advance to a mark outside the underlying input.
  ASSERT_FALSE(reader1.AdvanceToMark(mark2));
  ASSERT_FALSE(reader2.AdvanceToMark(mark1));
}

TEST(ByteReaderTest, MarksAreSharedBetweenSameInputs) {
  Input out;
  Input in1(kInput);
  Input in2(kInput, 1);
  ByteReader reader1(in1);
  ByteReader reader2(in2);
  ASSERT_TRUE(reader1.ReadBytes(2, &out));
  ASSERT_TRUE(reader2.ReadBytes(1, &out));
  Mark mark1 = reader1.NewMark();
  Mark mark2 = reader2.NewMark();
  reader1 = ByteReader(in1);
  reader2 = ByteReader(in2);

  // If Marks are created on the same underlying data, they can be shared
  // across ByteReaders and Inputs. However, they still must be inside the
  // bounds for the ByteReader they are being used on.

  // mark1 is past the end of the input for reader2.
  EXPECT_FALSE(reader2.AdvanceToMark(mark1));
  // mark2 is within the bounds of reader1.
  EXPECT_TRUE(reader1.AdvanceToMark(mark2));
}

TEST(ByteReaderTest, CantReadToWrongMarkWithInputsOnStack) {
  const uint8_t data1[] = "test";
  const uint8_t data2[] = "foo";
  Input out;
  Input in1(data1);
  Input in2(data2);

  ByteReader reader1(in1);
  ByteReader reader2(in2);
  ASSERT_TRUE(reader1.ReadBytes(2, &out));
  ASSERT_TRUE(reader2.ReadBytes(2, &out));
  Mark mark1 = reader1.NewMark();
  Mark mark2 = reader2.NewMark();
  reader1 = ByteReader(in1);
  reader2 = ByteReader(in2);

  ASSERT_FALSE(reader1.AdvanceToMark(mark2));
  ASSERT_FALSE(reader2.AdvanceToMark(mark1));
}

}  // namespace test
}  // namespace der
}  // namespace net
