// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_WIN32_X86_PATCHER_H_
#define COURGETTE_WIN32_X86_PATCHER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "courgette/assembly_program.h"
#include "courgette/encoded_program.h"
#include "courgette/ensemble.h"
#include "courgette/program_detector.h"

namespace courgette {

// PatcherX86_32 is the universal patcher for all executables.  The executable
// type is determined by ParseDetectedExecutable function.
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

    scoped_ptr<AssemblyProgram> program;
    status = ParseDetectedExecutable(ensemble_region_.start() + base_offset_,
                                     base_length_,
                                     &program);
    if (status != C_OK)
      return status;

    scoped_ptr<EncodedProgram> encoded;
    status = Encode(*program, &encoded);
    if (status != C_OK)
      return status;

    program.reset();

    return WriteEncodedProgram(encoded.get(), transformed_element);
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    Status status;
    scoped_ptr<EncodedProgram> encoded_program;
    status = ReadEncodedProgram(transformed_element, &encoded_program);
    if (status != C_OK)
      return status;

    return Assemble(encoded_program.get(), reformed_element);
  }

 private:
  Region ensemble_region_;

  uint32_t base_offset_;
  uint32_t base_length_;

  DISALLOW_COPY_AND_ASSIGN(PatcherX86_32);
};

}  // namespace

#endif  // COURGETTE_WIN32_X86_PATCHER_H_
