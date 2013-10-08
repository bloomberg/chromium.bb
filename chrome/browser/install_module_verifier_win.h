// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALL_MODULE_VERIFIER_WIN_H_
#define CHROME_BROWSER_INSTALL_MODULE_VERIFIER_WIN_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"

namespace base {
class ListValue;
}  // namespace base

typedef std::vector<std::pair<size_t, base::StringPiece> > AdditionalModules;

// Starts a background process to verify installed modules. Results are reported
// via UMA.
void BeginModuleVerification();

// Converts a list of modules descriptions extracted from EnumerateModulesModel
// into a list of loaded module name digests.
void ExtractLoadedModuleNameDigests(
    const base::ListValue& module_list,
    std::set<std::string>* loaded_module_name_digests);

// Verifies a list of installed module name digests and inserts installed module
// IDs into the supplied set.
void VerifyModules(const std::set<std::string>& module_name_digests,
                   const AdditionalModules& additional_modules,
                   std::set<size_t>* installed_module_ids);

// Parses a list of additional modules to verify. The data format is a series of
// lines. Each line starts with a decimal ID, then a module name digest,
// separated by a space. Lines are terminated by \r and/or \n. The result is a
// vector of pairs where the first value is an ID and the second value is the
// corresponding module name digest. The StringPieces use the same underlying
// storage as |raw_data|.
//
// Invalid lines are ignored.
void ParseAdditionalModules(const base::StringPiece& raw_data,
                            AdditionalModules* additional_modules);

#endif  // CHROME_BROWSER_INSTALL_MODULE_VERIFIER_WIN_H_
