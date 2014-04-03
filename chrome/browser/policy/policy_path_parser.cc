// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/bind.h"
#include "base/files/file_path.h"

namespace {

void TrimQuotes(base::FilePath::StringType* path) {
  if (path->length() > 1 &&
      (((*path)[0] == FILE_PATH_LITERAL('"') &&
        (*path)[path->length() - 1] == FILE_PATH_LITERAL('"')) ||
       ((*path)[0] == FILE_PATH_LITERAL('\'') &&
        (*path)[path->length() - 1] == FILE_PATH_LITERAL('\'')))) {
    // Strip first and last char which should be matching quotes now.
    *path = path->substr(1, path->length() - 2);
  }
}

}  // namespace

namespace policy {

namespace path_parser {

// This function performs a lazy call to the GetValueCallback, that is the
// callback is invoked only if the variable is found in the path. This is done
// to reduce the overhead during initialization.
void ReplaceVariableInPathWithValue(
    const base::FilePath::StringType& variable,
    const policy::path_parser::internal::GetValueFuncPtr& value_func_ptr,
    base::FilePath::StringType* path) {
  size_t position = path->find(variable);
  base::FilePath::StringType value;
  if (position != base::FilePath::StringType::npos && value_func_ptr(&value))
    path->replace(position, variable.length(), value);
}

// Replaces all variable occurrences in the policy string with the respective
// system settings values.
base::FilePath::StringType ExpandPathVariables(
    const base::FilePath::StringType& untranslated_string) {
  base::FilePath::StringType result(untranslated_string);

  if (result.length() == 0)
    return result;

  // Sanitize quotes in case of any around the whole string.
  TrimQuotes(&result);

  base::FilePath::StringType variable;
  internal::GetValueFuncPtr val_func_ptr;
  for (int i = 0; i < internal::kNoOfVariables; ++i) {
    variable = internal::kVariableNameAndValueCallbacks[i].name;
    val_func_ptr = internal::kVariableNameAndValueCallbacks[i].value_func_ptr;
    ReplaceVariableInPathWithValue(variable, val_func_ptr, &result);
  }

  return result;
}

}  // namespace path_parser

}  // namespace policy
