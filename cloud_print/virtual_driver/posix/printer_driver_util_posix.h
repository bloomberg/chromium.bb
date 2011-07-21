// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_LINUX_PRINTER_DRIVER_UTIL_POSIX_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_LINUX_PRINTER_DRIVER_UTIL_POSIX_H_
#pragma once
#include <string>

void LaunchPrintDialog(std::string output_path, std::string job_title,
                       std::string current_user);

#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_LINUX_PRINTER_DRIVER_UTIL_POSIX_H_
