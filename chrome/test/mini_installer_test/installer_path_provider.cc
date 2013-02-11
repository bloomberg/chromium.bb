// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/installer_path_provider.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct FilePathInfo {
  file_util::FileEnumerator::FindInfo info;
  base::FilePath path;
};

bool CompareDate(const FilePathInfo& a,
                 const FilePathInfo& b) {
#if defined(OS_POSIX)
  return a.info.stat.st_mtime > b.info.stat.st_mtime;
#elif defined(OS_WIN)
  if (a.info.ftLastWriteTime.dwHighDateTime ==
      b.info.ftLastWriteTime.dwHighDateTime) {
    return a.info.ftLastWriteTime.dwLowDateTime >
           b.info.ftLastWriteTime.dwLowDateTime;
  } else {
    return a.info.ftLastWriteTime.dwHighDateTime >
           b.info.ftLastWriteTime.dwHighDateTime;
  }
#endif
}

// Get list of file |type| matching |pattern| in |root|.
// The list is sorted in last modified date order.
// Return true if files/directories are found.
bool FindMatchingFiles(const base::FilePath& root,
                       const std::string& pattern,
                       file_util::FileEnumerator::FileType type,
                       std::vector<base::FilePath>* paths) {
  file_util::FileEnumerator files(root, false, type,
      base::FilePath().AppendASCII(pattern).value());
  std::vector<FilePathInfo> matches;
  for (base::FilePath current = files.Next(); !current.empty();
      current = files.Next()) {
    FilePathInfo entry;
    files.GetFindInfo(&entry.info);
    entry.path = current;
    matches.push_back(entry);
  }

  if (matches.empty())
    return false;

  std::sort(matches.begin(), matches.end(), CompareDate);
  std::vector<FilePathInfo>::iterator current;
  for (current = matches.begin(); current != matches.end(); ++current) {
    paths->push_back(current->path);
  }
  return true;
}

bool FindNewestMatchingFile(const base::FilePath& root,
                            const std::string& pattern,
                            file_util::FileEnumerator::FileType type,
                            base::FilePath* path) {
  std::vector<base::FilePath> paths;
  if (FindMatchingFiles(root, pattern, type, &paths)) {
    *path = paths[0];
    return true;
  }
  return false;
}

}  // namespace

namespace installer_test {

InstallerPathProvider::InstallerPathProvider() {
  base::FilePath full_installer, previous_installer;
  if (!GetFullInstaller(&full_installer) ||
      !GetPreviousInstaller(&previous_installer))
    return;
  current_build_ =
      full_installer.DirName().DirName().BaseName().MaybeAsASCII();
  previous_build_ =
      previous_installer.DirName().DirName().BaseName().MaybeAsASCII();
}

InstallerPathProvider::InstallerPathProvider(
    const std::string& build_under_test) : current_build_(build_under_test) {
  base::FilePath full_installer, previous_installer;
  if (!GetFullInstaller(&full_installer) ||
      !GetPreviousInstaller(&previous_installer))
    return;
  previous_build_ =
      previous_installer.DirName().DirName().BaseName().MaybeAsASCII();
}

InstallerPathProvider::~InstallerPathProvider() {}

bool InstallerPathProvider::GetFullInstaller(base::FilePath* path) {
  std::string full_installer_pattern("*_chrome_installer*");
  return GetInstaller(full_installer_pattern, path);
}

bool InstallerPathProvider::GetDiffInstaller(base::FilePath* path) {
  std::string diff_installer_pattern("*_from_*");
  return GetInstaller(diff_installer_pattern, path);
}

bool InstallerPathProvider::GetMiniInstaller(base::FilePath* path) {
  // Use local copy of installer, else fall back to filer.
  base::FilePath mini_installer = PathFromExeDir(
      mini_installer_constants::kChromeMiniInstallerExecutable);
  if (file_util::PathExists(mini_installer)) {
    *path = mini_installer;
    return true;
  }
  std::string mini_installer_pattern("mini_installer.exe");
  return GetInstaller(mini_installer_pattern, path);
}

bool InstallerPathProvider::GetPreviousInstaller(base::FilePath* path) {
  std::string diff_installer_pattern("*_from_*");
  std::string full_installer_pattern("*_chrome_installer*");
  base::FilePath diff_installer;
  if (!GetInstaller(diff_installer_pattern, &diff_installer))
    return false;

  base::FilePath previous_installer;
  std::vector<std::string> tokenized_name;
  Tokenize(diff_installer.BaseName().MaybeAsASCII(),
      "_", &tokenized_name);
  std::string build_pattern = base::StringPrintf(
      "*%s", tokenized_name[2].c_str());
  std::vector<base::FilePath> previous_build;
  if (FindMatchingFiles(diff_installer.DirName().DirName().DirName(),
      build_pattern, file_util::FileEnumerator::DIRECTORIES,
      &previous_build)) {
    base::FilePath windir = previous_build.at(0).Append(
        mini_installer_constants::kWinFolder);
    FindNewestMatchingFile(windir, full_installer_pattern,
        file_util::FileEnumerator::FILES, &previous_installer);
  }

  if (previous_installer.empty())
    return false;
  *path = previous_installer;
  return true;
}

bool InstallerPathProvider::GetStandaloneInstaller(base::FilePath* path) {
  // Get standalone installer.
  base::FilePath standalone_installer(
      mini_installer_constants::kChromeStandAloneInstallerLocation);
  // Get the file name.
  std::vector<std::string> tokenized_build_number;
  if (current_build_.empty())
    return false;
  Tokenize(current_build_, ".", &tokenized_build_number);
  std::string standalone_installer_filename = base::StringPrintf(
      "%s%s_%s.exe",
      base::FilePath(mini_installer_constants::kUntaggedInstallerPattern)
          .MaybeAsASCII().c_str(),
      tokenized_build_number[2].c_str(),
      tokenized_build_number[3].c_str());
  standalone_installer = standalone_installer.AppendASCII(current_build_)
      .Append(mini_installer_constants::kWinFolder)
      .AppendASCII(standalone_installer_filename);
  *path = standalone_installer;
  return file_util::PathExists(standalone_installer);
}

bool InstallerPathProvider::GetSignedStandaloneInstaller(base::FilePath* path) {
  base::FilePath standalone_installer;
  if (!GetStandaloneInstaller(&standalone_installer))
    return false;
  base::FilePath tagged_installer = PathFromExeDir(
      mini_installer_constants::kStandaloneInstaller);
  CommandLine sign_command = CommandLine::FromString(
      base::StringPrintf(L"%ls %ls %ls %ls",
      mini_installer_constants::kChromeApplyTagExe,
      standalone_installer.value().c_str(),
      tagged_installer.value().c_str(),
      mini_installer_constants::kChromeApplyTagParameters));

  if (!installer_test::RunAndWaitForCommandToFinish(sign_command))
    return false;

  *path = PathFromExeDir(mini_installer_constants::kStandaloneInstaller);
  return true;
}

base::FilePath InstallerPathProvider::PathFromExeDir(
    const base::FilePath::StringType& name) {
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(name);
  return path;
}

bool InstallerPathProvider::GetInstaller(const std::string& pattern,
                                         base::FilePath* path) {
  base::FilePath installer;
  // Search filer for installer binary.
  base::FilePath root(mini_installer_constants::kChromeInstallersLocation);
  std::vector<base::FilePath> paths;
  if (!FindMatchingFiles(root, current_build_,
      file_util::FileEnumerator::DIRECTORIES, &paths)) {
    return false;
  }

  std::vector<base::FilePath>::const_iterator dir;
  for (dir = paths.begin(); dir != paths.end(); ++dir) {
    base::FilePath windir = dir->Append(
        mini_installer_constants::kWinFolder);
    if (FindNewestMatchingFile(windir, pattern,
            file_util::FileEnumerator::FILES, &installer)) {
      break;
    }
  }

  if (installer.empty()) {
    LOG(WARNING) << "Failed to find installer with pattern: " << pattern;
    return false;
  }

  *path = installer;
  return true;
}

std::string InstallerPathProvider::GetCurrentBuild() {
  return current_build_;
}

std::string InstallerPathProvider::GetPreviousBuild() {
  return previous_build_;
}

}  // namespace
