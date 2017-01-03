// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_DATA_PIPE_UTILS_H_
#define MOJO_COMMON_DATA_PIPE_UTILS_H_

#include <stdint.h>

#include <string>

#include "mojo/common/mojo_common_export.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {
namespace common {

// Copies the data from |source| into |contents| and returns true on success and
// false on error.  In case of I/O error, |contents| holds the data that could
// be read from source before the error occurred.
bool MOJO_COMMON_EXPORT BlockingCopyToString(
    ScopedDataPipeConsumerHandle source,
    std::string* contents);

bool MOJO_COMMON_EXPORT BlockingCopyFromString(
    const std::string& source,
    const ScopedDataPipeProducerHandle& destination);

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_DATA_PIPE_UTILS_H_
