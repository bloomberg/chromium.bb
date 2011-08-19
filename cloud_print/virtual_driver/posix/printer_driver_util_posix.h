// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_POSIX_PRINTER_DRIVER_UTIL_POSIX_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_POSIX_PRINTER_DRIVER_UTIL_POSIX_H_
#pragma once

#include <string>

#include "base/file_path.h"

namespace printer_driver_util {

void LaunchPrintDialog(const std::string& output_path,
                       const std::string& job_title,
                       const std::string& current_user,
                       const std::string& print_ticket);
void WriteToTemp(FILE* input_pdf, FilePath output_path);
void SetUser(const char* user);
void GetOptions(const char* options, std::string* print_ticket);

}  // namespace printer_driver_util

#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_POSIX_PRINTER_DRIVER_UTIL_POSIX_H_
