// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/test/test_skcanvas.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Text, MultiLine) {
  UiScene scene;
  testing::NiceMock<cc::MockCanvas> canvas;

  const float kInitialSize = 1.0f;
  const int kPixelWidth = 512;

  // Create an initialize a text element with a long string.
  auto text_instance = base::MakeUnique<Text>(kPixelWidth, 0.020);
  Text* text = text_instance.get();
  text->SetSize(kInitialSize, 0);
  text->SetText(base::UTF8ToUTF16(std::string(1000, 'x')));
  text->SetInitializedForTesting();
  scene.AddUiElement(kRoot, std::move(text_instance));

  // Grab a pointer to the underlying texture to inspect it directly.
  UiTexture* texture = text->GetTextureForTest();
  auto texture_size = texture->GetPreferredTextureSize(kPixelWidth);

  // Make sure we get multiple lines of rendered text from the string.
  scene.OnBeginFrame(base::TimeTicks(), {0, 0, 0});
  texture->DrawAndLayout(&canvas, texture_size);
  int initial_num_lines = text->NumRenderedLinesForTest();
  EXPECT_GT(initial_num_lines, 1);

  // Reduce the field width, and ensure that the number of lines increases.
  text->SetSize(kInitialSize / 2, 0);
  scene.OnBeginFrame(base::TimeTicks(), {0, 0, 0});
  texture->DrawAndLayout(&canvas, texture_size);
  EXPECT_GT(text->NumRenderedLinesForTest(), initial_num_lines);

  // Enforce single-line rendering.
  text->SetMultiLine(false);
  scene.OnBeginFrame(base::TimeTicks(), {0, 0, 0});
  texture->DrawAndLayout(&canvas, texture_size);
  EXPECT_EQ(1, text->NumRenderedLinesForTest());
}

}  // namespace vr
