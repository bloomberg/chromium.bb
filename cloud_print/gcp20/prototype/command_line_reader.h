// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_COMMAND_LINE_READER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_COMMAND_LINE_READER_H_

#include <string>

#include "base/basictypes.h"

namespace command_line_reader {

uint16 ReadHttpPort(uint16 default_value);

uint32 ReadTtl(uint32 default_value);

std::string ReadServiceNamePrefix(const std::string& default_value);

std::string ReadDomainName(const std::string& default_value);

std::string ReadStatePath(const std::string& default_value);

}  // namespace command_line_reader

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_COMMAND_LINE_READER_H_

