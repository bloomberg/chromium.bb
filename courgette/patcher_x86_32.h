// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the transformation for Windows X86 executables.
// The same patcher can be used for Windows X64 executables.

#ifndef COURGETTE_WIN32_X86_PATCHER_H_
#define COURGETTE_WIN32_X86_PATCHER_H_

#include "courgette/ensemble.h"

namespace courgette {

// Courgette32X86Patcher is a TransformationPatcher for Windows 32-bit
// and 64-bit executables.  We can use the same patcher for both.
//
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
    Status status;
    if (!corrected_parameters->Empty())
      return C_GENERAL_ERROR;   // Don't expect any corrected parameters.

    AssemblyProgram* program = NULL;
    status = ParseDetectedExecutable(ensemble_region_.start() + base_offset_,
                                     base_length_,
                                     &program);
    if (status != C_OK)
      return status;

    // Trim labels below a certain threshold
    Status trim_status = TrimLabels(program);
    if (trim_status != C_OK) {
      DeleteAssemblyProgram(program);
      return trim_status;
    }

    EncodedProgram* encoded = NULL;
    status = Encode(program, &encoded);
    DeleteAssemblyProgram(program);
    if (status != C_OK)
      return status;

    status = WriteEncodedProgram(encoded, transformed_element);
    DeleteEncodedProgram(encoded);

    return status;
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    Status status;
    EncodedProgram* encoded_program = NULL;
    status = ReadEncodedProgram(transformed_element, &encoded_program);
    if (status != C_OK)
      return status;

    status = Assemble(encoded_program, reformed_element);
    DeleteEncodedProgram(encoded_program);
    if (status != C_OK)
      return status;

    return C_OK;
  }

 private:
  Region ensemble_region_;

  uint32 base_offset_;
  uint32 base_length_;

  DISALLOW_COPY_AND_ASSIGN(PatcherX86_32);
};

}  // namespace
#endif  // COURGETTE_WIN32_X86_PATCHER_H_
