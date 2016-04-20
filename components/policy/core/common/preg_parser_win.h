// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a parser for PReg files which are used for storing group
// policy settings in the file system. The file format is documented here:
//
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa374407(v=vs.85).aspx

#ifndef COMPONENTS_POLICY_CORE_COMMON_PREG_PARSER_WIN_H_
#define COMPONENTS_POLICY_CORE_COMMON_PREG_PARSER_WIN_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "components/policy/policy_export.h"

namespace base {
class FilePath;
}

namespace policy {

class PolicyLoadStatusSample;
class RegistryDict;

namespace preg_parser {

// The magic header in PReg files: ASCII "PReg" + version (0x0001).
POLICY_EXPORT extern const char kPRegFileHeader[8];

// Reads the PReg file at |file_path| and writes the registry data to |dict|.
// |root| specifies the registry subtree the caller is interested in,
// everything else gets ignored.
POLICY_EXPORT bool ReadFile(const base::FilePath& file_path,
                            const base::string16& root,
                            RegistryDict* dict,
                            PolicyLoadStatusSample* status);

}  // namespace preg_parser
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_PREG_PARSER_WIN_H_
