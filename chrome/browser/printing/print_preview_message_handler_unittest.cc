// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_message_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using PrintPreviewMessageHandlerTest = testing::Test;

TEST_F(PrintPreviewMessageHandlerTest, GetSymmetricalPrintableArea) {
  gfx::Rect printable_area =
      PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
          gfx::Size(612, 792), gfx::Rect(0, 0, 560, 750));
  EXPECT_EQ(gfx::Rect(52, 42, 508, 708), printable_area);

  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, 60, 550, 700));
  EXPECT_EQ(gfx::Rect(50, 60, 512, 672), printable_area);

  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(-1, 60, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, -1, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(100, 60, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, 100, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(400, 60, 212, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PrintPreviewMessageHandler::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(40, 600, 212, 192));
  EXPECT_EQ(gfx::Rect(), printable_area);
}

}  // namespace printing
