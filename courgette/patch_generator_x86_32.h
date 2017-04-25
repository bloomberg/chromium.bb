// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PATCH_GENERATOR_X86_32_H_
#define COURGETTE_PATCH_GENERATOR_X86_32_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette_flow.h"
#include "courgette/ensemble.h"
#include "courgette/patcher_x86_32.h"
#include "courgette/program_detector.h"

namespace courgette {

// PatchGeneratorX86_32 is the universal patch generator for all executables,
// performing transformation and adjustment. The executable type is determined
// by the program detector.
class PatchGeneratorX86_32 : public TransformationPatchGenerator {
 public:
  PatchGeneratorX86_32(Element* old_element,
                       Element* new_element,
                       PatcherX86_32* patcher,
                       ExecutableType kind)
      : TransformationPatchGenerator(old_element, new_element, patcher),
        kind_(kind) {
  }

  virtual ExecutableType Kind() { return kind_; }

  Status WriteInitialParameters(SinkStream* parameter_stream) {
    if (!parameter_stream->WriteSizeVarint32(
            old_element_->offset_in_ensemble()) ||
        !parameter_stream->WriteSizeVarint32(old_element_->region().length())) {
      return C_STREAM_ERROR;
    }
    return C_OK;
    // TODO(sra): Initialize |patcher_| with these parameters.
  }

  Status PredictTransformParameters(SinkStreamSet* prediction) {
    return TransformationPatchGenerator::PredictTransformParameters(prediction);
  }

  Status CorrectedTransformParameters(SinkStreamSet* parameters) {
    // No code needed to write an 'empty' parameter set.
    return C_OK;
  }

  // The format of a transformed_element is a serialized EncodedProgram.  We
  // first disassemble the original old and new Elements into AssemblyPrograms.
  // Then we adjust the new AssemblyProgram to make it as much like the old one
  // as possible, before converting the AssemblyPrograms to EncodedPrograms and
  // serializing them.
  Status Transform(SourceStreamSet* corrected_parameters,
                   SinkStreamSet* old_transformed_element,
                   SinkStreamSet* new_transformed_element) {
    // Don't expect any corrected parameters.
    if (!corrected_parameters->Empty())
      return C_GENERAL_ERROR;

    CourgetteFlow flow;
    RegionBuffer old_buffer(old_element_->region());
    RegionBuffer new_buffer(new_element_->region());
    flow.ReadAssemblyProgramFromBuffer(flow.OLD, old_buffer, true);
    flow.CreateEncodedProgramFromAssemblyProgram(flow.OLD);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.OLD,
                                              old_transformed_element);
    flow.DestroyEncodedProgram(flow.OLD);
    flow.ReadAssemblyProgramFromBuffer(flow.NEW, new_buffer, true);
    flow.AdjustNewAssemblyProgramToMatchOld();
    flow.DestroyAssemblyProgram(flow.OLD);
    flow.CreateEncodedProgramFromAssemblyProgram(flow.NEW);
    flow.DestroyAssemblyProgram(flow.NEW);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.NEW,
                                              new_transformed_element);
    if (flow.failed()) {
      LOG(ERROR) << flow.message() << " (" << old_element_->Name() << " => "
                 << new_element_->Name() << ")";
    }
    return flow.status();
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    return TransformationPatchGenerator::Reform(transformed_element,
                                                reformed_element);
  }

 private:
  virtual ~PatchGeneratorX86_32() { }

  ExecutableType kind_;

  DISALLOW_COPY_AND_ASSIGN(PatchGeneratorX86_32);
};

}  // namespace courgette

#endif  // COURGETTE_PATCH_GENERATOR_X86_32_H_
