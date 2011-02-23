// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/auto_start_linux.h"

#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"

namespace {

const FilePath::CharType kAutostart[] = "autostart";
const FilePath::CharType kConfig[] = ".config";
const char kXdgConfigHome[] = "XDG_CONFIG_HOME";

FilePath GetAutostartDirectory(base::Environment* environment) {
  FilePath result =
      base::nix::GetXDGDirectory(environment, kXdgConfigHome, kConfig);
  result = result.Append(kAutostart);
  return result;
}

}  // namespace

bool AutoStart::AddApplication(const std::string& autostart_filename,
                               const std::string& application_name,
                               const std::string& command_line,
                               bool is_terminal_app) {
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  FilePath autostart_directory = GetAutostartDirectory(environment.get());
  if (!file_util::DirectoryExists(autostart_directory) &&
      !file_util::CreateDirectory(autostart_directory)) {
    return false;
  }

  FilePath autostart_file = autostart_directory.Append(autostart_filename);
  std::string autostart_file_contents =
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Terminal=" + is_terminal_app ? "true\n" : "false\n"
      "Exec=" + command_line + "\n"
      "Name=" + application_name + "\n";
  std::string::size_type content_length = autostart_file_contents.length();
  if (file_util::WriteFile(autostart_file, autostart_file_contents.c_str(),
                           content_length) !=
      static_cast<int>(content_length)) {
    file_util::Delete(autostart_file, false);
    return false;
  }
  return true;
}

bool AutoStart::Remove(const std::string& autostart_filename) {
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  FilePath autostart_directory = GetAutostartDirectory(environment.get());
  FilePath autostart_file = autostart_directory.Append(autostart_filename);
  return file_util::Delete(autostart_file, false);
}
