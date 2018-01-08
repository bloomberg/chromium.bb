// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/installed_programs_win.h"

#include <algorithm>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/msi_util_win.h"

namespace {

// Returns true if |candidate| is registered as a system component.
bool IsSystemComponent(const base::win::RegKey& candidate) {
  DWORD system_component = 0;
  return candidate.ReadValueDW(L"SystemComponent", &system_component) ==
             ERROR_SUCCESS &&
         system_component == 1;
}

// Fetches a string |value| out of |key|. Return false if a non-empty value
// could not be retrieved.
bool GetValue(const base::win::RegKey& key,
              const wchar_t* value,
              base::string16* result) {
  return key.ReadValue(value, result) == ERROR_SUCCESS && !result->empty();
}

// Try to get the |install_path| from |candidate| using the InstallLocation
// value. Return true on success.
bool GetInstallPathUsingInstallLocation(const base::win::RegKey& candidate,
                                        base::FilePath* install_path) {
  base::string16 install_location;
  if (GetValue(candidate, L"InstallLocation", &install_location)) {
    *install_path = base::FilePath(std::move(install_location));
    return true;
  }
  return false;
}

// Returns true if the |component_path| points to a registry key. Registry key
// paths are characterized by a number instead of a drive letter.
// See the documentation for ::MsiGetComponentPath():
// https://msdn.microsoft.com/library/windows/desktop/aa370112.aspx
bool IsRegistryComponentPath(const base::string16& component_path) {
  base::string16 drive_letter =
      component_path.substr(0, component_path.find(':'));

  for (const wchar_t* registry_drive_letter :
       {L"00", L"01", L"02", L"03", L"20", L"21", L"22", L"23"}) {
    if (drive_letter == registry_drive_letter)
      return true;
  }

  return false;
}

// Returns all the files installed by the product identified by |product_guid|.
// Returns true on success.
bool GetInstalledFilesUsingMsiGuid(
    const base::string16& product_guid,
    const MsiUtil& msi_util,
    std::vector<base::FilePath>* installed_files) {
  // An invalid product guid may have been passed to this function. In this
  // case, GetMsiComponentPaths() will return false so it is not necessary to
  // specifically filter those out.
  std::vector<base::string16> component_paths;
  if (!msi_util.GetMsiComponentPaths(product_guid, &component_paths))
    return false;

  for (auto& component_path : component_paths) {
    // Exclude registry component paths.
    if (IsRegistryComponentPath(component_path))
      continue;

    installed_files->push_back(base::FilePath(std::move(component_path)));
  }

  return true;
}

// Checks if the registry key references an installed program in the Apps &
// Features settings page. Also keeps tracks of the memory used by all the
// strings that are added to |programs_data| and adds it to |size_in_bytes|.
void CheckRegistryKeyForInstalledProgram(
    HKEY hkey,
    const base::string16& key_path,
    REGSAM wow64access,
    const base::string16& key_name,
    const MsiUtil& msi_util,
    InstalledPrograms::ProgramsData* programs_data,
    int* size_in_bytes) {
  base::string16 candidate_key_path =
      base::StringPrintf(L"%ls\\%ls", key_path.c_str(), key_name.c_str());
  base::win::RegKey candidate(hkey, candidate_key_path.c_str(),
                              KEY_QUERY_VALUE | wow64access);

  if (!candidate.Valid())
    return;

  // System components are not displayed in the Add or remove programs list.
  if (IsSystemComponent(candidate))
    return;

  // If there is no UninstallString, the Uninstall button is grayed out.
  base::string16 uninstall_string;
  if (!GetValue(candidate, L"UninstallString", &uninstall_string))
    return;

  // Ignore Microsoft programs.
  base::string16 publisher;
  if (GetValue(candidate, L"Publisher", &publisher) &&
      base::StartsWith(publisher, L"Microsoft", base::CompareCase::SENSITIVE)) {
    return;
  }

  // Because this class is used to display a warning to the user, not having
  // a display name renders the warning somewhat useless. Ignore those
  // candidates.
  base::string16 display_name;
  if (!GetValue(candidate, L"DisplayName", &display_name))
    return;

  base::FilePath install_path;
  if (GetInstallPathUsingInstallLocation(candidate, &install_path)) {
    *size_in_bytes +=
        display_name.length() * sizeof(base::string16::value_type);
    programs_data->program_names.push_back(std::move(display_name));

    *size_in_bytes +=
        install_path.value().length() * sizeof(base::FilePath::CharType);
    const size_t program_name_index = programs_data->program_names.size() - 1;
    programs_data->install_directories.emplace_back(std::move(install_path),
                                                    program_name_index);
    return;
  }

  std::vector<base::FilePath> installed_files;
  if (GetInstalledFilesUsingMsiGuid(key_name, msi_util, &installed_files)) {
    *size_in_bytes +=
        display_name.length() * sizeof(base::string16::value_type);
    programs_data->program_names.push_back(std::move(display_name));

    const size_t program_name_index = programs_data->program_names.size() - 1;
    for (auto& installed_file : installed_files) {
      *size_in_bytes +=
          installed_file.value().length() * sizeof(base::FilePath::CharType);
      programs_data->installed_files.emplace_back(std::move(installed_file),
                                                  program_name_index);
    }
  }
}

// Helper function to sort |container| using CompareLessIgnoreCase.
void SortByFilePaths(
    std::vector<std::pair<base::FilePath, size_t>>* container) {
  std::sort(container->begin(), container->end(),
            [](const auto& lhs, const auto& rhs) {
              return base::FilePath::CompareLessIgnoreCase(lhs.first.value(),
                                                           rhs.first.value());
            });
}

// Populates and returns a ProgramsData instance.
std::unique_ptr<InstalledPrograms::ProgramsData> GetProgramsData(
    std::unique_ptr<MsiUtil> msi_util) {
  SCOPED_UMA_HISTOGRAM_TIMER("ThirdPartyModules.InstalledPrograms.GetDataTime");

  auto programs_data = base::MakeUnique<InstalledPrograms::ProgramsData>();

  // Iterate over all the variants of the uninstall registry key.
  const wchar_t kUninstallKeyPath[] =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

  // The "HKCU\SOFTWARE\" registry subtree is shared between both 32-bits and
  // 64-bits views. Accessing both would create duplicate entries.
  // https://msdn.microsoft.com/library/windows/desktop/aa384253.aspx
  static const std::pair<HKEY, REGSAM> kCombinations[] = {
      {HKEY_CURRENT_USER, 0},
      {HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY},
      {HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY},
  };
  int size_in_bytes = 0;
  for (const auto& combination : kCombinations) {
    for (base::win::RegistryKeyIterator i(combination.first, kUninstallKeyPath,
                                          combination.second);
         i.Valid(); ++i) {
      CheckRegistryKeyForInstalledProgram(
          combination.first, kUninstallKeyPath, combination.second, i.Name(),
          *msi_util, programs_data.get(), &size_in_bytes);
    }
  }

  // The vectors are sorted so that binary searching can be used. No additional
  // entries will be added anyways.
  SortByFilePaths(&programs_data->installed_files);
  SortByFilePaths(&programs_data->install_directories);

  // Calculate the size taken by |programs_data|.
  size_in_bytes +=
      programs_data->program_names.capacity() * sizeof(base::string16);
  size_in_bytes += programs_data->installed_files.capacity() *
                   sizeof(std::pair<base::FilePath, size_t>);
  size_in_bytes += programs_data->install_directories.capacity() *
                   sizeof(std::pair<base::FilePath, size_t>);

  // Using the function version of this UMA histogram because this will only be
  // invoked once during a browser run so the caching that comes with the macro
  // version is wasted resources.
  base::UmaHistogramMemoryKB("ThirdPartyModules.InstalledPrograms.DataSize",
                             size_in_bytes / 1024);

  return programs_data;
}

}  // namespace

InstalledPrograms::ProgramsData::ProgramsData() = default;

InstalledPrograms::ProgramsData::~ProgramsData() = default;

InstalledPrograms::InstalledPrograms()
    : initialized_(false), weak_ptr_factory_(this) {}

InstalledPrograms::~InstalledPrograms() = default;

void InstalledPrograms::Initialize(
    const base::Closure& on_initialized_callback) {
  Initialize(on_initialized_callback, base::MakeUnique<MsiUtil>());
}

bool InstalledPrograms::GetInstalledProgramNames(
    const base::FilePath& file,
    std::vector<base::string16>* program_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  // First, check if an exact file match exists in the installed files list.
  if (GetNamesFromInstalledFiles(file, program_names))
    return true;

  // Then try to find a parent directory in the install directories list.
  return GetNamesFromInstallDirectories(file, program_names);
}

void InstalledPrograms::Initialize(const base::Closure& on_initialized_callback,
                                   std::unique_ptr<MsiUtil> msi_util) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetProgramsData, std::move(msi_util)),
      base::BindOnce(&InstalledPrograms::OnInitializationDone,
                     weak_ptr_factory_.GetWeakPtr(), on_initialized_callback));
}

bool InstalledPrograms::GetNamesFromInstalledFiles(
    const base::FilePath& file,
    std::vector<base::string16>* program_names) {
  // This functor is used to find all exact items by their key in a collection
  // of key/value pairs.
  struct FilePathLess {
    bool operator()(const std::pair<base::FilePath, size_t> element,
                    const base::FilePath& file) {
      return base::FilePath::CompareLessIgnoreCase(element.first.value(),
                                                   file.value());
    }
    bool operator()(const base::FilePath& file,
                    const std::pair<base::FilePath, size_t> element) {
      return base::FilePath::CompareLessIgnoreCase(file.value(),
                                                   element.first.value());
    }
  };

  auto equal_range = std::equal_range(programs_data_->installed_files.begin(),
                                      programs_data_->installed_files.end(),
                                      file, FilePathLess());

  // If the range is of size 0, no matching files were found.
  if (std::distance(equal_range.first, equal_range.second) == 0)
    return false;

  for (auto iter = equal_range.first; iter != equal_range.second; ++iter) {
    program_names->push_back(programs_data_->program_names[iter->second]);
  }

  return true;
}

bool InstalledPrograms::GetNamesFromInstallDirectories(
    const base::FilePath& file,
    std::vector<base::string16>* program_names) {
  // This functor is used to find all matching items by their key in a
  // collection of key/value pairs. This also takes advantage of the fact that
  // only the first element of the pair is a directory.
  struct FilePathParentLess {
    bool operator()(const std::pair<base::FilePath, size_t> directory,
                    const base::FilePath& file) {
      if (directory.first.IsParent(file))
        return false;
      return base::FilePath::CompareLessIgnoreCase(directory.first.value(),
                                                   file.value());
    }
    bool operator()(const base::FilePath& file,
                    const std::pair<base::FilePath, size_t> directory) {
      if (directory.first.IsParent(file))
        return false;
      return base::FilePath::CompareLessIgnoreCase(file.value(),
                                                   directory.first.value());
    }
  };

  auto equal_range = std::equal_range(
      programs_data_->install_directories.begin(),
      programs_data_->install_directories.end(), file, FilePathParentLess());

  // Skip cases where there are multiple matches because there is no way to know
  // which program is the real owner of the |file| with the data owned by us.
  if (std::distance(equal_range.first, equal_range.second) != 1)
    return false;

  program_names->push_back(
      programs_data_->program_names[equal_range.first->second]);
  return true;
}

void InstalledPrograms::OnInitializationDone(
    const base::Closure& on_initialized_callback,
    std::unique_ptr<ProgramsData> programs_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);

  programs_data_ = std::move(programs_data);

  initialized_ = true;
  if (on_initialized_callback)
    on_initialized_callback.Run();
}
