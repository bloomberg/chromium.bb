// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the transformation and adjustment for Windows X86 executables.
// The same code can be used for Windows X64 executables.

#ifndef COURGETTE_WIN32_X86_GENERATOR_H_
#define COURGETTE_WIN32_X86_GENERATOR_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

#include "courgette/assembly_program.h"
#include "courgette/ensemble.h"

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
    AssemblyProgram* old_program = NULL;
    Status old_parse_status =
        ParseDetectedExecutable(old_element_->region().start(),
                                old_element_->region().length(),
                                &old_program);
    if (old_parse_status != C_OK) {
      LOG(ERROR) << "Cannot parse as WinPE " << old_element_->Name();
      return old_parse_status;
    }

    AssemblyProgram* new_program = NULL;
    Status new_parse_status =
        ParseDetectedExecutable(new_element_->region().start(),
                                new_element_->region().length(),
                                &new_program);
    if (new_parse_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      LOG(ERROR) << "Cannot parse as WinPE " << new_element_->Name();
      return new_parse_status;
    }

    // Trim labels below a certain threshold
    Status trim_old_status = TrimLabels(old_program);
    if (trim_old_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return trim_old_status;
    }

    Status trim_new_status = TrimLabels(new_program);
    if (trim_new_status != C_OK) {
      DeleteAssemblyProgram(new_program);
      return trim_new_status;
    }

    EncodedProgram* old_encoded = NULL;
    Status old_encode_status = Encode(old_program, &old_encoded);
    if (old_encode_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return old_encode_status;
    }

    Status old_write_status =
        WriteEncodedProgram(old_encoded, old_transformed_element);
    DeleteEncodedProgram(old_encoded);
    if (old_write_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return old_write_status;
    }

    Status adjust_status = Adjust(*old_program, new_program);
    DeleteAssemblyProgram(old_program);
    if (adjust_status != C_OK) {
      DeleteAssemblyProgram(new_program);
      return adjust_status;
    }

    EncodedProgram* new_encoded = NULL;
    Status new_encode_status = Encode(new_program, &new_encoded);
    DeleteAssemblyProgram(new_program);
    if (new_encode_status != C_OK)
      return new_encode_status;

    Status new_write_status =
        WriteEncodedProgram(new_encoded, new_transformed_element);
    DeleteEncodedProgram(new_encoded);
    if (new_write_status != C_OK)
      return new_write_status;

    return C_OK;
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
