// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/conversion_utils.h"

#include "base/memory/ptr_util.h"

namespace extensions {
namespace api {
namespace media_perception_private {

namespace {

std::unique_ptr<Point> PointProtoToIdl(const mri::Point& point) {
  std::unique_ptr<Point> point_result = std::make_unique<Point>();
  if (point.has_x())
    point_result->x = std::make_unique<double>(point.x());

  if (point.has_y())
    point_result->y = std::make_unique<double>(point.y());

  return point_result;
}

std::unique_ptr<BoundingBox> BoundingBoxProtoToIdl(
    const mri::BoundingBox& bounding_box) {
  std::unique_ptr<BoundingBox> bounding_box_result =
      std::make_unique<BoundingBox>();
  if (bounding_box.has_normalized()) {
    bounding_box_result->normalized =
        std::make_unique<bool>(bounding_box.normalized());
  }

  if (bounding_box.has_top_left())
    bounding_box_result->top_left = PointProtoToIdl(bounding_box.top_left());

  if (bounding_box.has_bottom_right()) {
    bounding_box_result->bottom_right =
        PointProtoToIdl(bounding_box.bottom_right());
  }

  return bounding_box_result;
}

DistanceUnits DistanceUnitsProtoToIdl(const mri::Distance& distance) {
  if (distance.has_units()) {
    switch (distance.units()) {
      case mri::Distance::METERS:
        return DISTANCE_UNITS_METERS;
      case mri::Distance::PIXELS:
        return DISTANCE_UNITS_PIXELS;
      case mri::Distance::UNITS_UNSPECIFIED:
        return DISTANCE_UNITS_UNSPECIFIED;
    }
    NOTREACHED() << "Unknown distance units: " << distance.units();
  }
  return DISTANCE_UNITS_UNSPECIFIED;
}

std::unique_ptr<Distance> DistanceProtoToIdl(const mri::Distance& distance) {
  std::unique_ptr<Distance> distance_result = std::make_unique<Distance>();
  distance_result->units = DistanceUnitsProtoToIdl(distance);

  if (distance.has_magnitude())
    distance_result->magnitude = std::make_unique<double>(distance.magnitude());

  return distance_result;
}

EntityType EntityTypeProtoToIdl(const mri::Entity& entity) {
  if (entity.has_type()) {
    switch (entity.type()) {
      case mri::Entity::FACE:
        return ENTITY_TYPE_FACE;
      case mri::Entity::PERSON:
        return ENTITY_TYPE_PERSON;
      case mri::Entity::MOTION_REGION:
        return ENTITY_TYPE_MOTION_REGION;
      case mri::Entity::UNSPECIFIED:
        return ENTITY_TYPE_UNSPECIFIED;
    }
    NOTREACHED() << "Unknown entity type: " << entity.type();
  }
  return ENTITY_TYPE_UNSPECIFIED;
}

Entity EntityProtoToIdl(const mri::Entity& entity) {
  Entity entity_result;
  if (entity.has_id())
    entity_result.id = std::make_unique<int>(entity.id());

  entity_result.type = EntityTypeProtoToIdl(entity);
  if (entity.has_confidence())
    entity_result.confidence = std::make_unique<double>(entity.confidence());

  if (entity.has_bounding_box())
    entity_result.bounding_box = BoundingBoxProtoToIdl(entity.bounding_box());

  if (entity.has_depth())
    entity_result.depth = DistanceProtoToIdl(entity.depth());

  return entity_result;
}

FramePerception FramePerceptionProtoToIdl(
    const mri::FramePerception& frame_perception) {
  FramePerception frame_perception_result;
  if (frame_perception.has_frame_id()) {
    frame_perception_result.frame_id =
        std::make_unique<int>(frame_perception.frame_id());
  }
  if (frame_perception.has_frame_width_in_px()) {
    frame_perception_result.frame_width_in_px =
        std::make_unique<int>(frame_perception.frame_width_in_px());
  }
  if (frame_perception.has_frame_height_in_px()) {
    frame_perception_result.frame_height_in_px =
        std::make_unique<int>(frame_perception.frame_height_in_px());
  }
  if (frame_perception.has_timestamp()) {
    frame_perception_result.timestamp =
        std::make_unique<double>(frame_perception.timestamp());
  }
  if (frame_perception.entity_size() > 0) {
    frame_perception_result.entities = std::make_unique<std::vector<Entity>>();
    for (const auto& entity : frame_perception.entity())
      frame_perception_result.entities->emplace_back(EntityProtoToIdl(entity));
  }
  return frame_perception_result;
}

ImageFormat ImageFormatProtoToIdl(const mri::ImageFrame& image_frame) {
  if (image_frame.has_format()) {
    switch (image_frame.format()) {
      case mri::ImageFrame::RGB:
        return IMAGE_FORMAT_RAW;
      case mri::ImageFrame::PNG:
        return IMAGE_FORMAT_PNG;
      case mri::ImageFrame::JPEG:
        return IMAGE_FORMAT_JPEG;
      case mri::ImageFrame::FORMAT_UNSPECIFIED:
        return IMAGE_FORMAT_NONE;
    }
    NOTREACHED() << "Unknown image format: " << image_frame.format();
  }
  return IMAGE_FORMAT_NONE;
}

ImageFrame ImageFrameProtoToIdl(const mri::ImageFrame& image_frame) {
  ImageFrame image_frame_result;
  if (image_frame.has_width())
    image_frame_result.width = std::make_unique<int>(image_frame.width());

  if (image_frame.has_height())
    image_frame_result.height = std::make_unique<int>(image_frame.height());

  if (image_frame.has_data_length()) {
    image_frame_result.data_length =
        std::make_unique<int>(image_frame.data_length());
  }

  if (image_frame.has_pixel_data()) {
    image_frame_result.frame = std::make_unique<std::vector<char>>(
        image_frame.pixel_data().begin(), image_frame.pixel_data().end());
  }

  image_frame_result.format = ImageFormatProtoToIdl(image_frame);
  return image_frame_result;
}

PerceptionSample PerceptionSampleProtoToIdl(
    const mri::PerceptionSample& perception_sample) {
  PerceptionSample perception_sample_result;
  if (perception_sample.has_frame_perception()) {
    perception_sample_result.frame_perception =
        std::make_unique<FramePerception>(
            FramePerceptionProtoToIdl(perception_sample.frame_perception()));
  }
  if (perception_sample.has_image_frame()) {
    perception_sample_result.image_frame = std::make_unique<ImageFrame>(
        ImageFrameProtoToIdl(perception_sample.image_frame()));
  }
  return perception_sample_result;
}

Status StateStatusProtoToIdl(const mri::State& state) {
  switch (state.status()) {
    case mri::State::UNINITIALIZED:
      return STATUS_UNINITIALIZED;
    case mri::State::STARTED:
      return STATUS_STARTED;
    case mri::State::RUNNING:
      return STATUS_RUNNING;
    case mri::State::SUSPENDED:
      return STATUS_SUSPENDED;
    case mri::State::RESTARTING:
      return STATUS_RESTARTING;
    case mri::State::STATUS_UNSPECIFIED:
      return STATUS_NONE;
  }
  NOTREACHED() << "Reached status not in switch.";
  return STATUS_NONE;
}

mri::State::Status StateStatusIdlToProto(const State& state) {
  switch (state.status) {
    case STATUS_UNINITIALIZED:
      return mri::State::UNINITIALIZED;
    case STATUS_STARTED:
      return mri::State::STARTED;
    case STATUS_RUNNING:
      return mri::State::RUNNING;
    case STATUS_SUSPENDED:
      return mri::State::SUSPENDED;
    case STATUS_RESTARTING:
      return mri::State::RESTARTING;
    case STATUS_SERVICE_ERROR:
    case STATUS_STOPPED:  // Process is stopped by MPP.
    case STATUS_NONE:
      return mri::State::STATUS_UNSPECIFIED;
  }
  NOTREACHED() << "Reached status not in switch.";
  return mri::State::STATUS_UNSPECIFIED;
}

}  //  namespace

State StateProtoToIdl(const mri::State& state) {
  State state_result;
  if (state.has_status()) {
    state_result.status = StateStatusProtoToIdl(state);
  }
  if (state.has_device_context()) {
    state_result.device_context =
        std::make_unique<std::string>(state.device_context());
  }
  return state_result;
}

mri::State StateIdlToProto(const State& state) {
  mri::State state_result;
  state_result.set_status(StateStatusIdlToProto(state));
  if (state.device_context)
    state_result.set_device_context(*state.device_context);

  return state_result;
}

MediaPerception MediaPerceptionProtoToIdl(
    const mri::MediaPerception& media_perception) {
  MediaPerception media_perception_result;
  if (media_perception.has_timestamp()) {
    media_perception_result.timestamp =
        std::make_unique<double>(media_perception.timestamp());
  }

  if (media_perception.frame_perception_size() > 0) {
    media_perception_result.frame_perceptions =
        std::make_unique<std::vector<FramePerception>>();
    for (const auto& frame_perception : media_perception.frame_perception()) {
      media_perception_result.frame_perceptions->emplace_back(
          FramePerceptionProtoToIdl(frame_perception));
    }
  }
  return media_perception_result;
}

Diagnostics DiagnosticsProtoToIdl(const mri::Diagnostics& diagnostics) {
  Diagnostics diagnostics_result;
  if (diagnostics.perception_sample_size() > 0) {
    diagnostics_result.perception_samples =
        std::make_unique<std::vector<PerceptionSample>>();
    for (const auto& perception_sample : diagnostics.perception_sample()) {
      diagnostics_result.perception_samples->emplace_back(
          PerceptionSampleProtoToIdl(perception_sample));
    }
  }
  return diagnostics_result;
}

}  // namespace media_perception_private
}  // namespace api
}  // namespace extensions
