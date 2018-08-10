// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_PROTO_ENCODER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_PROTO_ENCODER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/call_stack_profile_params.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// These functions are used to encode protobufs.

// The protobuf expects the MD5 checksum prefix of the module name.
uint64_t HashModuleFilename(const base::FilePath& filename);

// Transcode |sample| into |proto_sample|, using base addresses in |modules| to
// compute module instruction pointer offsets.
void CopySampleToProto(
    const base::StackSamplingProfiler::Sample& sample,
    const std::vector<base::StackSamplingProfiler::Module>& modules,
    CallStackProfile::Sample* proto_sample);

// Transcode Sample annotations into protobuf fields. The C++ code uses a bit-
// field with each bit corresponding to an entry in an enumeration while the
// protobuf uses a repeated field of individual values. Conversion tables
// allow for arbitrary mapping, though no more than 32 in any given version
// of the code.
void CopyAnnotationsToProto(uint32_t new_milestones,
                            CallStackProfile::Sample* sample_proto);

// Transcode |profile| into |proto_profile|.
void CopyProfileToProto(
    const base::StackSamplingProfiler::CallStackProfile& profile,
    CallStackProfile* proto_profile);

// Translates CallStackProfileParams's process to the corresponding
// execution context Process.
Process ToExecutionContextProcess(CallStackProfileParams::Process process);

// Translates CallStackProfileParams's thread to the corresponding
// SampledProfile Thread.
Thread ToExecutionContextThread(CallStackProfileParams::Thread thread);

// Translates CallStackProfileParams's trigger to the corresponding
// SampledProfile TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    CallStackProfileParams::Trigger trigger);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_PROTO_ENCODER_H_
