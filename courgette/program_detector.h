// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PROGRAM_DETECTOR_H_
#define COURGETTE_PROGRAM_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "courgette/courgette.h"

namespace courgette {

class AssemblyProgram;

// Detects the type of an executable file, and it's length. The length may be
// slightly smaller than some executables (like ELF), but will include all bytes
// the courgette algorithm has special benefit for.
// On success:
//   Fills in |type| and |detected_length|, and returns C_OK.
// On failure:
//   Fills in |type| with UNKNOWN, |detected_length| with 0, and returns
//   C_INPUT_NOT_RECOGNIZED.
Status DetectExecutableType(const uint8_t* buffer,
                            size_t length,
                            ExecutableType* type,
                            size_t* detected_length);

// Attempts to detect the type of executable, and parse it with the appropriate
// tools.
// On success:
//   Parses the executable into a new AssemblyProgram in |*output|, and returns
//   C_OK.
// On failure:
//   Returns an error status and assigns |*output| to null.
Status ParseDetectedExecutable(const uint8_t* buffer,
                               size_t length,
                               std::unique_ptr<AssemblyProgram>* output);

}  // namespace courgette

#endif  // COURGETTE_PROGRAM_DETECTOR_H_
