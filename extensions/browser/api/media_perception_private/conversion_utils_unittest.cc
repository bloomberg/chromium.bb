// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/conversion_utils.h"

#include "base/memory/ptr_util.h"
#include "chromeos/media_perception/media_perception.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

const char kTestDeviceContext[] = "Video camera";

void InitializeFakeFramePerception(const int index,
                                   mri::FramePerception* frame_perception) {
  frame_perception->set_frame_id(index);
  frame_perception->set_frame_width_in_px(3);
  frame_perception->set_frame_height_in_px(4);
  frame_perception->set_timestamp(5);

  // Add a couple fake entities to the frame perception. Note: PERSON
  // EntityType is currently unused.
  mri::Entity* entity_one = frame_perception->add_entity();
  entity_one->set_id(6);
  entity_one->set_type(mri::Entity::FACE);
  entity_one->set_confidence(7);

  mri::Distance* distance = entity_one->mutable_depth();
  distance->set_units(mri::Distance::METERS);
  distance->set_magnitude(7.5);

  mri::Entity* entity_two = frame_perception->add_entity();
  entity_two->set_id(8);
  entity_two->set_type(mri::Entity::MOTION_REGION);
  entity_two->set_confidence(9);

  mri::BoundingBox* bounding_box_one = entity_one->mutable_bounding_box();
  bounding_box_one->mutable_top_left()->set_x(10);
  bounding_box_one->mutable_top_left()->set_y(11);
  bounding_box_one->mutable_bottom_right()->set_x(12);
  bounding_box_one->mutable_bottom_right()->set_y(13);
  bounding_box_one->set_normalized(false);

  mri::BoundingBox* bounding_box_two = entity_two->mutable_bounding_box();
  bounding_box_two->mutable_top_left()->set_x(14);
  bounding_box_two->mutable_top_left()->set_y(15);
  bounding_box_two->set_normalized(true);
}

void ValidateFramePerceptionResult(
    const int index,
    const media_perception::FramePerception& frame_perception_result) {
  ASSERT_TRUE(frame_perception_result.frame_id);
  EXPECT_EQ(*frame_perception_result.frame_id, index);
  ASSERT_TRUE(frame_perception_result.frame_width_in_px);
  EXPECT_EQ(*frame_perception_result.frame_width_in_px, 3);
  ASSERT_TRUE(frame_perception_result.frame_height_in_px);
  EXPECT_EQ(*frame_perception_result.frame_height_in_px, 4);
  ASSERT_TRUE(frame_perception_result.timestamp);
  EXPECT_EQ(*frame_perception_result.timestamp, 5);

  ASSERT_EQ(2u, frame_perception_result.entities->size());
  const media_perception::Entity& entity_result_one =
      frame_perception_result.entities->at(0);
  ASSERT_TRUE(entity_result_one.id);
  EXPECT_EQ(*entity_result_one.id, 6);
  ASSERT_TRUE(entity_result_one.confidence);
  EXPECT_EQ(*entity_result_one.confidence, 7);
  EXPECT_EQ(entity_result_one.type, media_perception::ENTITY_TYPE_FACE);

  const media_perception::Distance* distance = entity_result_one.depth.get();
  ASSERT_TRUE(distance);
  EXPECT_EQ(media_perception::DISTANCE_UNITS_METERS, distance->units);
  ASSERT_TRUE(distance->magnitude);
  EXPECT_EQ(7.5f, *distance->magnitude);

  const media_perception::Entity& entity_result_two =
      frame_perception_result.entities->at(1);
  ASSERT_TRUE(entity_result_two.id);
  EXPECT_EQ(*entity_result_two.id, 8);
  ASSERT_TRUE(entity_result_two.confidence);
  EXPECT_EQ(*entity_result_two.confidence, 9);
  EXPECT_EQ(entity_result_two.type,
            media_perception::ENTITY_TYPE_MOTION_REGION);

  const media_perception::BoundingBox* bounding_box_result_one =
      entity_result_one.bounding_box.get();
  ASSERT_TRUE(bounding_box_result_one);
  ASSERT_TRUE(bounding_box_result_one->top_left);
  ASSERT_TRUE(bounding_box_result_one->top_left->x);
  EXPECT_EQ(*bounding_box_result_one->top_left->x, 10);
  ASSERT_TRUE(bounding_box_result_one->top_left->y);
  EXPECT_EQ(*bounding_box_result_one->top_left->y, 11);
  ASSERT_TRUE(bounding_box_result_one->bottom_right);
  ASSERT_TRUE(bounding_box_result_one->bottom_right->x);
  EXPECT_EQ(*bounding_box_result_one->bottom_right->x, 12);
  ASSERT_TRUE(bounding_box_result_one->bottom_right->y);
  EXPECT_EQ(*bounding_box_result_one->bottom_right->y, 13);
  EXPECT_FALSE(*bounding_box_result_one->normalized);

  const media_perception::BoundingBox* bounding_box_result_two =
      entity_result_two.bounding_box.get();
  ASSERT_TRUE(bounding_box_result_two);
  ASSERT_TRUE(bounding_box_result_two->top_left);
  EXPECT_EQ(*bounding_box_result_two->top_left->x, 14);
  EXPECT_EQ(*bounding_box_result_two->top_left->y, 15);
  EXPECT_FALSE(bounding_box_result_two->bottom_right);
  EXPECT_TRUE(*bounding_box_result_two->normalized);
}

void InitializeFakeImageFrameData(mri::ImageFrame* image_frame) {
  image_frame->set_width(1);
  image_frame->set_height(2);
  image_frame->set_data_length(3);
  image_frame->set_pixel_data("    ");
  image_frame->set_format(mri::ImageFrame::JPEG);
}

void ValidateFakeImageFrameData(
    const media_perception::ImageFrame& image_frame_result) {
  ASSERT_TRUE(image_frame_result.width);
  EXPECT_EQ(*image_frame_result.width, 1);
  ASSERT_TRUE(image_frame_result.height);
  EXPECT_EQ(*image_frame_result.height, 2);
  ASSERT_TRUE(image_frame_result.data_length);
  EXPECT_EQ(*image_frame_result.data_length, 3);
  ASSERT_TRUE(image_frame_result.frame);
  EXPECT_EQ((*image_frame_result.frame).size(), 4ul);
  EXPECT_EQ(image_frame_result.format, media_perception::IMAGE_FORMAT_JPEG);
}

}  // namespace

// Verifies that the data is converted successfully and as expected in each of
// these cases.

TEST(MediaPerceptionConversionUtilsTest, MediaPerceptionProtoToIdl) {
  const int kFrameId = 2;
  mri::MediaPerception media_perception;
  // Fill in fake values for the media_perception proto.
  media_perception.set_timestamp(1);
  mri::FramePerception* frame_perception =
      media_perception.add_frame_perception();
  InitializeFakeFramePerception(kFrameId, frame_perception);
  media_perception::MediaPerception media_perception_result =
      media_perception::MediaPerceptionProtoToIdl(media_perception);
  EXPECT_EQ(*media_perception_result.timestamp, 1);
  ASSERT_TRUE(media_perception_result.frame_perceptions);
  ASSERT_EQ(1u, media_perception_result.frame_perceptions->size());
  ValidateFramePerceptionResult(
      kFrameId, media_perception_result.frame_perceptions->at(0));
}

TEST(MediaPerceptionConversionUtilsTest, DiagnosticsProtoToIdl) {
  const size_t kNumSamples = 3;
  mri::Diagnostics diagnostics;
  for (size_t i = 0; i < kNumSamples; i++) {
    mri::PerceptionSample* perception_sample =
        diagnostics.add_perception_sample();
    mri::FramePerception* frame_perception =
        perception_sample->mutable_frame_perception();
    InitializeFakeFramePerception(i, frame_perception);
    mri::ImageFrame* image_frame = perception_sample->mutable_image_frame();
    InitializeFakeImageFrameData(image_frame);
  }
  media_perception::Diagnostics diagnostics_result =
      media_perception::DiagnosticsProtoToIdl(diagnostics);
  ASSERT_EQ(kNumSamples, diagnostics_result.perception_samples->size());
  for (size_t i = 0; i < kNumSamples; i++) {
    SCOPED_TRACE(testing::Message() << "Sample number: " << i);
    const media_perception::PerceptionSample& perception_sample_result =
        diagnostics_result.perception_samples->at(i);

    const media_perception::FramePerception* frame_perception_result =
        perception_sample_result.frame_perception.get();
    ASSERT_TRUE(frame_perception_result);

    const media_perception::ImageFrame* image_frame_result =
        perception_sample_result.image_frame.get();
    ASSERT_TRUE(image_frame_result);

    ValidateFramePerceptionResult(i, *frame_perception_result);
    ValidateFakeImageFrameData(*image_frame_result);
  }
}

TEST(MediaPerceptionConversionUtilsTest, StateProtoToIdl) {
  mri::State state;
  state.set_status(mri::State::RUNNING);
  media_perception::State state_result =
      media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::STATUS_RUNNING);

  state.set_status(mri::State::STARTED);
  state.set_device_context(kTestDeviceContext);
  state_result = media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::STATUS_STARTED);
  ASSERT_TRUE(state_result.device_context);
  EXPECT_EQ(*state_result.device_context, kTestDeviceContext);

  state.set_status(mri::State::RESTARTING);
  state_result = media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::STATUS_RESTARTING);
}

TEST(MediaPerceptionConversionUtilsTest, StateIdlToProto) {
  media_perception::State state;
  state.status = media_perception::STATUS_UNINITIALIZED;
  mri::State state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::UNINITIALIZED);
  EXPECT_FALSE(state_proto.has_device_context());

  state.status = media_perception::STATUS_SUSPENDED;
  state.device_context = std::make_unique<std::string>(kTestDeviceContext);
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::SUSPENDED);
  EXPECT_EQ(state_proto.device_context(), kTestDeviceContext);

  state.status = media_perception::STATUS_RESTARTING;
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::RESTARTING);

  state.status = media_perception::STATUS_STOPPED;
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(mri::State::STATUS_UNSPECIFIED, state_proto.status());
}

}  // namespace extensions
