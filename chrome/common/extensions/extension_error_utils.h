// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_
#pragma once

#include <string>

#include "base/string16.h"

class ExtensionErrorUtils {
 public:
  // Creates an error messages from a pattern.
  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1);

  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1,
                                        const std::string& s2);

  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1,
                                        const std::string& s2,
                                        const std::string& s3);

  static string16 FormatErrorMessageUTF16(const std::string& format,
                                          const std::string& s1);

  static string16 FormatErrorMessageUTF16(const std::string& format,
                                          const std::string& s1,
                                          const std::string& s2);

  static string16 FormatErrorMessageUTF16(const std::string& format,
                                          const std::string& s1,
                                          const std::string& s2,
                                          const std::string& s3);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_
