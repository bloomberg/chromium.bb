// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/tools/package_manager/manifest.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "url/gurl.h"

namespace mojo {

Manifest::Manifest() {
}

Manifest::~Manifest() {
}

bool Manifest::Parse(const std::string& str, std::string* err_msg) {
  int err_code = base::JSONReader::JSON_NO_ERROR;
  scoped_ptr<base::Value> root(base::JSONReader::ReadAndReturnError(
      str,
      base::JSON_ALLOW_TRAILING_COMMAS,
      &err_code, err_msg));
  if (err_code != base::JSONReader::JSON_NO_ERROR)
    return false;

  const base::DictionaryValue* root_dict;
  if (!root->GetAsDictionary(&root_dict)) {
    *err_msg = "Manifest is not a dictionary.";
    return false;
  }

  if (!PopulateDeps(root_dict, err_msg))
    return false;

  return true;
}

bool Manifest::ParseFromFile(const base::FilePath& file_name,
                             std::string* err_msg) {
  std::string data;
  if (!base::ReadFileToString(file_name, &data)) {
    *err_msg = "Couldn't read manifest file " + file_name.AsUTF8Unsafe();
    return false;
  }
  return Parse(data, err_msg);
}

bool Manifest::PopulateDeps(const base::DictionaryValue* root,
                            std::string* err_msg) {
  const base::Value* deps_value;
  if (!root->Get("deps", &deps_value))
    return true;  // No deps, that's OK.

  const base::ListValue* deps;
  if (!deps_value->GetAsList(&deps)) {
    *err_msg = "Deps is not a list. Should be \"deps\": [ \"...\", \"...\" ]";
    return false;
  }

  deps_.reserve(deps->GetSize());
  for (size_t i = 0; i < deps->GetSize(); i++) {
    std::string cur_dep;
    if (!deps->GetString(i, &cur_dep)) {
      *err_msg = "Dependency list item wasn't a string.";
      return false;
    }

    GURL cur_url(cur_dep);
    if (!cur_url.is_valid()) {
      *err_msg = "Dependency entry isn't a valid URL: " + cur_dep;
      return false;
    }

    deps_.push_back(cur_url);
  }

  return true;
}

}  // namespace mojo
