// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/file_utils.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace resource_provider {
namespace {

bool IsPathNameValid(const std::string& name) {
  if (name.empty() || name == "." || name == "..")
    return false;

  for (auto c : name) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) &&
        c != '_' && c != '.')
      return false;
  }
  return true;
}

}  // namespace

base::FilePath GetPathForApplicationName(const std::string& application_name) {
  std::string path = application_name;
  const bool is_mojo =
      base::StartsWith(path, "mojo:", base::CompareCase::INSENSITIVE_ASCII);
  const bool is_exe =
      !is_mojo &&
      base::StartsWith(path, "exe:", base::CompareCase::INSENSITIVE_ASCII);
  if (!is_mojo && !is_exe)
    return base::FilePath();
  if (path.find('.') != std::string::npos)
    return base::FilePath();
  if (is_mojo)
    path.erase(path.begin(), path.begin() + 5);
  else
    path.erase(path.begin(), path.begin() + 4);
  base::TrimString(path, "/", &path);
  size_t end_of_name = path.find('/');
  if (end_of_name != std::string::npos)
    path.erase(path.begin() + end_of_name, path.end());

  if (!IsPathNameValid(path))
    return base::FilePath();

  base::FilePath base_path;
#if defined(OS_ANDROID)
  PathService::Get(base::DIR_ANDROID_APP_DATA, &base_path);
  // |base_path| on android has an additional path, need to go up a level to get
  // at other apps resources.
  base_path = base_path.DirName();
  base_path = base_path.AppendASCII("app_cached_apps");
#else
  PathService::Get(base::DIR_EXE, &base_path);
#endif
  // TODO(beng): this won't handle user-specific components.
  return base_path.AppendASCII("Mojo Applications").AppendASCII(path).
      AppendASCII("resources");
}

base::FilePath GetPathForResourceNamed(const base::FilePath& app_path,
                                       const std::string& resource_path) {
  CHECK(!app_path.empty());

  if (resource_path.empty() || resource_path[0] == '/' ||
      resource_path[resource_path.size() - 1] == '/' ||
      resource_path.find("//") != std::string::npos)
    return base::FilePath();

  std::vector<std::string> path_components = base::SplitString(
      resource_path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (path_components.empty())
    return base::FilePath();

  base::FilePath result(app_path);
  for (const auto& path_component : path_components) {
    if (!IsPathNameValid(path_component))
      return base::FilePath();
    result = result.AppendASCII(path_component);
  }
  return result;
}

}  // namespace resource_provider
