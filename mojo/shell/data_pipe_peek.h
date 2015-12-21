// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DATA_PIPE_PEEK_H_
#define MOJO_SHELL_DATA_PIPE_PEEK_H_

#include <stddef.h>

#include <string>

#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace shell {

// The Peek functions are only intended to be used by the
// DyanmicApplicationLoader class for discovering the type of a
// URL response. They are a stopgap to be replaced by real support
// in the DataPipe classes.

// Return true and the first newline terminated line from source. Return false
// if more than max_line_length bytes are scanned without seeing a newline, or
// if the timeout is exceeded.
bool BlockingPeekLine(DataPipeConsumerHandle source,
                      std::string* line,
                      size_t max_line_length,
                      MojoDeadline timeout);

// Return true and the first bytes_length bytes from source. Return false
// if the timeout is exceeded.
bool BlockingPeekNBytes(DataPipeConsumerHandle source,
                        std::string* bytes,
                        size_t bytes_length,
                        MojoDeadline timeout);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DATA_PIPE_PEEK_H_
