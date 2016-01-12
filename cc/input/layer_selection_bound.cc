// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_selection_bound.pb.h"

namespace cc {
namespace {

proto::LayerSelectionBound::SelectionBoundType SelectionBoundTypeToProtobuf(
    const SelectionBoundType& type) {
  switch (type) {
    case SELECTION_BOUND_LEFT:
      return proto::LayerSelectionBound_SelectionBoundType_LEFT;
    case SELECTION_BOUND_RIGHT:
      return proto::LayerSelectionBound_SelectionBoundType_RIGHT;
    case SELECTION_BOUND_CENTER:
      return proto::LayerSelectionBound_SelectionBoundType_CENTER;
    case SELECTION_BOUND_EMPTY:
      return proto::LayerSelectionBound_SelectionBoundType_EMPTY;
  }
  NOTREACHED() << "proto::LayerSelectionBound_SelectionBoundType_UNKNOWN";
  return proto::LayerSelectionBound_SelectionBoundType_UNKNOWN;
}

SelectionBoundType SelectionBoundTypeFromProtobuf(
    const proto::LayerSelectionBound::SelectionBoundType& type) {
  switch (type) {
    case proto::LayerSelectionBound_SelectionBoundType_LEFT:
      return SELECTION_BOUND_LEFT;
    case proto::LayerSelectionBound_SelectionBoundType_RIGHT:
      return SELECTION_BOUND_RIGHT;
    case proto::LayerSelectionBound_SelectionBoundType_CENTER:
      return SELECTION_BOUND_CENTER;
    case proto::LayerSelectionBound_SelectionBoundType_EMPTY:
      return SELECTION_BOUND_EMPTY;
    case proto::LayerSelectionBound_SelectionBoundType_UNKNOWN:
      NOTREACHED() << "proto::LayerSelectionBound_SelectionBoundType_UNKNOWN";
      return SELECTION_BOUND_EMPTY;
  }
  return SELECTION_BOUND_EMPTY;
}

}  // namespace

LayerSelectionBound::LayerSelectionBound()
    : type(SELECTION_BOUND_EMPTY), layer_id(0) {
}

LayerSelectionBound::~LayerSelectionBound() {
}

bool LayerSelectionBound::operator==(const LayerSelectionBound& other) const {
  return type == other.type && layer_id == other.layer_id &&
         edge_top == other.edge_top && edge_bottom == other.edge_bottom;
}

bool LayerSelectionBound::operator!=(const LayerSelectionBound& other) const {
  return !(*this == other);
}

void LayerSelectionBound::ToProtobuf(proto::LayerSelectionBound* proto) const {
  proto->set_type(SelectionBoundTypeToProtobuf(type));
  PointToProto(edge_top, proto->mutable_edge_top());
  PointToProto(edge_bottom, proto->mutable_edge_bottom());
  proto->set_layer_id(layer_id);
}

void LayerSelectionBound::FromProtobuf(
    const proto::LayerSelectionBound& proto) {
  type = SelectionBoundTypeFromProtobuf(proto.type());
  edge_top = ProtoToPoint(proto.edge_top());
  edge_bottom = ProtoToPoint(proto.edge_bottom());
  layer_id = proto.layer_id();
}

void LayerSelectionToProtobuf(const LayerSelection& selection,
                              proto::LayerSelection* proto) {
  selection.start.ToProtobuf(proto->mutable_start());
  selection.end.ToProtobuf(proto->mutable_end());
  proto->set_is_editable(selection.is_editable);
  proto->set_is_empty_text_form_control(selection.is_empty_text_form_control);
}

void LayerSelectionFromProtobuf(LayerSelection* selection,
                                const proto::LayerSelection& proto) {
  selection->start.FromProtobuf(proto.start());
  selection->end.FromProtobuf(proto.end());
  selection->is_editable = proto.is_editable();
  selection->is_empty_text_form_control = proto.is_empty_text_form_control();
}

}  // namespace cc
