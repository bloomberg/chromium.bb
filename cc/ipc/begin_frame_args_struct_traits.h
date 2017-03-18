// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_BEGIN_FRAME_ARGS_STRUCT_TRAITS_H_
#define CC_IPC_BEGIN_FRAME_ARGS_STRUCT_TRAITS_H_

#include "cc/ipc/begin_frame_args.mojom-shared.h"
#include "cc/output/begin_frame_args.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::BeginFrameArgsDataView, cc::BeginFrameArgs> {
  static base::TimeTicks frame_time(const cc::BeginFrameArgs& args) {
    return args.frame_time;
  }

  static base::TimeTicks deadline(const cc::BeginFrameArgs& args) {
    return args.deadline;
  }

  static base::TimeDelta interval(const cc::BeginFrameArgs& args) {
    return args.interval;
  }

  static uint64_t sequence_number(const cc::BeginFrameArgs& args) {
    return args.sequence_number;
  }

  static uint32_t source_id(const cc::BeginFrameArgs& args) {
    return args.source_id;
  }

  static cc::mojom::BeginFrameArgsType type(const cc::BeginFrameArgs& args) {
    return static_cast<cc::mojom::BeginFrameArgsType>(args.type);
  }

  static bool on_critical_path(const cc::BeginFrameArgs& args) {
    return args.on_critical_path;
  }

  static bool Read(cc::mojom::BeginFrameArgsDataView data,
                   cc::BeginFrameArgs* out);
};

template <>
struct StructTraits<cc::mojom::BeginFrameAckDataView, cc::BeginFrameAck> {
  static uint64_t sequence_number(const cc::BeginFrameAck& ack) {
    return ack.sequence_number;
  }

  static uint64_t latest_confirmed_sequence_number(
      const cc::BeginFrameAck& ack) {
    return ack.latest_confirmed_sequence_number;
  }

  static uint32_t source_id(const cc::BeginFrameAck& ack) {
    return ack.source_id;
  }

  static bool Read(cc::mojom::BeginFrameAckDataView data,
                   cc::BeginFrameAck* out);
};

}  // namespace mojo

#endif  // CC_IPC_BEGIN_FRAME_ARGS_STRUCT_TRAITS_H_
