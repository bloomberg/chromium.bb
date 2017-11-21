// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/element_detection.h"

#include <vector>

#include "base/bind.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using ElementVector = std::vector<Element>;

}  // namespace

TEST(ElementDetectionTest, ElementFinderEmpty) {
  std::vector<uint8_t> buffer(10, 0);
  ElementFinder finder(
      ConstBufferView(buffer.data(), buffer.size()),
      base::BindRepeating([](ConstBufferView image) -> base::Optional<Element> {
        return base::nullopt;
      }));
  EXPECT_EQ(base::nullopt, finder.GetNext());
}

ElementVector TestElementFinder(std::vector<uint8_t> buffer) {
  ConstBufferView image(buffer.data(), buffer.size());

  ElementFinder finder(
      image,
      base::BindRepeating(
          [](ConstBufferView image,
             ConstBufferView region) -> base::Optional<Element> {
            EXPECT_GE(region.begin(), image.begin());
            EXPECT_LE(region.end(), image.end());
            EXPECT_GE(region.size(), 0U);

            if (region[0] != 0) {
              offset_t length = 1;
              while (length < region.size() && region[length] == region[0])
                ++length;
              return Element{{0, length},
                             static_cast<ExecutableType>(region[0])};
            }
            return base::nullopt;
          },
          image));
  std::vector<Element> elements;
  for (auto element = finder.GetNext(); element; element = finder.GetNext()) {
    elements.push_back(*element);
  }
  return elements;
}

TEST(ElementDetectionTest, ElementFinder) {
  EXPECT_EQ(ElementVector(), TestElementFinder({}));
  EXPECT_EQ(ElementVector(), TestElementFinder({0, 0}));
  EXPECT_EQ(ElementVector({{{0, 2}, kExeTypeWin32X86}}),
            TestElementFinder({1, 1}));
  EXPECT_EQ(
      ElementVector({{{0, 2}, kExeTypeWin32X86}, {{2, 2}, kExeTypeWin32X64}}),
      TestElementFinder({1, 1, 2, 2}));
  EXPECT_EQ(ElementVector({{{1, 2}, kExeTypeWin32X86}}),
            TestElementFinder({0, 1, 1, 0}));
  EXPECT_EQ(
      ElementVector({{{1, 2}, kExeTypeWin32X86}, {{3, 3}, kExeTypeWin32X64}}),
      TestElementFinder({0, 1, 1, 2, 2, 2}));
  EXPECT_EQ(
      ElementVector({{{1, 2}, kExeTypeWin32X86}, {{4, 3}, kExeTypeWin32X64}}),
      TestElementFinder({0, 1, 1, 0, 2, 2, 2}));
}

}  // namespace zucchini
