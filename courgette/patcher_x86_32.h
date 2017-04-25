// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PATCHER_X86_32_H_
#define COURGETTE_PATCHER_X86_32_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette_flow.h"
#include "courgette/encoded_program.h"
#include "courgette/ensemble.h"
#include "courgette/program_detector.h"

namespace courgette {

// PatcherX86_32 is the universal patcher for all executables. The executable
// type is determined by the program detector.
class PatcherX86_32 : public TransformationPatcher {
 public:
  explicit PatcherX86_32(const Region& region)
      : ensemble_region_(region),
        base_offset_(0),
        base_length_(0) {
  }

  Status Init(SourceStream* parameter_stream) {
    if (!parameter_stream->ReadVarint32(&base_offset_))
      return C_BAD_TRANSFORM;
    if (!parameter_stream->ReadVarint32(&base_length_))
      return C_BAD_TRANSFORM;

    if (base_offset_ > ensemble_region_.length())
      return C_BAD_TRANSFORM;
    if (base_length_ > ensemble_region_.length() - base_offset_)
      return C_BAD_TRANSFORM;

    return C_OK;
  }

  Status PredictTransformParameters(SinkStreamSet* predicted_parameters) {
    // No code needed to write an 'empty' predicted parameter set.
    return C_OK;
  }

  Status Transform(SourceStreamSet* corrected_parameters,
                   SinkStreamSet* transformed_element) {
    CourgetteFlow flow;
    RegionBuffer only_buffer(
        Region(ensemble_region_.start() + base_offset_, base_length_));
    flow.ReadAssemblyProgramFromBuffer(flow.ONLY, only_buffer, false);
    flow.CreateEncodedProgramFromAssemblyProgram(flow.ONLY);
    flow.DestroyAssemblyProgram(flow.ONLY);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY, transformed_element);
    if (flow.failed())
      LOG(ERROR) << flow.message();
    return flow.status();
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    CourgetteFlow flow;
    flow.ReadEncodedProgramFromSourceStreamSet(flow.ONLY, transformed_element);
    flow.WriteExecutableFromEncodedProgram(flow.ONLY, reformed_element);
    if (flow.failed())
      LOG(ERROR) << flow.message();
    return flow.status();
  }

 private:
  Region ensemble_region_;

  uint32_t base_offset_;
  uint32_t base_length_;

  DISALLOW_COPY_AND_ASSIGN(PatcherX86_32);
};

}  // namespace courgette

#endif  // COURGETTE_PATCHER_X86_32_H_
