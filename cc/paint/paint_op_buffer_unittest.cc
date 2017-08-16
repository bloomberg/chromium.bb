// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkWriteBuffer.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"

using testing::_;
using testing::Property;
using testing::Mock;

namespace cc {
namespace {
// An arbitrary size guaranteed to fit the size of any serialized op in this
// unit test.  This can also be used for deserialized op size safely in this
// unit test suite as generally deserialized ops are smaller.
static constexpr size_t kBufferBytesPerOp = 1000 + sizeof(LargestPaintOp);

template <typename T>
void ValidateOps(PaintOpBuffer* buffer) {
  // Make sure all test data is valid before serializing it.
  for (auto* op : PaintOpBuffer::Iterator(buffer))
    EXPECT_TRUE(static_cast<T*>(op)->IsValid());
}

}  // namespace

class PaintOpSerializationTestUtils {
 public:
  static void ExpectFlattenableEqual(SkFlattenable* expected,
                                     SkFlattenable* actual) {
    sk_sp<SkData> expected_data(SkValidatingSerializeFlattenable(expected));
    sk_sp<SkData> actual_data(SkValidatingSerializeFlattenable(actual));
    ASSERT_EQ(expected_data->size(), actual_data->size());
    EXPECT_TRUE(expected_data->equals(actual_data.get()));
  }

  static void ExpectPaintShadersEqual(const PaintShader* one,
                                      const PaintShader* two) {
    if (!one) {
      EXPECT_FALSE(two);
      return;
    }

    ASSERT_TRUE(one);
    ASSERT_TRUE(two);

    EXPECT_EQ(one->shader_type_, two->shader_type_);
    EXPECT_EQ(one->flags_, two->flags_);
    EXPECT_EQ(one->end_radius_, two->end_radius_);
    EXPECT_EQ(one->start_radius_, two->start_radius_);
    EXPECT_EQ(one->tx_, two->tx_);
    EXPECT_EQ(one->ty_, two->ty_);
    EXPECT_EQ(one->fallback_color_, two->fallback_color_);
    EXPECT_EQ(one->scaling_behavior_, two->scaling_behavior_);
    if (one->local_matrix_) {
      EXPECT_TRUE(two->local_matrix_.has_value());
      EXPECT_TRUE(*one->local_matrix_ == *two->local_matrix_);
    } else {
      EXPECT_FALSE(two->local_matrix_.has_value());
    }
    EXPECT_EQ(one->center_, two->center_);
    EXPECT_EQ(one->tile_, two->tile_);
    EXPECT_EQ(one->start_point_, two->start_point_);
    EXPECT_EQ(one->end_point_, two->end_point_);
    EXPECT_EQ(one->start_degrees_, two->start_degrees_);
    EXPECT_EQ(one->end_degrees_, two->end_degrees_);
    EXPECT_THAT(one->colors_, testing::ElementsAreArray(two->colors_));
    EXPECT_THAT(one->positions_, testing::ElementsAreArray(two->positions_));
  }

  static void ExpectPaintFlagsEqual(const PaintFlags& expected,
                                    const PaintFlags& actual) {
    // Can't just ToSkPaint and operator== here as SkPaint does pointer
    // comparisons on all the ref'd skia objects on the SkPaint, which
    // is not true after serialization.
    EXPECT_EQ(expected.getTextSize(), actual.getTextSize());
    EXPECT_EQ(expected.getColor(), actual.getColor());
    EXPECT_EQ(expected.getStrokeWidth(), actual.getStrokeWidth());
    EXPECT_EQ(expected.getStrokeMiter(), actual.getStrokeMiter());
    EXPECT_EQ(expected.getBlendMode(), actual.getBlendMode());
    EXPECT_EQ(expected.getStrokeCap(), actual.getStrokeCap());
    EXPECT_EQ(expected.getStrokeJoin(), actual.getStrokeJoin());
    EXPECT_EQ(expected.getStyle(), actual.getStyle());
    EXPECT_EQ(expected.getTextEncoding(), actual.getTextEncoding());
    EXPECT_EQ(expected.getHinting(), actual.getHinting());
    EXPECT_EQ(expected.getFilterQuality(), actual.getFilterQuality());

    // TODO(enne): compare typeface too
    ExpectFlattenableEqual(expected.getPathEffect().get(),
                           actual.getPathEffect().get());
    ExpectFlattenableEqual(expected.getMaskFilter().get(),
                           actual.getMaskFilter().get());
    ExpectFlattenableEqual(expected.getColorFilter().get(),
                           actual.getColorFilter().get());
    ExpectFlattenableEqual(expected.getLooper().get(),
                           actual.getLooper().get());
    ExpectFlattenableEqual(expected.getImageFilter().get(),
                           actual.getImageFilter().get());

    ExpectPaintShadersEqual(expected.getShader(), actual.getShader());
  }

  static void FillArbitraryShaderValues(PaintShader* shader, bool use_matrix) {
    shader->shader_type_ = PaintShader::Type::kTwoPointConicalGradient;
    shader->flags_ = 12345;
    shader->end_radius_ = 12.3f;
    shader->start_radius_ = 13.4f;
    shader->tx_ = SkShader::kRepeat_TileMode;
    shader->ty_ = SkShader::kMirror_TileMode;
    shader->fallback_color_ = SkColorSetARGB(254, 252, 250, 248);
    shader->scaling_behavior_ = PaintShader::ScalingBehavior::kRasterAtScale;
    if (use_matrix) {
      shader->local_matrix_.emplace(SkMatrix::I());
      shader->local_matrix_->setSkewX(10);
      shader->local_matrix_->setSkewY(20);
    }
    shader->center_ = SkPoint::Make(50, 40);
    shader->tile_ = SkRect::MakeXYWH(7, 77, 777, 7777);
    shader->start_point_ = SkPoint::Make(-1, -5);
    shader->end_point_ = SkPoint::Make(13, -13);
    shader->start_degrees_ = 123;
    shader->end_degrees_ = 456;
    // TODO(vmpstr): Add PaintImage/PaintRecord.
    shader->colors_ = {SkColorSetARGB(1, 2, 3, 4), SkColorSetARGB(5, 6, 7, 8),
                       SkColorSetARGB(9, 0, 1, 2)};
    shader->positions_ = {0.f, 0.4f, 1.f};
  }
};

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  buffer.Reset();
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  PaintOpBuffer buffer2(std::move(buffer));
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);
  EXPECT_EQ(buffer2.size(), 0u);
  EXPECT_EQ(buffer2.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer2), false);
}

class PaintOpAppendTest : public ::testing::Test {
 public:
  PaintOpAppendTest() {
    rect_ = SkRect::MakeXYWH(2, 3, 4, 5);
    flags_.setColor(SK_ColorMAGENTA);
    flags_.setAlpha(100);
  }

  void PushOps(PaintOpBuffer* buffer) {
    buffer->push<SaveLayerOp>(&rect_, &flags_);
    buffer->push<SaveOp>();
    buffer->push<DrawColorOp>(draw_color_, blend_);
    buffer->push<RestoreOp>();
    EXPECT_EQ(buffer->size(), 4u);
  }

  void VerifyOps(PaintOpBuffer* buffer) {
    EXPECT_EQ(buffer->size(), 4u);

    PaintOpBuffer::Iterator iter(buffer);
    ASSERT_EQ(iter->GetType(), PaintOpType::SaveLayer);
    SaveLayerOp* save_op = static_cast<SaveLayerOp*>(*iter);
    EXPECT_EQ(save_op->bounds, rect_);
    PaintOpSerializationTestUtils::ExpectPaintFlagsEqual(save_op->flags,
                                                         flags_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Save);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::DrawColor);
    DrawColorOp* op = static_cast<DrawColorOp*>(*iter);
    EXPECT_EQ(op->color, draw_color_);
    EXPECT_EQ(op->mode, blend_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
    ++iter;

    EXPECT_FALSE(iter);
  }

 private:
  SkRect rect_;
  PaintFlags flags_;
  SkColor draw_color_ = SK_ColorRED;
  SkBlendMode blend_ = SkBlendMode::kSrc;
};

TEST_F(PaintOpAppendTest, SimpleAppend) {
  PaintOpBuffer buffer;
  PushOps(&buffer);
  VerifyOps(&buffer);

  buffer.Reset();
  PushOps(&buffer);
  VerifyOps(&buffer);
}

TEST_F(PaintOpAppendTest, MoveThenDestruct) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination(std::move(original));
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&original), false);
}

TEST_F(PaintOpAppendTest, MoveThenDestructOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&original), false);
}

TEST_F(PaintOpAppendTest, MoveThenReappend) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination(std::move(original));

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
}

TEST_F(PaintOpAppendTest, MoveThenReappendOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

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
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrc);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// Same as above, but the save layer itself appears to be a noop.
// See: http://crbug.com/748485.  If the inner draw op itself
// doesn't support folding, then the external save can't be skipped.
TEST(PaintOpBufferTest, SaveDrawRestore_BadFlags255Alpha) {
  PaintOpBuffer buffer;

  uint8_t alpha = 255;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kColorBurn);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when there are more than one ops between the save and restore.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_TooManyOps) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

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
  buffer.Playback(&canvas);

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
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

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
  EXPECT_EQ(record->size(), 1u);

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

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
  EXPECT_EQ(record->size(), 1u);
  EXPECT_FALSE(record->GetFirstOp()->IsDrawOp());

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

TEST(PaintOpBufferTest, SaveLayerRestore_DrawColor) {
  PaintOpBuffer buffer;
  uint8_t alpha = 100;
  SkColor original = SkColorSetA(50, SK_ColorRED);

  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);
  buffer.push<DrawColorOp>(original, SkBlendMode::kSrcOver);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);
  EXPECT_EQ(canvas.save_count_, 0);
  EXPECT_EQ(canvas.restore_count_, 0);

  uint8_t expected_alpha = SkMulDiv255Round(alpha, SkColorGetA(original));
  EXPECT_EQ(canvas.paint_.getColor(), SkColorSetA(original, expected_alpha));
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_EmptyBuffer) {
  PaintOpBuffer buffer;
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_NoImageOp) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImage) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0), nullptr);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImageRect) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  buffer.push<DrawImageRectOp>(
      image, SkRect::MakeWH(100, 100), SkRect::MakeWH(100, 100), nullptr,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_OpWithFlags) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  auto image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  flags.setShader(PaintShader::MakeImage(std::move(image),
                                         SkShader::kClamp_TileMode,
                                         SkShader::kClamp_TileMode, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, SlowPaths) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(buffer->numSlowPaths(), 0);

  // Op without slow paths
  PaintFlags noop_flags;
  SkRect rect = SkRect::MakeXYWH(2, 3, 4, 5);
  buffer->push<SaveLayerOp>(&rect, &noop_flags);

  // Line op with a slow path
  PaintFlags line_effect_slow;
  line_effect_slow.setStrokeWidth(1.f);
  line_effect_slow.setStyle(PaintFlags::kStroke_Style);
  line_effect_slow.setStrokeCap(PaintFlags::kRound_Cap);
  SkScalar intervals[] = {1.f, 1.f};
  line_effect_slow.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_slow);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Line effect special case that Skia handles specially.
  PaintFlags line_effect = line_effect_slow;
  line_effect.setStrokeCap(PaintFlags::kButt_Cap);
  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Antialiased convex path is not slow.
  SkPath path;
  path.addCircle(2, 2, 5);
  EXPECT_TRUE(path.isConvex());
  buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Concave paths are slow only when antialiased.
  SkPath concave = path;
  concave.addCircle(3, 4, 2);
  EXPECT_FALSE(concave.isConvex());
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 2);
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, false);
  EXPECT_EQ(buffer->numSlowPaths(), 2);

  // Drawing a record with slow paths into another adds the same
  // number of slow paths as the record.
  auto buffer2 = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(0, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(2, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(4, buffer2->numSlowPaths());
}

TEST(PaintOpBufferTest, NonAAPaint) {
  // PaintOpWithFlags
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) with AA.
    PaintFlags line_effect;
    line_effect.setAntiAlias(true);
    buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) without AA.
    PaintFlags line_effect_no_aa;
    line_effect_no_aa.setAntiAlias(false);
    buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_no_aa);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // ClipPathOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    SkPath path;
    path.addCircle(2, 2, 5);

    // ClipPathOp with AA
    buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, true /* antialias */);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipPathOp without AA
    buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, false /* antialias */);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // ClipRRectOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipRRectOp with AA
    buffer->push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                              true /* antialias */);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipRRectOp without AA
    buffer->push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                              false /* antialias */);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // Drawing a record with non-aa paths into another propogates the value.
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    auto sub_buffer = sk_make_sp<PaintOpBuffer>();
    SkPath path;
    path.addCircle(2, 2, 5);
    sub_buffer->push<ClipPathOp>(path, SkClipOp::kIntersect,
                                 false /* antialias */);
    EXPECT_TRUE(sub_buffer->HasNonAAPaint());

    buffer->push<DrawRecordOp>(sub_buffer);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // The following PaintOpWithFlags types are overridden to *not* ever have
  // non-AA paint. AA is hard to notice, and these kick us out of MSAA in too
  // many cases.

  // DrawImageOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer->push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0),
                              &non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
  }

  // DrawIRectOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer->push<DrawIRectOp>(SkIRect::MakeWH(1, 1), non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
  }

  // SaveLayerOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    auto bounds = SkRect::MakeWH(1, 1);
    buffer->push<SaveLayerOp>(&bounds, &non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
  }
}

class PaintOpBufferOffsetsTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {
    offsets_.clear();
    buffer_.Reset();
  }

  template <typename T, typename... Args>
  void push_op(Args&&... args) {
    offsets_.push_back(buffer_.next_op_offset());
    buffer_.push<T>(std::forward<Args>(args)...);
  }

  // Returns a subset of offsets_ by selecting only the specified indices.
  std::vector<size_t> Select(const std::vector<size_t>& indices) {
    std::vector<size_t> result;
    for (size_t i : indices)
      result.push_back(offsets_[i]);
    return result;
  }

  void Playback(SkCanvas* canvas, const std::vector<size_t>& offsets) {
    buffer_.Playback(canvas, nullptr, nullptr, &offsets);
  }

 protected:
  std::vector<size_t> offsets_;
  PaintOpBuffer buffer_;
};

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndices) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays all items.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, NonContiguousIndices) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays 0, 1, 3, 4 indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, FirstTwoIndices) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays first two indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  Playback(&canvas, Select({0, 1}));
}

TEST_F(PaintOpBufferOffsetsTest, MiddleIndex) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays index 2.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  Playback(&canvas, Select({2}));
}

TEST_F(PaintOpBufferOffsetsTest, LastTwoElements) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays last two elements.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndicesWithSaveLayerAlphaRestore) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha, true);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Items are {0, 1, save, restore, 2, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped.
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       NonContiguousIndicesWithSaveLayerAlphaRestore) {
  MockCanvas canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha, true);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Items are {0, 1, save, 2, 3, restore, 4}.

  // Plays back all indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The SaveLayerAlpha/Restore is not dropped if we draw the middle
    // range, as we need them to represent the two draws inside the layer
    // correctly.
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // Skips the middle indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The now-empty SaveLayerAlpha/Restore is dropped
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       ContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  MockCanvas canvas;

  auto add_draw_rect = [this](SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    push_op<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(0u);
  add_draw_rect(1u);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(2u);
  push_op<RestoreOp>();
  add_draw_rect(3u);
  add_draw_rect(4u);

  // Items are {0, 1, save, 2, restore, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped, the containing
  // operation can be drawn with alpha.
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       NonContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  MockCanvas canvas;

  auto add_draw_rect = [this](SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    push_op<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(0u);
  add_draw_rect(1u);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(2u);
  add_draw_rect(3u);
  add_draw_rect(4u);
  push_op<RestoreOp>();

  // Items are are {0, 1, save, 2, 3, 4, restore}.

  // If the middle range is played, then the SaveLayerAlpha/Restore
  // can't be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 6}));
  }
}

TEST_F(PaintOpBufferOffsetsTest, FlatteningIteratorEmptyList) {
  std::vector<size_t> offsets = Select({});
  PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
  EXPECT_FALSE(iter);
}

TEST_F(PaintOpBufferOffsetsTest, FlatteningIteratorNoRecords) {
  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // all ops
  {
    size_t op_count = 0;
    std::vector<size_t> offsets = Select({0, 1, 2, 3, 4});
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      EXPECT_EQ(op_count, op->color);
      ++op_count;
    }
    EXPECT_EQ(5u, op_count);
  }

  // some ops
  {
    size_t op_count = 0;
    size_t expected_color = 0;
    std::vector<size_t> offsets = Select({0, 2, 4});
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      EXPECT_EQ(expected_color, op->color);
      expected_color += 2;
      ++op_count;
    }
    EXPECT_EQ(3u, op_count);
  }

  // no ops
  {
    std::vector<size_t> offsets = Select({});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }
}

TEST_F(PaintOpBufferOffsetsTest, FlatteningIteratorMultipleUnnestedRecords) {
  static constexpr SkBlendMode mode = SkBlendMode::kClear;

  // Deliberately start with a draw record.
  auto internal0 = sk_make_sp<PaintOpBuffer>();
  internal0->push<DrawColorOp>(0u, mode);
  push_op<DrawRecordOp>(internal0);
  push_op<DrawColorOp>(1u, mode);
  auto internal1 = sk_make_sp<PaintOpBuffer>();
  internal1->push<DrawColorOp>(2u, mode);
  internal1->push<DrawColorOp>(3u, mode);
  internal1->push<DrawColorOp>(4u, mode);
  push_op<DrawRecordOp>(internal1);
  // Two draw records back to back.
  auto internal2 = sk_make_sp<PaintOpBuffer>();
  internal2->push<DrawColorOp>(5u, mode);
  internal2->push<DrawColorOp>(6u, mode);
  push_op<DrawRecordOp>(internal2);
  push_op<DrawColorOp>(7u, SkBlendMode::kClear);
  // Test empty draw records.
  auto internal3 = sk_make_sp<PaintOpBuffer>();
  push_op<DrawRecordOp>(internal3);
  auto internal4 = sk_make_sp<PaintOpBuffer>();
  push_op<DrawRecordOp>(internal4);
  push_op<DrawColorOp>(8u, SkBlendMode::kClear);
  auto internal5 = sk_make_sp<PaintOpBuffer>();
  internal5->push<DrawColorOp>(9u, mode);
  internal5->push<DrawColorOp>(10u, mode);
  internal5->push<DrawColorOp>(11u, mode);
  internal5->push<DrawColorOp>(12u, mode);
  internal5->push<DrawColorOp>(13u, mode);
  // Deliberately end with a draw record.
  push_op<DrawRecordOp>(internal5);

  EXPECT_EQ(9u, buffer_.size());

  // all ops
  {
    size_t op_count = 0;
    std::vector<size_t> offsets = Select({0, 1, 2, 3, 4, 5, 6, 7, 8});
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      EXPECT_EQ(op_count, op->color);
      ++op_count;
    }
    EXPECT_EQ(14u, op_count);
  }

  // some ops
  {
    size_t op_count = 0;
    std::vector<size_t> offsets = Select({2, 4, 5, 6, 8});
    std::vector<size_t> expected_colors = {2, 3, 4, 7, 9, 10, 11, 12, 13};
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      ASSERT_LT(op_count, expected_colors.size());
      EXPECT_EQ(expected_colors[op_count], op->color);
      ++op_count;
    }
    EXPECT_EQ(expected_colors.size(), op_count);
  }

  // no ops
  {
    std::vector<size_t> offsets = Select({});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }
}

TEST_F(PaintOpBufferOffsetsTest, FlatteningIteratorManyNestedRecordsAllEmpty) {
  auto empty_record = sk_make_sp<PaintOpBuffer>();

  auto outer_record = sk_make_sp<PaintOpBuffer>();
  auto record_chain = sk_make_sp<PaintOpBuffer>();
  for (size_t i = 0; i < 5; ++i) {
    outer_record->push<DrawRecordOp>(empty_record);
    record_chain->push<DrawRecordOp>(empty_record);
  }
  for (size_t i = 0; i < 5; ++i) {
    auto next_parent = sk_make_sp<PaintOpBuffer>();
    next_parent->push<DrawRecordOp>(record_chain);
    record_chain = next_parent;
  }
  outer_record->push<DrawRecordOp>(record_chain);
  push_op<DrawRecordOp>(outer_record);

  // Test a single top level draw record op.
  {
    std::vector<size_t> offsets = Select({0});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }

  // Test multiple top level draw record ops.
  push_op<DrawRecordOp>(outer_record);
  push_op<DrawRecordOp>(outer_record);
  {
    std::vector<size_t> offsets = Select({0, 1, 2});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }

  // Test multiple top level draw record ops, but only some offsets.
  {
    std::vector<size_t> offsets = Select({1});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }

  // No offsets.
  {
    std::vector<size_t> offsets = Select({});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }
}

TEST_F(PaintOpBufferOffsetsTest, FlatteningIteratorManyNestedRecords) {
  static constexpr SkBlendMode mode = SkBlendMode::kClear;

  push_op<DrawColorOp>(0u, mode);
  auto internal0 = sk_make_sp<PaintOpBuffer>();
  auto internal00 = sk_make_sp<PaintOpBuffer>();
  auto internal000 = sk_make_sp<PaintOpBuffer>();
  internal000->push<DrawColorOp>(1u, mode);
  internal00->push<DrawRecordOp>(internal000);
  auto internal001 = sk_make_sp<PaintOpBuffer>();
  internal001->push<DrawColorOp>(2u, mode);
  internal001->push<DrawColorOp>(3u, mode);
  internal00->push<DrawRecordOp>(internal001);
  internal0->push<DrawRecordOp>(internal00);
  push_op<DrawRecordOp>(internal0);
  push_op<DrawColorOp>(4u, mode);

  auto chain = sk_make_sp<PaintOpBuffer>();
  chain->push<DrawColorOp>(5u, mode);
  chain->push<DrawColorOp>(6u, mode);
  chain->push<DrawColorOp>(7u, mode);
  for (size_t i = 0; i < 5; ++i) {
    auto next_parent = sk_make_sp<PaintOpBuffer>();
    next_parent->push<DrawRecordOp>(chain);
    chain = next_parent;
  }
  push_op<DrawRecordOp>(chain);

  EXPECT_EQ(4u, buffer_.size());

  // all ops
  {
    size_t op_count = 0;
    std::vector<size_t> offsets = Select({0, 1, 2, 3});
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      EXPECT_EQ(op_count, op->color);
      ++op_count;
    }
    EXPECT_EQ(8u, op_count);
  }

  // some ops
  {
    size_t op_count = 0;
    std::vector<size_t> offsets = Select({1, 3});
    std::vector<size_t> expected_colors = {1, 2, 3, 5, 6, 7};
    for (PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets); iter;
         ++iter) {
      PaintOp* base_op = *iter;
      ASSERT_EQ(base_op->GetType(), PaintOpType::DrawColor);
      DrawColorOp* op = static_cast<DrawColorOp*>(base_op);
      ASSERT_LT(op_count, expected_colors.size());
      EXPECT_EQ(expected_colors[op_count], op->color);
      ++op_count;
    }
    EXPECT_EQ(expected_colors.size(), op_count);
  }

  // no ops
  {
    std::vector<size_t> offsets = Select({});
    PaintOpBuffer::FlatteningIterator iter(&buffer_, &offsets);
    EXPECT_FALSE(iter);
  }
}

TEST(PaintOpBufferTest, SaveLayerAlphaDrawRestoreWithBadBlendMode) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    // This blend mode prevents the optimization.
    flags.setBlendMode(SkBlendMode::kSrc);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(&buffer, 0u);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 1u);
  buffer.push<RestoreOp>();
  add_draw_rect(&buffer, 2u);

  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    buffer.Playback(&canvas);
  }
}

TEST(PaintOpBufferTest, UnmatchedSaveRestoreNoSideEffects) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  // Push 2 saves.

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 0u);
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 1u);
  add_draw_rect(&buffer, 2u);
  // But only 1 restore.
  buffer.push<RestoreOp>();

  testing::Sequence s;
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  // We will restore back to the original save count regardless with 2 restores.
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  buffer.Playback(&canvas);
}

std::vector<float> test_floats = {0.f,
                                  1.f,
                                  -1.f,
                                  2384.981971f,
                                  0.0001f,
                                  std::numeric_limits<float>::min(),
                                  std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::infinity()};

std::vector<uint8_t> test_uint8s = {
    0, 255, 128, 10, 45,
};

std::vector<SkRect> test_rects = {
    SkRect::MakeXYWH(1, 2.5, 3, 4), SkRect::MakeXYWH(0, 0, 0, 0),
    SkRect::MakeLargest(),          SkRect::MakeXYWH(0.5f, 0.5f, 8.2f, 8.2f),
    SkRect::MakeXYWH(-1, -1, 0, 0), SkRect::MakeXYWH(-100, -101, -102, -103)};

std::vector<SkRRect> test_rrects = {
    SkRRect::MakeEmpty(), SkRRect::MakeOval(SkRect::MakeXYWH(1, 2, 3, 4)),
    SkRRect::MakeRect(SkRect::MakeXYWH(-10, 100, 5, 4)),
    [] {
      SkRRect rrect = SkRRect::MakeEmpty();
      rrect.setNinePatch(SkRect::MakeXYWH(10, 20, 30, 40), 1, 2, 3, 4);
      return rrect;
    }(),
};

std::vector<SkIRect> test_irects = {
    SkIRect::MakeXYWH(1, 2, 3, 4),   SkIRect::MakeXYWH(0, 0, 0, 0),
    SkIRect::MakeLargest(),          SkIRect::MakeXYWH(0, 0, 10, 10),
    SkIRect::MakeXYWH(-1, -1, 0, 0), SkIRect::MakeXYWH(-100, -101, -102, -103)};

std::vector<SkMatrix> test_matrices = {
    SkMatrix(),
    SkMatrix::MakeScale(3.91f, 4.31f),
    SkMatrix::MakeTrans(-5.2f, 8.7f),
    [] {
      SkMatrix matrix;
      SkScalar buffer[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
      matrix.set9(buffer);
      return matrix;
    }(),
    [] {
      SkMatrix matrix;
      SkScalar buffer[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      matrix.set9(buffer);
      return matrix;
    }(),
};

std::vector<SkPath> test_paths = {
    [] {
      SkPath path;
      path.moveTo(SkIntToScalar(20), SkIntToScalar(20));
      path.lineTo(SkIntToScalar(80), SkIntToScalar(20));
      path.lineTo(SkIntToScalar(30), SkIntToScalar(30));
      path.lineTo(SkIntToScalar(20), SkIntToScalar(80));
      return path;
    }(),
    [] {
      SkPath path;
      path.addCircle(2, 2, 5);
      path.addCircle(3, 4, 2);
      path.addArc(SkRect::MakeXYWH(1, 2, 3, 4), 5, 6);
      return path;
    }(),
    SkPath(),
};

std::vector<PaintFlags> test_flags = {
    PaintFlags(),
    [] {
      PaintFlags flags;
      flags.setTextSize(82.7f);
      flags.setColor(SK_ColorMAGENTA);
      flags.setStrokeWidth(4.2f);
      flags.setStrokeMiter(5.91f);
      flags.setBlendMode(SkBlendMode::kDst);
      flags.setStrokeCap(PaintFlags::kSquare_Cap);
      flags.setStrokeJoin(PaintFlags::kBevel_Join);
      flags.setStyle(PaintFlags::kStrokeAndFill_Style);
      flags.setTextEncoding(PaintFlags::kGlyphID_TextEncoding);
      flags.setHinting(PaintFlags::kNormal_Hinting);
      flags.setFilterQuality(SkFilterQuality::kMedium_SkFilterQuality);
      flags.setShader(PaintShader::MakeColor(SkColorSetARGB(1, 2, 3, 4)));
      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setTextSize(0.0f);
      flags.setColor(SK_ColorCYAN);
      flags.setAlpha(103);
      flags.setStrokeWidth(0.32f);
      flags.setStrokeMiter(7.98f);
      flags.setBlendMode(SkBlendMode::kSrcOut);
      flags.setStrokeCap(PaintFlags::kRound_Cap);
      flags.setStrokeJoin(PaintFlags::kRound_Join);
      flags.setStyle(PaintFlags::kFill_Style);
      flags.setTextEncoding(PaintFlags::kUTF32_TextEncoding);
      flags.setHinting(PaintFlags::kSlight_Hinting);
      flags.setFilterQuality(SkFilterQuality::kHigh_SkFilterQuality);

      SkScalar intervals[] = {1.f, 1.f};
      flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
      flags.setMaskFilter(
          SkBlurMaskFilter::Make(SkBlurStyle::kOuter_SkBlurStyle, 4.3f,
                                 test_rects[0], kHigh_SkBlurQuality));
      flags.setColorFilter(SkColorMatrixFilter::MakeLightingFilter(
          SK_ColorYELLOW, SK_ColorGREEN));

      SkLayerDrawLooper::Builder looper_builder;
      looper_builder.addLayer();
      looper_builder.addLayer(2.3f, 4.5f);
      SkLayerDrawLooper::LayerInfo layer_info;
      layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
      layer_info.fColorMode = SkBlendMode::kDst;
      layer_info.fOffset.set(-1.f, 5.2f);
      looper_builder.addLayer(layer_info);
      flags.setLooper(looper_builder.detach());

      flags.setImageFilter(SkOffsetImageFilter::Make(10, 11, nullptr));

      sk_sp<PaintShader> shader = PaintShader::MakeColor(SK_ColorTRANSPARENT);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               true);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setShader(PaintShader::MakeColor(SkColorSetARGB(12, 34, 56, 78)));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      sk_sp<PaintShader> shader = PaintShader::MakeColor(SK_ColorTRANSPARENT);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               false);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      SkPoint points[2] = {SkPoint::Make(1, 2), SkPoint::Make(3, 4)};
      SkColor colors[3] = {SkColorSetARGB(1, 2, 3, 4),
                           SkColorSetARGB(4, 3, 2, 1),
                           SkColorSetARGB(0, 10, 20, 30)};
      SkScalar positions[3] = {0.f, 0.3f, 1.f};
      flags.setShader(PaintShader::MakeLinearGradient(
          points, colors, positions, 3, SkShader::kMirror_TileMode));

      return flags;
    }(),
    PaintFlags(),
    PaintFlags(),
    PaintFlags(),
};

std::vector<SkColor> test_colors = {
    SkColorSetARGBInline(0, 0, 0, 0),
    SkColorSetARGBInline(255, 255, 255, 255),
    SkColorSetARGBInline(0, 255, 10, 255),
    SkColorSetARGBInline(255, 0, 20, 255),
    SkColorSetARGBInline(30, 255, 0, 255),
    SkColorSetARGBInline(255, 40, 0, 0),
};

std::vector<std::string> test_strings = {
    "", "foobar",
    "blarbideeblarasdfaiousydfp234poiausdofiuapsodfjknla;sdfkjasd;f",
};

std::vector<std::vector<SkPoint>> test_point_arrays = {
    std::vector<SkPoint>(),
    {SkPoint::Make(1, 2)},
    {SkPoint::Make(1, 2), SkPoint::Make(-5.4f, -3.8f)},
    {SkPoint::Make(0, 0), SkPoint::Make(5, 6), SkPoint::Make(-1, -1),
     SkPoint::Make(9, 9), SkPoint::Make(50, 50), SkPoint::Make(100, 100)},
};

std::vector<sk_sp<SkTextBlob>> test_blobs = {
    [] {
      SkPaint font;
      font.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

      SkTextBlobBuilder builder;
      builder.allocRun(font, 5, 1.2f, 2.3f, &test_rects[0]);
      return builder.make();
    }(),
    [] {
      SkPaint font;
      font.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

      SkTextBlobBuilder builder;
      builder.allocRun(font, 5, 1.2f, 2.3f, &test_rects[0]);
      builder.allocRunPos(font, 16, &test_rects[1]);
      builder.allocRunPosH(font, 8, 0, &test_rects[2]);
      return builder.make();
    }(),
};

// TODO(enne): In practice, probably all paint images need to be uploaded
// ahead of time and not be bitmaps. These paint images should be fake
// gpu resource paint images.
std::vector<PaintImage> test_images = {
    CreateDiscardablePaintImage(gfx::Size(5, 10)),
    CreateDiscardablePaintImage(gfx::Size(1, 1)),
    CreateDiscardablePaintImage(gfx::Size(50, 50)),
};

// Writes as many ops in |buffer| as can fit in |output_size| to |output|.
// Records the numbers of bytes written for each op.
class SimpleSerializer {
 public:
  SimpleSerializer(void* output, size_t output_size)
      : current_(static_cast<char*>(output)),
        output_size_(output_size),
        remaining_(output_size) {}

  void Serialize(const PaintOpBuffer& buffer) {
    bytes_written_.resize(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i)
      bytes_written_[i] = 0;

    PaintOp::SerializeOptions options;

    size_t op_idx = 0;
    for (const auto* op : PaintOpBuffer::Iterator(&buffer)) {
      size_t bytes_written = op->Serialize(current_, remaining_, options);
      if (!bytes_written)
        return;

      PaintOp* written = reinterpret_cast<PaintOp*>(current_);
      EXPECT_EQ(op->GetType(), written->GetType());
      EXPECT_EQ(bytes_written, written->skip);

      bytes_written_[op_idx] = bytes_written;
      op_idx++;
      current_ += bytes_written;
      remaining_ -= bytes_written;

      // Number of bytes bytes_written must be a multiple of PaintOpAlign
      // unless the buffer is filled entirely.
      if (remaining_ != 0u)
        DCHECK_EQ(0u, bytes_written % PaintOpBuffer::PaintOpAlign);
    }
  }

  const std::vector<size_t>& bytes_written() const { return bytes_written_; }
  size_t TotalBytesWritten() const { return output_size_ - remaining_; }

 private:
  char* current_ = nullptr;
  size_t output_size_ = 0u;
  size_t remaining_ = 0u;
  std::vector<size_t> bytes_written_;
};

class DeserializerIterator {
 public:
  DeserializerIterator(const void* input, size_t input_size)
      : DeserializerIterator(input,
                             static_cast<const char*>(input),
                             input_size,
                             input_size) {}

  DeserializerIterator(DeserializerIterator&&) = default;
  DeserializerIterator& operator=(DeserializerIterator&&) = default;

  ~DeserializerIterator() { DestroyDeserializedOp(); }

  DeserializerIterator begin() {
    return DeserializerIterator(input_, static_cast<const char*>(input_),
                                input_size_, input_size_);
  }
  DeserializerIterator end() {
    return DeserializerIterator(
        input_, static_cast<const char*>(input_) + input_size_, input_size_, 0);
  }
  bool operator!=(const DeserializerIterator& other) {
    return input_ != other.input_ || current_ != other.current_ ||
           input_size_ != other.input_size_ || remaining_ != other.remaining_;
  }
  DeserializerIterator& operator++() {
    CHECK_GE(remaining_, last_bytes_read_);
    current_ += last_bytes_read_;
    remaining_ -= last_bytes_read_;

    if (remaining_ > 0)
      CHECK_GE(remaining_, 4u);

    DeserializeCurrentOp();

    return *this;
  }

  operator bool() const { return remaining_ == 0u; }
  const PaintOp* operator->() const { return deserialized_op_; }
  const PaintOp* operator*() const { return deserialized_op_; }

 private:
  DeserializerIterator(const void* input,
                       const char* current,
                       size_t input_size,
                       size_t remaining)
      : input_(input),
        current_(current),
        input_size_(input_size),
        remaining_(remaining) {
    data_.reset(static_cast<char*>(base::AlignedAlloc(
        sizeof(LargestPaintOp), PaintOpBuffer::PaintOpAlign)));
    DeserializeCurrentOp();
  }

  void DestroyDeserializedOp() {
    if (!deserialized_op_)
      return;
    deserialized_op_->DestroyThis();
    deserialized_op_ = nullptr;
  }

  void DeserializeCurrentOp() {
    DestroyDeserializedOp();

    if (!remaining_)
      return;
    deserialized_op_ =
        PaintOp::Deserialize(current_, remaining_, data_.get(),
                             sizeof(LargestPaintOp), &last_bytes_read_);
  }

  const void* input_ = nullptr;
  const char* current_ = nullptr;
  size_t input_size_ = 0u;
  size_t remaining_ = 0u;
  size_t last_bytes_read_ = 0u;
  std::unique_ptr<char, base::AlignedFreeDeleter> data_;
  PaintOp* deserialized_op_ = nullptr;
};

void PushAnnotateOps(PaintOpBuffer* buffer) {
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::URL, test_rects[0],
                           SkData::MakeWithCString("thingerdoowhatchamagig"));
  // Deliberately test both null and empty SkData.
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::LINK_TO_DESTINATION,
                           test_rects[1], nullptr);
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::NAMED_DESTINATION,
                           test_rects[2], SkData::MakeEmpty());
  ValidateOps<AnnotateOp>(buffer);
}

void PushClipDeviceRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 1; i < test_irects.size(); i += 2) {
    SkClipOp op = i % 3 ? SkClipOp::kDifference : SkClipOp::kIntersect;
    buffer->push<ClipDeviceRectOp>(test_irects[i - 1], test_irects[0], op);
  }
  ValidateOps<ClipDeviceRectOp>(buffer);
}

void PushClipPathOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_paths.size(); ++i) {
    SkClipOp op = i % 3 ? SkClipOp::kDifference : SkClipOp::kIntersect;
    buffer->push<ClipPathOp>(test_paths[i], op, !!(i % 2));
  }
  ValidateOps<ClipPathOp>(buffer);
}

void PushClipRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRectOp>(test_rects[i], op, antialias);
  }
  ValidateOps<ClipRectOp>(buffer);
}

void PushClipRRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rrects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRRectOp>(test_rrects[i], op, antialias);
  }
  ValidateOps<ClipRRectOp>(buffer);
}

void PushConcatOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_matrices.size(); ++i)
    buffer->push<ConcatOp>(test_matrices[i]);
  ValidateOps<ConcatOp>(buffer);
}

void PushDrawArcOps(PaintOpBuffer* buffer) {
  size_t len = std::min(std::min(test_floats.size() - 1, test_flags.size()),
                        test_rects.size());
  for (size_t i = 0; i < len; ++i) {
    bool use_center = !!(i % 2);
    buffer->push<DrawArcOp>(test_rects[i], test_floats[i], test_floats[i + 1],
                            use_center, test_flags[i]);
  }
  ValidateOps<DrawArcOp>(buffer);
}

void PushDrawColorOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_colors.size(); ++i) {
    buffer->push<DrawColorOp>(test_colors[i], static_cast<SkBlendMode>(i));
  }
  ValidateOps<DrawColorOp>(buffer);
}

void PushDrawDRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size() - 1, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawDRRectOp>(test_rrects[i], test_rrects[i + 1],
                               test_flags[i]);
  }
  ValidateOps<DrawDRRectOp>(buffer);
}

void PushDrawImageOps(PaintOpBuffer* buffer) {
  size_t len = std::min(std::min(test_images.size(), test_flags.size()),
                        test_floats.size() - 1);
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawImageOp>(test_images[i], test_floats[i],
                              test_floats[i + 1], &test_flags[i]);
  }

  // Test optional flags
  // TODO(enne): maybe all these optional ops should not be optional.
  buffer->push<DrawImageOp>(test_images[0], test_floats[0], test_floats[1],
                            nullptr);
  ValidateOps<DrawImageOp>(buffer);
}

void PushDrawImageRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(std::min(test_images.size(), test_flags.size()),
                        test_rects.size() - 1);
  for (size_t i = 0; i < len; ++i) {
    PaintCanvas::SrcRectConstraint constraint =
        i % 2 ? PaintCanvas::kStrict_SrcRectConstraint
              : PaintCanvas::kFast_SrcRectConstraint;
    buffer->push<DrawImageRectOp>(test_images[i], test_rects[i],
                                  test_rects[i + 1], &test_flags[i],
                                  constraint);
  }

  // Test optional flags.
  buffer->push<DrawImageRectOp>(test_images[0], test_rects[0], test_rects[1],
                                nullptr,
                                PaintCanvas::kStrict_SrcRectConstraint);
  ValidateOps<DrawImageRectOp>(buffer);
}

void PushDrawIRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_irects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawIRectOp>(test_irects[i], test_flags[i]);
  ValidateOps<DrawIRectOp>(buffer);
}

void PushDrawLineOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_floats.size() - 3, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawLineOp>(test_floats[i], test_floats[i + 1],
                             test_floats[i + 2], test_floats[i + 3],
                             test_flags[i]);
  }
  ValidateOps<DrawLineOp>(buffer);
}

void PushDrawOvalOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawOvalOp>(test_rects[i], test_flags[i]);
  ValidateOps<DrawOvalOp>(buffer);
}

void PushDrawPathOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawPathOp>(test_paths[i], test_flags[i]);
  ValidateOps<DrawPathOp>(buffer);
}

void PushDrawRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRectOp>(test_rects[i], test_flags[i]);
  ValidateOps<DrawRectOp>(buffer);
}

void PushDrawRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRRectOp>(test_rrects[i], test_flags[i]);
  ValidateOps<DrawRRectOp>(buffer);
}

void PushDrawTextBlobOps(PaintOpBuffer* buffer) {
  size_t len = std::min(std::min(test_blobs.size(), test_flags.size()),
                        test_floats.size() - 1);
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawTextBlobOp>(test_blobs[i], test_floats[i],
                                 test_floats[i + 1], test_flags[i]);
  }
  ValidateOps<DrawTextBlobOp>(buffer);
}

void PushNoopOps(PaintOpBuffer* buffer) {
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  ValidateOps<NoopOp>(buffer);
}

void PushRestoreOps(PaintOpBuffer* buffer) {
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  ValidateOps<RestoreOp>(buffer);
}

void PushRotateOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size(); ++i)
    buffer->push<RotateOp>(test_floats[i]);
  ValidateOps<RotateOp>(buffer);
}

void PushSaveOps(PaintOpBuffer* buffer) {
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  ValidateOps<SaveOp>(buffer);
}

void PushSaveLayerOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_flags.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerOp>(&test_rects[i], &test_flags[i]);

  // Test combinations of optional args.
  buffer->push<SaveLayerOp>(nullptr, &test_flags[0]);
  buffer->push<SaveLayerOp>(&test_rects[0], nullptr);
  buffer->push<SaveLayerOp>(nullptr, nullptr);
  ValidateOps<SaveLayerOp>(buffer);
}

void PushSaveLayerAlphaOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_uint8s.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerAlphaOp>(&test_rects[i], test_uint8s[i], !!(i % 2));

  // Test optional args.
  buffer->push<SaveLayerAlphaOp>(nullptr, test_uint8s[0], false);
  ValidateOps<SaveLayerAlphaOp>(buffer);
}

void PushScaleOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size(); i += 2)
    buffer->push<ScaleOp>(test_floats[i], test_floats[i + 1]);
  ValidateOps<ScaleOp>(buffer);
}

void PushSetMatrixOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_matrices.size(); ++i)
    buffer->push<SetMatrixOp>(test_matrices[i]);
  ValidateOps<SetMatrixOp>(buffer);
}

void PushTranslateOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size(); i += 2)
    buffer->push<TranslateOp>(test_floats[i], test_floats[i + 1]);
  ValidateOps<TranslateOp>(buffer);
}

void CompareFlags(const PaintFlags& original, const PaintFlags& written) {
  PaintOpSerializationTestUtils::ExpectPaintFlagsEqual(original, written);
}

void CompareImages(const PaintImage& original, const PaintImage& written) {}

void CompareAnnotateOp(const AnnotateOp* original, const AnnotateOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->annotation_type, written->annotation_type);
  EXPECT_EQ(original->rect, written->rect);
  EXPECT_EQ(!!original->data, !!written->data);
  if (original->data) {
    EXPECT_EQ(original->data->size(), written->data->size());
    EXPECT_EQ(0, memcmp(original->data->data(), written->data->data(),
                        written->data->size()));
  }
}

void CompareClipDeviceRectOp(const ClipDeviceRectOp* original,
                             const ClipDeviceRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->device_rect, written->device_rect);
  EXPECT_EQ(original->subtract_rect, written->subtract_rect);
  EXPECT_EQ(original->op, written->op);
}

void CompareClipPathOp(const ClipPathOp* original, const ClipPathOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_TRUE(original->path == written->path);
  EXPECT_EQ(original->op, written->op);
  EXPECT_EQ(original->antialias, written->antialias);
}

void CompareClipRectOp(const ClipRectOp* original, const ClipRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->rect, written->rect);
  EXPECT_EQ(original->op, written->op);
  EXPECT_EQ(original->antialias, written->antialias);
}

void CompareClipRRectOp(const ClipRRectOp* original,
                        const ClipRRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->rrect, written->rrect);
  EXPECT_EQ(original->op, written->op);
  EXPECT_EQ(original->antialias, written->antialias);
}

void CompareConcatOp(const ConcatOp* original, const ConcatOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->matrix, written->matrix);
  EXPECT_EQ(original->matrix.getType(), written->matrix.getType());
}

void CompareDrawArcOp(const DrawArcOp* original, const DrawArcOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->oval, written->oval);
  EXPECT_EQ(original->start_angle, written->start_angle);
  EXPECT_EQ(original->sweep_angle, written->sweep_angle);
  EXPECT_EQ(original->use_center, written->use_center);
}

void CompareDrawColorOp(const DrawColorOp* original,
                        const DrawColorOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->color, written->color);
}

void CompareDrawDRRectOp(const DrawDRRectOp* original,
                         const DrawDRRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->outer, written->outer);
  EXPECT_EQ(original->inner, written->inner);
}

void CompareDrawImageOp(const DrawImageOp* original,
                        const DrawImageOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  CompareImages(original->image, written->image);
  EXPECT_EQ(original->left, written->left);
  EXPECT_EQ(original->top, written->top);
}

void CompareDrawImageRectOp(const DrawImageRectOp* original,
                            const DrawImageRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  CompareImages(original->image, written->image);
  EXPECT_EQ(original->src, written->src);
  EXPECT_EQ(original->dst, written->dst);
}

void CompareDrawIRectOp(const DrawIRectOp* original,
                        const DrawIRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->rect, written->rect);
}

void CompareDrawLineOp(const DrawLineOp* original, const DrawLineOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->x0, written->x0);
  EXPECT_EQ(original->y0, written->y0);
  EXPECT_EQ(original->x1, written->x1);
  EXPECT_EQ(original->y1, written->y1);
}

void CompareDrawOvalOp(const DrawOvalOp* original, const DrawOvalOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->oval, written->oval);
}

void CompareDrawPathOp(const DrawPathOp* original, const DrawPathOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_TRUE(original->path == written->path);
}

void CompareDrawRectOp(const DrawRectOp* original, const DrawRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->rect, written->rect);
}

void CompareDrawRRectOp(const DrawRRectOp* original,
                        const DrawRRectOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->rrect, written->rrect);
}

void CompareDrawTextBlobOp(const DrawTextBlobOp* original,
                           const DrawTextBlobOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->x, written->x);
  EXPECT_EQ(original->y, written->y);

  // TODO(enne): implement SkTextBlob serialization: http://crbug.com/737629
  if (!original->blob || !written->blob)
    return;

  ASSERT_TRUE(original->blob);
  ASSERT_TRUE(written->blob);

  // No text blob operator==, so flatten them both and compare.
  size_t max_size = original->skip;

  std::vector<char> original_mem;
  original_mem.resize(max_size);
  SkBinaryWriteBuffer original_flattened(&original_mem[0], max_size);
  original->blob->flatten(original_flattened);
  original_mem.resize(original_flattened.bytesWritten());

  std::vector<char> written_mem;
  written_mem.resize(max_size);
  SkBinaryWriteBuffer written_flattened(&written_mem[0], max_size);
  written->blob->flatten(written_flattened);
  written_mem.resize(written_flattened.bytesWritten());

  ASSERT_EQ(original_mem.size(), written_mem.size());
  EXPECT_EQ(original_mem, written_mem);
}

void CompareNoopOp(const NoopOp* original, const NoopOp* written) {
  // Nothing to compare.
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
}

void CompareRestoreOp(const RestoreOp* original, const RestoreOp* written) {
  // Nothing to compare.
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
}

void CompareRotateOp(const RotateOp* original, const RotateOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->degrees, written->degrees);
}

void CompareSaveOp(const SaveOp* original, const SaveOp* written) {
  // Nothing to compare.
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
}

void CompareSaveLayerOp(const SaveLayerOp* original,
                        const SaveLayerOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  CompareFlags(original->flags, written->flags);
  EXPECT_EQ(original->bounds, written->bounds);
}

void CompareSaveLayerAlphaOp(const SaveLayerAlphaOp* original,
                             const SaveLayerAlphaOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->bounds, written->bounds);
  EXPECT_EQ(original->alpha, written->alpha);
  EXPECT_EQ(original->preserve_lcd_text_requests,
            written->preserve_lcd_text_requests);
}

void CompareScaleOp(const ScaleOp* original, const ScaleOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->sx, written->sx);
  EXPECT_EQ(original->sy, written->sy);
}

void CompareSetMatrixOp(const SetMatrixOp* original,
                        const SetMatrixOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->matrix, written->matrix);
}

void CompareTranslateOp(const TranslateOp* original,
                        const TranslateOp* written) {
  EXPECT_TRUE(original->IsValid());
  EXPECT_TRUE(written->IsValid());
  EXPECT_EQ(original->dx, written->dx);
  EXPECT_EQ(original->dy, written->dy);
}

class PaintOpSerializationTest : public ::testing::TestWithParam<uint8_t> {
 public:
  PaintOpType GetParamType() const {
    return static_cast<PaintOpType>(GetParam());
  }

  void PushTestOps(PaintOpType type) {
    switch (type) {
      case PaintOpType::Annotate:
        PushAnnotateOps(&buffer_);
        break;
      case PaintOpType::ClipDeviceRect:
        PushClipDeviceRectOps(&buffer_);
        break;
      case PaintOpType::ClipPath:
        PushClipPathOps(&buffer_);
        break;
      case PaintOpType::ClipRect:
        PushClipRectOps(&buffer_);
        break;
      case PaintOpType::ClipRRect:
        PushClipRRectOps(&buffer_);
        break;
      case PaintOpType::Concat:
        PushConcatOps(&buffer_);
        break;
      case PaintOpType::DrawArc:
        PushDrawArcOps(&buffer_);
        break;
      case PaintOpType::DrawColor:
        PushDrawColorOps(&buffer_);
        break;
      case PaintOpType::DrawDRRect:
        PushDrawDRRectOps(&buffer_);
        break;
      case PaintOpType::DrawImage:
        PushDrawImageOps(&buffer_);
        break;
      case PaintOpType::DrawImageRect:
        PushDrawImageRectOps(&buffer_);
        break;
      case PaintOpType::DrawIRect:
        PushDrawIRectOps(&buffer_);
        break;
      case PaintOpType::DrawLine:
        PushDrawLineOps(&buffer_);
        break;
      case PaintOpType::DrawOval:
        PushDrawOvalOps(&buffer_);
        break;
      case PaintOpType::DrawPath:
        PushDrawPathOps(&buffer_);
        break;
      case PaintOpType::DrawRecord:
        // Not supported.
        break;
      case PaintOpType::DrawRect:
        PushDrawRectOps(&buffer_);
        break;
      case PaintOpType::DrawRRect:
        PushDrawRRectOps(&buffer_);
        break;
      case PaintOpType::DrawTextBlob:
        PushDrawTextBlobOps(&buffer_);
        break;
      case PaintOpType::Noop:
        PushNoopOps(&buffer_);
        break;
      case PaintOpType::Restore:
        PushRestoreOps(&buffer_);
        break;
      case PaintOpType::Rotate:
        PushRotateOps(&buffer_);
        break;
      case PaintOpType::Save:
        PushSaveOps(&buffer_);
        break;
      case PaintOpType::SaveLayer:
        PushSaveLayerOps(&buffer_);
        break;
      case PaintOpType::SaveLayerAlpha:
        PushSaveLayerAlphaOps(&buffer_);
        break;
      case PaintOpType::Scale:
        PushScaleOps(&buffer_);
        break;
      case PaintOpType::SetMatrix:
        PushSetMatrixOps(&buffer_);
        break;
      case PaintOpType::Translate:
        PushTranslateOps(&buffer_);
        break;
    }
  }

  static void ExpectOpsEqual(const PaintOp* original, const PaintOp* written) {
    ASSERT_TRUE(original);
    ASSERT_TRUE(written);
    ASSERT_EQ(original->GetType(), written->GetType());
    EXPECT_EQ(original->skip, written->skip);

    switch (original->GetType()) {
      case PaintOpType::Annotate:
        CompareAnnotateOp(static_cast<const AnnotateOp*>(original),
                          static_cast<const AnnotateOp*>(written));
        break;
      case PaintOpType::ClipDeviceRect:
        CompareClipDeviceRectOp(static_cast<const ClipDeviceRectOp*>(original),
                                static_cast<const ClipDeviceRectOp*>(written));
        break;
      case PaintOpType::ClipPath:
        CompareClipPathOp(static_cast<const ClipPathOp*>(original),
                          static_cast<const ClipPathOp*>(written));
        break;
      case PaintOpType::ClipRect:
        CompareClipRectOp(static_cast<const ClipRectOp*>(original),
                          static_cast<const ClipRectOp*>(written));
        break;
      case PaintOpType::ClipRRect:
        CompareClipRRectOp(static_cast<const ClipRRectOp*>(original),
                           static_cast<const ClipRRectOp*>(written));
        break;
      case PaintOpType::Concat:
        CompareConcatOp(static_cast<const ConcatOp*>(original),
                        static_cast<const ConcatOp*>(written));
        break;
      case PaintOpType::DrawArc:
        CompareDrawArcOp(static_cast<const DrawArcOp*>(original),
                         static_cast<const DrawArcOp*>(written));
        break;
      case PaintOpType::DrawColor:
        CompareDrawColorOp(static_cast<const DrawColorOp*>(original),
                           static_cast<const DrawColorOp*>(written));
        break;
      case PaintOpType::DrawDRRect:
        CompareDrawDRRectOp(static_cast<const DrawDRRectOp*>(original),
                            static_cast<const DrawDRRectOp*>(written));
        break;
      case PaintOpType::DrawImage:
        CompareDrawImageOp(static_cast<const DrawImageOp*>(original),
                           static_cast<const DrawImageOp*>(written));
        break;
      case PaintOpType::DrawImageRect:
        CompareDrawImageRectOp(static_cast<const DrawImageRectOp*>(original),
                               static_cast<const DrawImageRectOp*>(written));
        break;
      case PaintOpType::DrawIRect:
        CompareDrawIRectOp(static_cast<const DrawIRectOp*>(original),
                           static_cast<const DrawIRectOp*>(written));
        break;
      case PaintOpType::DrawLine:
        CompareDrawLineOp(static_cast<const DrawLineOp*>(original),
                          static_cast<const DrawLineOp*>(written));
        break;
      case PaintOpType::DrawOval:
        CompareDrawOvalOp(static_cast<const DrawOvalOp*>(original),
                          static_cast<const DrawOvalOp*>(written));
        break;
      case PaintOpType::DrawPath:
        CompareDrawPathOp(static_cast<const DrawPathOp*>(original),
                          static_cast<const DrawPathOp*>(written));
        break;
      case PaintOpType::DrawRecord:
        // Not supported.
        break;
      case PaintOpType::DrawRect:
        CompareDrawRectOp(static_cast<const DrawRectOp*>(original),
                          static_cast<const DrawRectOp*>(written));
        break;
      case PaintOpType::DrawRRect:
        CompareDrawRRectOp(static_cast<const DrawRRectOp*>(original),
                           static_cast<const DrawRRectOp*>(written));
        break;
      case PaintOpType::DrawTextBlob:
        CompareDrawTextBlobOp(static_cast<const DrawTextBlobOp*>(original),
                              static_cast<const DrawTextBlobOp*>(written));
        break;
      case PaintOpType::Noop:
        CompareNoopOp(static_cast<const NoopOp*>(original),
                      static_cast<const NoopOp*>(written));
        break;
      case PaintOpType::Restore:
        CompareRestoreOp(static_cast<const RestoreOp*>(original),
                         static_cast<const RestoreOp*>(written));
        break;
      case PaintOpType::Rotate:
        CompareRotateOp(static_cast<const RotateOp*>(original),
                        static_cast<const RotateOp*>(written));
        break;
      case PaintOpType::Save:
        CompareSaveOp(static_cast<const SaveOp*>(original),
                      static_cast<const SaveOp*>(written));
        break;
      case PaintOpType::SaveLayer:
        CompareSaveLayerOp(static_cast<const SaveLayerOp*>(original),
                           static_cast<const SaveLayerOp*>(written));
        break;
      case PaintOpType::SaveLayerAlpha:
        CompareSaveLayerAlphaOp(static_cast<const SaveLayerAlphaOp*>(original),
                                static_cast<const SaveLayerAlphaOp*>(written));
        break;
      case PaintOpType::Scale:
        CompareScaleOp(static_cast<const ScaleOp*>(original),
                       static_cast<const ScaleOp*>(written));
        break;
      case PaintOpType::SetMatrix:
        CompareSetMatrixOp(static_cast<const SetMatrixOp*>(original),
                           static_cast<const SetMatrixOp*>(written));
        break;
      case PaintOpType::Translate:
        CompareTranslateOp(static_cast<const TranslateOp*>(original),
                           static_cast<const TranslateOp*>(written));
        break;
    }
  }

  void ResizeOutputBuffer() {
    // An arbitrary deserialization buffer size that should fit all the ops
    // in the buffer_.
    output_size_ = kBufferBytesPerOp * buffer_.size();
    output_.reset(static_cast<char*>(
        base::AlignedAlloc(output_size_, PaintOpBuffer::PaintOpAlign)));
  }

  bool IsTypeSupported() {
    // DrawRecordOps must be flattened and are not currently serialized.
    // All other types must push non-zero amounts of ops in PushTestOps.
    return GetParamType() != PaintOpType::DrawRecord;
  }

 protected:
  std::unique_ptr<char, base::AlignedFreeDeleter> output_;
  size_t output_size_ = 0u;
  PaintOpBuffer buffer_;
};

INSTANTIATE_TEST_CASE_P(
    P,
    PaintOpSerializationTest,
    ::testing::Range(static_cast<uint8_t>(0),
                     static_cast<uint8_t>(PaintOpType::LastPaintOpType)));

// Test serializing and then deserializing all test ops.  They should all
// write successfully and be identical to the original ops in the buffer.
TEST_P(PaintOpSerializationTest, SmokeTest) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);
  serializer.Serialize(buffer_);

  // Expect all ops to write more than 0 bytes.
  for (size_t i = 0; i < buffer_.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zd", PaintOpTypeToString(GetParamType()).c_str(), i));
    EXPECT_GT(serializer.bytes_written()[i], 0u);
  }

  PaintOpBuffer::Iterator iter(&buffer_);
  size_t i = 0;
  for (auto* base_written :
       DeserializerIterator(output_.get(), serializer.TotalBytesWritten())) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zu", PaintOpTypeToString(GetParamType()).c_str(), i));
    ExpectOpsEqual(*iter, base_written);
    ++iter;
    ++i;
  }

  EXPECT_EQ(buffer_.size(), i);
}

// Verify for all test ops that serializing into a smaller size aborts
// correctly and doesn't write anything.
TEST_P(PaintOpSerializationTest, SerializationFailures) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);
  serializer.Serialize(buffer_);
  std::vector<size_t> bytes_written = serializer.bytes_written();

  PaintOp::SerializeOptions options;

  size_t op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer_); iter; ++iter, ++op_idx) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zu", PaintOpTypeToString(GetParamType()).c_str(), op_idx));
    size_t expected_bytes = bytes_written[op_idx];
    EXPECT_GT(expected_bytes, 0u);

    // Attempt to write op into a buffer of size |i|, and only expect
    // it to succeed if the buffer is large enough.
    for (size_t i = 0; i < bytes_written[op_idx] + 2; ++i) {
      size_t written_bytes = iter->Serialize(output_.get(), i, options);
      if (i >= expected_bytes) {
        EXPECT_EQ(expected_bytes, written_bytes) << "i: " << i;
      } else {
        EXPECT_EQ(0u, written_bytes) << "i: " << i;
      }
    }
  }
}

// Verify that deserializing test ops from too small buffers aborts
// correctly, in case the deserialized data is lying about how big it is.
TEST_P(PaintOpSerializationTest, DeserializationFailures) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);
  serializer.Serialize(buffer_);

  char* current = static_cast<char*>(output_.get());

  static constexpr size_t kAlign = PaintOpBuffer::PaintOpAlign;
  static constexpr size_t kOutputOpSize = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialize_buffer_(
      static_cast<char*>(base::AlignedAlloc(kOutputOpSize, kAlign)));

  size_t op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer_); iter; ++iter, ++op_idx) {
    PaintOp* serialized = reinterpret_cast<PaintOp*>(current);
    uint32_t skip = serialized->skip;

    // Read from buffers of various sizes to make sure that having a serialized
    // op size that is larger than the input buffer provided causes a
    // deserialization failure to return nullptr.  Also test a few valid sizes
    // larger than read size.
    for (size_t read_size = 0; read_size < skip + kAlign * 2 + 2; ++read_size) {
      SCOPED_TRACE(base::StringPrintf(
          "%s #%zd, read_size: %zu",
          PaintOpTypeToString(GetParamType()).c_str(), op_idx, read_size));
      // Because PaintOp::Deserialize early outs when the input size is < skip
      // deliberately lie about the skip.  This op tooooootally fits.
      // This will verify that individual op deserializing code behaves
      // properly when presented with invalid offsets.
      serialized->skip = read_size;
      size_t bytes_read = 0;
      PaintOp* written =
          PaintOp::Deserialize(current, read_size, deserialize_buffer_.get(),
                               kOutputOpSize, &bytes_read);

      // Skips are only valid if they are aligned.
      if (read_size >= skip && read_size % kAlign == 0) {
        ASSERT_NE(nullptr, written);
        ASSERT_LE(written->skip, kOutputOpSize);
        EXPECT_EQ(GetParamType(), written->GetType());
        EXPECT_EQ(serialized->skip, bytes_read);
      } else {
        EXPECT_EQ(nullptr, written);
      }

      if (written)
        written->DestroyThis();
    }

    current += skip;
  }
}

// Test generic PaintOp deserializing failure cases.
TEST(PaintOpBufferTest, PaintOpDeserialize) {
  static constexpr size_t kSize = sizeof(LargestPaintOp) + 100;
  static constexpr size_t kAlign = PaintOpBuffer::PaintOpAlign;
  std::unique_ptr<char, base::AlignedFreeDeleter> input_(
      static_cast<char*>(base::AlignedAlloc(kSize, kAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> output_(
      static_cast<char*>(base::AlignedAlloc(kSize, kAlign)));

  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SK_ColorMAGENTA, SkBlendMode::kSrc);

  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* op = *iter;
  ASSERT_TRUE(op);

  PaintOp::SerializeOptions options;
  size_t bytes_written = op->Serialize(input_.get(), kSize, options);
  ASSERT_GT(bytes_written, 0u);

  // can deserialize from exactly the right size
  size_t bytes_read = 0;
  PaintOp* success = PaintOp::Deserialize(input_.get(), bytes_written,
                                          output_.get(), kSize, &bytes_read);
  ASSERT_TRUE(success);
  EXPECT_EQ(bytes_written, bytes_read);
  success->DestroyThis();

  // fail to deserialize if skip goes past input size
  // (the DeserializationFailures test above tests if the skip is lying)
  for (size_t i = 0; i < bytes_written - 1; ++i)
    EXPECT_FALSE(PaintOp::Deserialize(input_.get(), i, output_.get(), kSize,
                                      &bytes_read));

  // unaligned skips fail to deserialize
  PaintOp* serialized = reinterpret_cast<PaintOp*>(input_.get());
  EXPECT_EQ(0u, serialized->skip % kAlign);
  serialized->skip -= 1;
  EXPECT_FALSE(PaintOp::Deserialize(input_.get(), bytes_written, output_.get(),
                                    kSize, &bytes_read));
  serialized->skip += 1;

  // bogus types fail to deserialize
  serialized->type = static_cast<uint8_t>(PaintOpType::LastPaintOpType) + 1;
  EXPECT_FALSE(PaintOp::Deserialize(input_.get(), bytes_written, output_.get(),
                                    kSize, &bytes_read));
}

// Test that deserializing invalid SkClipOp enums fails silently.
// Skia release asserts on this in several places so these are not safe
// to pass through to the SkCanvas API.
TEST(PaintOpBufferTest, ValidateSkClip) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  PaintOpBuffer buffer;

  // Successful first op.
  SkPath path;
  buffer.push<ClipPathOp>(path, SkClipOp::kMax_EnumValue, true);

  // Bad other ops.
  SkClipOp bad_clip = static_cast<SkClipOp>(
      static_cast<uint32_t>(SkClipOp::kMax_EnumValue) + 1);

  buffer.push<ClipPathOp>(path, bad_clip, true);
  buffer.push<ClipRectOp>(test_rects[0], bad_clip, true);
  buffer.push<ClipRRectOp>(test_rrects[0], bad_clip, false);

  SkClipOp bad_clip_max = static_cast<SkClipOp>(~static_cast<uint32_t>(0));
  buffer.push<ClipRectOp>(test_rects[1], bad_clip_max, false);

  PaintOp::SerializeOptions options;

  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written =
        op->Serialize(serialized.get(), buffer_size, options);
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written =
        PaintOp::Deserialize(serialized.get(), bytes_written,
                             deserialized.get(), buffer_size, &bytes_read);
    // First op should succeed.  Other ops with bad enums should
    // serialize correctly but fail to deserialize due to the bad
    // SkClipOp enum.
    if (!op_idx) {
      EXPECT_TRUE(written) << "op: " << op_idx;
      EXPECT_EQ(bytes_written, bytes_read);
      written->DestroyThis();
    } else {
      EXPECT_FALSE(written) << "op: " << op_idx;
    }

    ++op_idx;
  }
}

TEST(PaintOpBufferTest, ValidateSkBlendMode) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  PaintOpBuffer buffer;

  // Successful first two ops.
  buffer.push<DrawColorOp>(SK_ColorMAGENTA, SkBlendMode::kDstIn);
  PaintFlags good_flags = test_flags[0];
  good_flags.setBlendMode(SkBlendMode::kColorBurn);
  buffer.push<DrawRectOp>(test_rects[0], good_flags);

  // Modes that are not supported by drawColor or SkPaint.
  SkBlendMode bad_modes_for_draw_color[] = {
      SkBlendMode::kOverlay,
      SkBlendMode::kDarken,
      SkBlendMode::kLighten,
      SkBlendMode::kColorDodge,
      SkBlendMode::kColorBurn,
      SkBlendMode::kHardLight,
      SkBlendMode::kSoftLight,
      SkBlendMode::kDifference,
      SkBlendMode::kExclusion,
      SkBlendMode::kMultiply,
      SkBlendMode::kHue,
      SkBlendMode::kSaturation,
      SkBlendMode::kColor,
      SkBlendMode::kLuminosity,
      static_cast<SkBlendMode>(static_cast<uint32_t>(SkBlendMode::kLastMode) +
                               1),
      static_cast<SkBlendMode>(static_cast<uint32_t>(~0)),
  };

  SkBlendMode bad_modes_for_flags[] = {
      static_cast<SkBlendMode>(static_cast<uint32_t>(SkBlendMode::kLastMode) +
                               1),
      static_cast<SkBlendMode>(static_cast<uint32_t>(~0)),
  };

  for (size_t i = 0; i < arraysize(bad_modes_for_draw_color); ++i) {
    buffer.push<DrawColorOp>(SK_ColorMAGENTA, bad_modes_for_draw_color[i]);
  }

  for (size_t i = 0; i < arraysize(bad_modes_for_flags); ++i) {
    PaintFlags flags = test_flags[i % test_flags.size()];
    flags.setBlendMode(bad_modes_for_flags[i]);
    buffer.push<DrawRectOp>(test_rects[i % test_rects.size()], flags);
  }

  PaintOp::SerializeOptions options;

  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written =
        op->Serialize(serialized.get(), buffer_size, options);
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written =
        PaintOp::Deserialize(serialized.get(), bytes_written,
                             deserialized.get(), buffer_size, &bytes_read);
    // First two ops should succeed.  Other ops with bad enums should
    // serialize correctly but fail to deserialize due to the bad
    // SkBlendMode enum.
    if (op_idx < 2) {
      EXPECT_TRUE(written) << "op: " << op_idx;
      EXPECT_EQ(bytes_written, bytes_read);
      written->DestroyThis();
    } else {
      EXPECT_FALSE(written) << "op: " << op_idx;
    }

    ++op_idx;
  }
}

TEST(PaintOpBufferTest, ValidateRects) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  SkRect bad_rect = SkRect::MakeEmpty();
  bad_rect.fBottom = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(bad_rect.isFinite());

  // Push all op variations that take rects.
  PaintOpBuffer buffer;
  buffer.push<AnnotateOp>(PaintCanvas::AnnotationType::URL, bad_rect,
                          SkData::MakeWithCString("test1"));
  buffer.push<ClipRectOp>(bad_rect, SkClipOp::kDifference, true);

  buffer.push<DrawArcOp>(bad_rect, test_floats[0], test_floats[1], true,
                         test_flags[0]);
  buffer.push<DrawImageRectOp>(test_images[0], bad_rect, test_rects[1], nullptr,
                               PaintCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawImageRectOp>(test_images[0], test_rects[0], bad_rect, nullptr,
                               PaintCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawOvalOp>(bad_rect, test_flags[0]);
  buffer.push<DrawRectOp>(bad_rect, test_flags[0]);
  buffer.push<SaveLayerOp>(&bad_rect, nullptr);
  buffer.push<SaveLayerOp>(&bad_rect, &test_flags[0]);
  buffer.push<SaveLayerAlphaOp>(&bad_rect, test_uint8s[0], true);

  PaintOp::SerializeOptions options;

  // Every op should serialize but fail to deserialize due to the bad rect.
  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written =
        op->Serialize(serialized.get(), buffer_size, options);
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written =
        PaintOp::Deserialize(serialized.get(), bytes_written,
                             deserialized.get(), buffer_size, &bytes_read);
    EXPECT_FALSE(written) << "op: " << op_idx;
    ++op_idx;
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawArcOp) {
  PaintOpBuffer buffer;
  PushDrawArcOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawArcOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->oval.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageOp) {
  PaintOpBuffer buffer;
  PushDrawImageOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawImageOp*>(base_op);

    SkRect image_rect =
        SkRect::MakeXYWH(op->left, op->top, op->image.GetSkImage()->width(),
                         op->image.GetSkImage()->height());
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, image_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageRectOp) {
  PaintOpBuffer buffer;
  PushDrawImageRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawImageRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->dst.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawIRectOp) {
  PaintOpBuffer buffer;
  PushDrawIRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawIRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, SkRect::Make(op->rect).makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawOvalOp) {
  PaintOpBuffer buffer;
  PushDrawOvalOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawOvalOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->oval.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawPathOp) {
  PaintOpBuffer buffer;
  PushDrawPathOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawPathOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->path.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRectOp) {
  PaintOpBuffer buffer;
  PushDrawRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRRectOp) {
  PaintOpBuffer buffer;
  PushDrawRRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawRRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->rrect.rect().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawLineOp) {
  PaintOpBuffer buffer;
  PushDrawLineOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawLineOp*>(base_op);

    SkRect line_rect;
    line_rect.fLeft = op->x0;
    line_rect.fTop = op->y0;
    line_rect.fRight = op->x1;
    line_rect.fBottom = op->y1;
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, line_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawDRRectOp) {
  PaintOpBuffer buffer;
  PushDrawDRRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawDRRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->outer.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawTextBlobOp) {
  PaintOpBuffer buffer;
  PushDrawTextBlobOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawTextBlobOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->blob->bounds().makeOffset(op->x, op->y).makeSorted());
  }
}

class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  explicit MockImageProvider(bool fail_all_decodes)
      : fail_all_decodes_(fail_all_decodes) {}
  MockImageProvider(std::vector<SkSize> src_rect_offset,
                    std::vector<SkSize> scale,
                    std::vector<SkFilterQuality> quality)
      : src_rect_offset_(src_rect_offset), scale_(scale), quality_(quality) {}

  ~MockImageProvider() override = default;

  ScopedDecodedDrawImage GetDecodedDrawImage(const PaintImage& paint_image,
                                             const SkRect& src_rect,
                                             SkFilterQuality filter_quality,
                                             const SkMatrix& matrix) override {
    if (fail_all_decodes_)
      return ScopedDecodedDrawImage();

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
    size_t i = index_++;
    return ScopedDecodedDrawImage(
        DecodedDrawImage(image, src_rect_offset_[i], scale_[i], quality_[i]));
  }

 private:
  std::vector<SkSize> src_rect_offset_;
  std::vector<SkSize> scale_;
  std::vector<SkFilterQuality> quality_;
  size_t index_ = 0;
  bool fail_all_decodes_ = false;
};

TEST(PaintOpBufferTest, SkipsOpsOutsideClip) {
  // All ops with images draw outside the clip and should be skipped. If any
  // call is made to the ImageProvider, it should crash.
  MockImageProvider image_provider;
  PaintOpBuffer buffer;

  // Apply a clip outside the region for images.
  buffer.push<ClipRectOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                          SkClipOp::kIntersect, false);

  SkRect rect = SkRect::MakeXYWH(0, 0, 100, 100);
  uint8_t alpha = 220;
  buffer.push<SaveLayerAlphaOp>(&rect, alpha, false);

  PaintFlags flags;
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f, &flags);
  PaintFlags image_flags;
  image_flags.setShader(
      PaintShader::MakeImage(paint_image, SkShader::TileMode::kRepeat_TileMode,
                             SkShader::TileMode::kRepeat_TileMode, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(110, 110, 100, 100), image_flags);

  buffer.push<DrawRectOp>(rect, PaintFlags());
  buffer.push<RestoreOp>();

  // Using a strict mock to ensure that skipping image ops optimizes the
  // save/restore sequences. The single save/restore call is from the
  // PaintOpBuffer's use of SkAutoRestoreCanvas.
  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(_)).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  buffer.Playback(&canvas, &image_provider);
}

TEST(PaintOpBufferTest, SkipsOpsWithFailedDecodes) {
  MockImageProvider image_provider(true);
  PaintOpBuffer buffer;

  PaintFlags flags;
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f, &flags);
  PaintFlags image_flags;
  image_flags.setShader(
      PaintShader::MakeImage(paint_image, SkShader::TileMode::kRepeat_TileMode,
                             SkShader::TileMode::kRepeat_TileMode, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(110, 110, 100, 100), image_flags);
  buffer.push<DrawColorOp>(SK_ColorRED, SkBlendMode::kSrcOver);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(_)).InSequence(s);
  buffer.Playback(&canvas, &image_provider);
}

MATCHER(NonLazyImage, "") {
  return !arg->isLazyGenerated();
}

MATCHER_P(MatchesInvScale, expected, "") {
  SkSize scale;
  arg.decomposeScale(&scale, nullptr);
  SkSize inv = SkSize::Make(1.0f / scale.width(), 1.0f / scale.height());
  return inv == expected;
}

MATCHER_P2(MatchesRect, rect, scale, "") {
  EXPECT_EQ(arg->x(), rect.x() * scale.width());
  EXPECT_EQ(arg->y(), rect.y() * scale.height());
  EXPECT_EQ(arg->width(), rect.width() * scale.width());
  EXPECT_EQ(arg->height(), rect.height() * scale.height());
  return true;
}

MATCHER_P(MatchesQuality, quality, "") {
  return quality == arg->getFilterQuality();
}

MATCHER_P2(MatchesShader, flags, scale, "") {
  SkMatrix matrix;
  SkShader::TileMode xy[2];
  SkImage* image = arg.getShader()->isAImage(&matrix, xy);

  EXPECT_FALSE(image->isLazyGenerated());

  SkSize local_scale;
  matrix.decomposeScale(&local_scale, nullptr);
  EXPECT_EQ(local_scale.width(), 1.0f / scale.width());
  EXPECT_EQ(local_scale.height(), 1.0f / scale.height());

  EXPECT_EQ(flags.getShader()->tx(), xy[0]);
  EXPECT_EQ(flags.getShader()->ty(), xy[1]);

  return true;
};

TEST(PaintOpBufferTest, ReplacesImagesFromProvider) {
  std::vector<SkSize> src_rect_offset = {
      SkSize::MakeEmpty(), SkSize::Make(2.0f, 2.0f), SkSize::Make(3.0f, 3.0f)};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f),
                                          SkSize::Make(0.3f, 0.3f),
                                          SkSize::Make(0.4f, 0.4f)};
  std::vector<SkFilterQuality> quality = {
      kHigh_SkFilterQuality, kMedium_SkFilterQuality, kHigh_SkFilterQuality};

  MockImageProvider image_provider(src_rect_offset, scale_adjustment, quality);
  PaintOpBuffer buffer;

  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, &flags);
  buffer.push<DrawImageRectOp>(
      paint_image, rect, rect, &flags,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
  flags.setShader(
      PaintShader::MakeImage(paint_image, SkShader::TileMode::kRepeat_TileMode,
                             SkShader::TileMode::kRepeat_TileMode, nullptr));
  buffer.push<DrawOvalOp>(SkRect::MakeWH(10, 10), flags);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  // Save/scale/image/restore from DrawImageop.
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(MatchesInvScale(scale_adjustment[0])));
  EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f,
                                  MatchesQuality(quality[0])));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  // DrawImageRectop.
  SkRect src_rect =
      rect.makeOffset(src_rect_offset[1].width(), src_rect_offset[1].height());
  EXPECT_CALL(canvas,
              onDrawImageRect(
                  NonLazyImage(), MatchesRect(src_rect, scale_adjustment[1]),
                  SkRect::MakeWH(10, 10), MatchesQuality(quality[1]),
                  SkCanvas::kFast_SrcRectConstraint));

  // DrawOvalop.
  EXPECT_CALL(canvas, onDrawOval(SkRect::MakeWH(10, 10),
                                 MatchesShader(flags, scale_adjustment[2])));

  buffer.Playback(&canvas, &image_provider);
}

}  // namespace cc
