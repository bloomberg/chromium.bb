// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/error_utils.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace extensions {

std::string ErrorUtils::FormatErrorMessage(const std::string& format,
                                           const std::string& s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return ret_val;
}

std::string ErrorUtils::FormatErrorMessage(const std::string& format,
                                           const std::string& s1,
                                           const std::string& s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return ret_val;
}

std::string ErrorUtils::FormatErrorMessage(const std::string& format,
                                           const std::string& s1,
                                           const std::string& s2,
                                           const std::string& s3) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s3);
  return ret_val;
}

string16 ErrorUtils::FormatErrorMessageUTF16(const std::string& format,
                                             const std::string& s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return UTF8ToUTF16(ret_val);
}

string16 ErrorUtils::FormatErrorMessageUTF16(const std::string& format,
                                             const std::string& s1,
                                             const std::string& s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return UTF8ToUTF16(ret_val);
}

string16 ErrorUtils::FormatErrorMessageUTF16(const std::string& format,
                                             const std::string& s1,
                                             const std::string& s2,
                                             const std::string& s3) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s3);
  return UTF8ToUTF16(ret_val);
}

}  // namespace
