// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/src/utils/debugger/SkDebugCanvas.h"
#include "third_party/skia/src/utils/debugger/SkDrawCommand.h"

namespace {

testing::AssertionResult HasInfoField(SkDebugCanvas& canvas, int index,
                                      const char* field) {

  const SkTDArray<SkString*>* info = canvas.getCommandInfo(index);
  if (info == NULL)
    return testing::AssertionFailure() << " command info not found for index "
                                       << index;

  for (int i = 0; i < info->count(); ++i) {
    const SkString* info_str = (*info)[i];
    if (info_str == NULL)
      return testing::AssertionFailure() << " NULL info string for index "
                                         << index;

    // FIXME: loose info paramter test.
    if (strstr(info_str->c_str(), field) != NULL)
      return testing::AssertionSuccess() << field << " found";
  }

  return testing::AssertionFailure() << field << " not found";
}

}

namespace content {

TEST(SkiaBenchmarkingExtensionTest, SkDebugCanvas) {
  SkGraphics::Init();

  // Prepare canvas and resources.
  SkDebugCanvas canvas(100, 100);
  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkRect fullRect = SkRect::MakeWH(SkIntToScalar(100), SkIntToScalar(100));
  SkRect fillRect = SkRect::MakeXYWH(SkIntToScalar(25), SkIntToScalar(25),
                                     SkIntToScalar(50), SkIntToScalar(50));

  SkMatrix trans;
  trans.setTranslate(SkIntToScalar(10), SkIntToScalar(10));

  // Draw a trivial scene.
  canvas.save();
  canvas.clipRect(fullRect, SkRegion::kIntersect_Op, false);
  canvas.setMatrix(trans);
  canvas.drawRect(fillRect, red_paint);
  canvas.restore();

  // Verify the recorded commands.
  SkDrawCommand::OpType cmd;
  int idx = 0;
  ASSERT_EQ(canvas.getSize(), 5);

  ASSERT_TRUE(canvas.getDrawCommandAt(idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SkDrawCommand::kSave_OpType);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Save");

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SkDrawCommand::kClipRect_OpType);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "ClipRect");
  EXPECT_TRUE(HasInfoField(canvas, idx, "SkRect"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "Op"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "doAA"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SkDrawCommand::kSetMatrix_OpType);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "SetMatrix");

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SkDrawCommand::kDrawRect_OpType);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "DrawRect");
  EXPECT_TRUE(HasInfoField(canvas, idx, "SkRect"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SkDrawCommand::kRestore_OpType);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Restore");
}

} // namespace content
