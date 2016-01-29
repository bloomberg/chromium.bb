// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the transformation and adjustment for all executables.
// The executable type is determined by ParseDetectedExecutable function.

#ifndef COURGETTE_WIN32_X86_GENERATOR_H_
#define COURGETTE_WIN32_X86_GENERATOR_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "courgette/assembly_program.h"
#include "courgette/ensemble.h"
#include "courgette/program_detector.h"

namespace courgette {

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

    // Generate old version of program using |corrected_parameters|.
    // TODO(sra): refactor to use same code from patcher_.
    scoped_ptr<AssemblyProgram> old_program;
    Status old_parse_status =
        ParseDetectedExecutable(old_element_->region().start(),
                                old_element_->region().length(),
                                &old_program);
    if (old_parse_status != C_OK) {
      LOG(ERROR) << "Cannot parse an executable " << old_element_->Name();
      return old_parse_status;
    }

    // TODO(huangs): Move the block below to right before |new_program| gets
    // used, so we can reduce Courgette-gen peak memory.
    scoped_ptr<AssemblyProgram> new_program;
    Status new_parse_status =
        ParseDetectedExecutable(new_element_->region().start(),
                                new_element_->region().length(),
                                &new_program);
    if (new_parse_status != C_OK) {
      LOG(ERROR) << "Cannot parse an executable " << new_element_->Name();
      return new_parse_status;
    }

    scoped_ptr<EncodedProgram> old_encoded;
    Status old_encode_status = Encode(*old_program, &old_encoded);
    if (old_encode_status != C_OK)
      return old_encode_status;

    Status old_write_status =
        WriteEncodedProgram(old_encoded.get(), old_transformed_element);

    old_encoded.reset();

    if (old_write_status != C_OK)
      return old_write_status;

    Status adjust_status = Adjust(*old_program, new_program.get());
    old_program.reset();
    if (adjust_status != C_OK)
      return adjust_status;

    scoped_ptr<EncodedProgram> new_encoded;
    Status new_encode_status = Encode(*new_program, &new_encoded);
    if (new_encode_status != C_OK)
      return new_encode_status;

    new_program.reset();

    Status new_write_status =
        WriteEncodedProgram(new_encoded.get(), new_transformed_element);
    return new_write_status;
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

#endif  // COURGETTE_WIN32_X86_GENERATOR_H_
