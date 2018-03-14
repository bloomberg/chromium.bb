// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_INSTALLED_PROGRAMS_WIN_H_
#define CHROME_BROWSER_CONFLICTS_INSTALLED_PROGRAMS_WIN_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/windows_types.h"

class MsiUtil;

// This class inspects the user's installed programs and builds a mapping of
// files to its associated program.
//
// Installed programs are found by searching the
// "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" registry key and
// its variants. There are 2 cases that are covered:
//
// 1 - If the program's installer did its due dilligence, it populated the
//     "InstallLocation" registry key with the directory where it was installed,
//     and all the files under that directory are assumed to be owned by this
//     program.
//
//     In the event of 2 conflicting "InstallLocation", both are ignored as this
//     method doesn't let us know for sure who is the owner of any enclosing
//     files.
//
// 2 - If the program's entry is a valid MSI Product GUID, the complete list of
//     associated file is used to exactly match a given file to a program.
//
//     If multiple products installed the same file as the same component,
//     Windows keeps a reference count of that component so that the file
//     doesn't get removed if one of them is uninstalled. So both programs are
//     returned by GetInstalledPrograms().
//
//  Note: Programs may be skipped and so would not be returned by
//        GetInstalledPrograms() for the following reasons:
//        - The program is owned by Microsoft.
//        - The uninstall entry is marked as a system component.
//        - The uninstall entry has no display name.
//        - The uninstall entry has no UninstallString.
//
class InstalledPrograms {
 public:
  struct ProgramInfo {
    base::string16 name;

    // Holds the path to the uninstall entry in the registry.
    HKEY registry_root;
    base::string16 registry_key_path;
    REGSAM registry_wow64_access;
  };

  // Initializes this instance with the list of installed programs. While the
  // constructor must be called in a sequence that allows blocking, its public
  // method can be used without such restrictions.
  InstalledPrograms();

  virtual ~InstalledPrograms();

  // Given a |file|, checks if it matches an installed program on the user's
  // machine and appends all the matching programs to |programs|.
  // Virtual to allow mocking.
  virtual bool GetInstalledPrograms(const base::FilePath& file,
                                    std::vector<ProgramInfo>* programs) const;

 protected:
  // Protected so that tests can subclass InstalledPrograms and access it.
  explicit InstalledPrograms(std::unique_ptr<MsiUtil> msi_util);

 private:
  // If the registry key references a valid installed program, this function
  // adds an entry to |programs_| with its list of files or installation
  // directory to their associated vector.
  void CheckRegistryKeyForInstalledProgram(HKEY hkey,
                                           const base::string16& key_path,
                                           REGSAM wow64access,
                                           const base::string16& key_name,
                                           const MsiUtil& msi_util);

  bool GetProgramsFromInstalledFiles(const base::FilePath& file,
                                     std::vector<ProgramInfo>* programs) const;
  bool GetProgramsFromInstallDirectories(
      const base::FilePath& file,
      std::vector<ProgramInfo>* programs) const;

  // Programs are stored in this vector because multiple entries in
  // |installed_files| could point to the same one. This is to avoid
  // duplicating them.
  std::vector<ProgramInfo> programs_;

  // Contains all the files from programs installed via Microsoft Installer.
  // The second part of the pair is the index into |programs|.
  std::vector<std::pair<base::FilePath, size_t>> installed_files_;

  // For some programs, the best information available is the directory of the
  // installation. The compare functor treats file paths where one is the
  // parent of the other as equal.
  // The second part of the pair is the index into |programs|.
  std::vector<std::pair<base::FilePath, size_t>> install_directories_;

  DISALLOW_COPY_AND_ASSIGN(InstalledPrograms);
};

bool operator<(const InstalledPrograms::ProgramInfo& lhs,
               const InstalledPrograms::ProgramInfo& rhs);

#endif  // CHROME_BROWSER_CONFLICTS_INSTALLED_PROGRAMS_WIN_H_
