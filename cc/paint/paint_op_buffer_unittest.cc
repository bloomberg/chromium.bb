// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename T>
void CheckRefCnt(const T& obj, int32_t count) {
// Skia doesn't define getRefCnt in all builds.
#ifdef SK_DEBUG
  EXPECT_EQ(obj->getRefCnt(), count);
#endif
}

}  // namespace

namespace cc {

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(buffer.approximateBytesUsed(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  buffer.Reset();
  EXPECT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(buffer.approximateBytesUsed(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);
}

TEST(PaintOpBufferTest, SimpleAppend) {
  SkRect rect = SkRect::MakeXYWH(2, 3, 4, 5);
  PaintFlags flags;
  flags.setColor(SK_ColorMAGENTA);
  flags.setAlpha(100);
  SkColor draw_color = SK_ColorRED;
  SkBlendMode blend = SkBlendMode::kSrc;

  PaintOpBuffer buffer;
  buffer.push<SaveLayerOp>(&rect, &flags);
  buffer.push<SaveOp>();
  buffer.push<DrawColorOp>(draw_color, blend);
  buffer.push<RestoreOp>();

  EXPECT_EQ(buffer.approximateOpCount(), 4);

  PaintOpBuffer::Iterator iter(&buffer);
  ASSERT_EQ(iter->GetType(), PaintOpType::SaveLayer);
  SaveLayerOp* save_op = static_cast<SaveLayerOp*>(*iter);
  EXPECT_EQ(save_op->bounds, rect);
  EXPECT_TRUE(save_op->flags == flags);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::Save);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::DrawColor);
  DrawColorOp* op = static_cast<DrawColorOp*>(*iter);
  EXPECT_EQ(op->color, draw_color);
  EXPECT_EQ(op->mode, blend);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
  ++iter;

  EXPECT_FALSE(iter);
}

// PaintOpBuffer has a special case for first ops stored locally, so
// make sure that appending different kind of ops as a first op works
// properly, as well as resetting and reusing the first local op.
TEST(PaintOpBufferTest, FirstOpWithAndWithoutData) {
  PaintOpBuffer buffer;
  char text[] = "asdf";

  // Use a color filter and its ref count to verify that the destructor
  // is called on ops after reset.
  PaintFlags flags;
  sk_sp<SkColorFilter> filter =
      SkColorFilter::MakeModeFilter(SK_ColorMAGENTA, SkBlendMode::kSrcOver);
  flags.setColorFilter(filter);
  CheckRefCnt(filter, 2);

  buffer.push_with_data<DrawTextOp>(text, arraysize(text), 0.f, 0.f, flags);
  CheckRefCnt(filter, 3);

  // Verify that when the first op has data, which may not fit in the
  // PaintRecord internal buffer, that it adds a noop as the first op
  // and then appends the "op with data" into the heap buffer.
  ASSERT_EQ(buffer.approximateOpCount(), 2);
  EXPECT_EQ(buffer.GetFirstOp()->GetType(), PaintOpType::Noop);

  // Verify iteration behavior and brief smoke test of op state.
  {
    PaintOpBuffer::Iterator iter(&buffer);
    PaintOp* noop = *iter;
    EXPECT_EQ(buffer.GetFirstOp(), noop);
    ++iter;

    PaintOp* op = *iter;
    ASSERT_EQ(op->GetType(), PaintOpType::DrawText);
    DrawTextOp* draw_text_op = static_cast<DrawTextOp*>(op);
    EXPECT_EQ(draw_text_op->bytes, arraysize(text));

    const void* data = draw_text_op->GetData();
    EXPECT_EQ(memcmp(data, text, arraysize(text)), 0);

    ++iter;
    EXPECT_FALSE(iter);
  }

  // Reset, verify state, and append an op that will fit in the first slot.
  buffer.Reset();
  CheckRefCnt(filter, 2);

  ASSERT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, flags);
  CheckRefCnt(filter, 3);

  ASSERT_EQ(buffer.approximateOpCount(), 1);
  EXPECT_EQ(buffer.GetFirstOp()->GetType(), PaintOpType::DrawRect);

  PaintOpBuffer::Iterator iter(&buffer);
  ASSERT_EQ(iter->GetType(), PaintOpType::DrawRect);
  DrawRectOp* draw_rect_op = static_cast<DrawRectOp*>(*iter);
  EXPECT_EQ(draw_rect_op->rect, rect);

  ++iter;
  EXPECT_FALSE(iter);

  buffer.Reset();
  ASSERT_EQ(buffer.approximateOpCount(), 0);
  CheckRefCnt(filter, 2);
}

TEST(PaintOpBufferTest, Peek) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  PaintFlags draw_flags;
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(1, 2, 3, 4), draw_flags);
  buffer.push<RestoreOp>();
  buffer.push<SaveOp>();
  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  PaintOpBuffer::Iterator init_iter(&buffer);
  PaintOp* peek[2] = {*init_iter, init_iter.peek1()};

  // Expect that while iterating that next = current.peek1() and that
  // next.peek1() == current.peek2().
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    EXPECT_EQ(*iter, peek[0]) << iter.op_idx();
    EXPECT_EQ(iter.peek1(), peek[1]) << iter.op_idx();

    peek[0] = iter.peek1();
    peek[1] = iter.peek2();
  }
}

// Verify that PaintOps with data are stored properly.
TEST(PaintOpBufferTest, PaintOpData) {
  PaintOpBuffer buffer;

  buffer.push<SaveOp>();
  PaintFlags flags;
  char text1[] = "asdfasdf";
  buffer.push_with_data<DrawTextOp>(text1, arraysize(text1), 0.f, 0.f, flags);

  char text2[] = "qwerty";
  buffer.push_with_data<DrawTextOp>(text2, arraysize(text2), 0.f, 0.f, flags);

  ASSERT_EQ(buffer.approximateOpCount(), 3);

  // Verify iteration behavior and brief smoke test of op state.
  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* save_op = *iter;
  EXPECT_EQ(save_op->GetType(), PaintOpType::Save);
  ++iter;

  PaintOp* op1 = *iter;
  ASSERT_EQ(op1->GetType(), PaintOpType::DrawText);
  DrawTextOp* draw_text_op1 = static_cast<DrawTextOp*>(op1);
  EXPECT_EQ(draw_text_op1->bytes, arraysize(text1));
  const void* data1 = draw_text_op1->GetData();
  EXPECT_EQ(memcmp(data1, text1, arraysize(text1)), 0);
  ++iter;

  PaintOp* op2 = *iter;
  ASSERT_EQ(op2->GetType(), PaintOpType::DrawText);
  DrawTextOp* draw_text_op2 = static_cast<DrawTextOp*>(op2);
  EXPECT_EQ(draw_text_op2->bytes, arraysize(text2));
  const void* data2 = draw_text_op2->GetData();
  EXPECT_EQ(memcmp(data2, text2, arraysize(text2)), 0);
  ++iter;

  EXPECT_FALSE(iter);
}

// Verify that PaintOps with arrays are stored properly.
TEST(PaintOpBufferTest, PaintOpArray) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();

  // arbitrary data
  std::string texts[] = {"xyz", "abcdefg", "thingerdoo"};
  SkPoint point1[] = {SkPoint::Make(1, 2), SkPoint::Make(2, 3),
                      SkPoint::Make(3, 4)};
  SkPoint point2[] = {SkPoint::Make(8, -12)};
  SkPoint point3[] = {SkPoint::Make(0, 0),   SkPoint::Make(5, 6),
                      SkPoint::Make(-1, -1), SkPoint::Make(9, 9),
                      SkPoint::Make(50, 50), SkPoint::Make(100, 100)};
  SkPoint* points[] = {point1, point2, point3};
  size_t counts[] = {arraysize(point1), arraysize(point2), arraysize(point3)};

  for (size_t i = 0; i < arraysize(texts); ++i) {
    PaintFlags flags;
    flags.setAlpha(i);
    buffer.push_with_array<DrawPosTextOp>(texts[i].c_str(), texts[i].length(),
                                          points[i], counts[i], flags);
  }

  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* save_op = *iter;
  EXPECT_EQ(save_op->GetType(), PaintOpType::Save);
  ++iter;

  for (size_t i = 0; i < arraysize(texts); ++i) {
    ASSERT_EQ(iter->GetType(), PaintOpType::DrawPosText);
    DrawPosTextOp* op = static_cast<DrawPosTextOp*>(*iter);

    EXPECT_EQ(op->flags.getAlpha(), i);

    EXPECT_EQ(op->bytes, texts[i].length());
    const void* data = op->GetData();
    EXPECT_EQ(memcmp(data, texts[i].c_str(), op->bytes), 0);

    EXPECT_EQ(op->count, counts[i]);
    const SkPoint* op_points = op->GetArray();
    for (size_t k = 0; k < op->count; ++k)
      EXPECT_EQ(op_points[k], points[i][k]);

    ++iter;
  }

  EXPECT_FALSE(iter);
}

TEST(PaintOpBufferTest, PeekEmpty) {
  PaintOpBuffer empty;
  PaintOpBuffer::Iterator empty_iter(&empty);
  EXPECT_EQ(nullptr, empty_iter.peek1());
  EXPECT_EQ(nullptr, empty_iter.peek2());
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  // Expect the alpha from the draw and the save layer to be folded together.
  // Since alpha is stored in a uint8_t and gets rounded, so use tolerance.
  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when the drawing op's flags are not compatible with being folded into the
// save layer with opacity.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_BadFlags) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrc);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when there are more than one ops between the save and restore.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_TooManyOps) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrcOver);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// Verify that the save draw restore code works with a single op
// that's not a draw op, and the optimization does not kick in.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpNotADrawOp) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

// Test that the save/draw/restore optimization applies if the single op
// is a DrawRecord that itself has a single draw op.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleOp) {
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  record->push<DrawRectOp>(rect, draw_flags);
  EXPECT_EQ(record->approximateOpCount(), 1);

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// The same as the above SingleOpRecord test, but the single op is not
// a draw op.  So, there's no way to fold in the save layer optimization.
// Verify that the optimization doesn't apply and that this doesn't crash.
// See: http://crbug.com/712093.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleNonDrawOp) {
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();
  record->push<NoopOp>();
  EXPECT_EQ(record->approximateOpCount(), 1);
  EXPECT_FALSE(record->GetFirstOp()->IsDrawOp());

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

}  // namespace cc
