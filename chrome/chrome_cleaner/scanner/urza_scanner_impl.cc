// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/urza_scanner_impl.h"

#include <shlobj.h>

#include <locale>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/scoped_timed_task_logger.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_disk_util.h"
#include "chrome/chrome_cleaner/scanner/matcher_util.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/settings/settings_definitions.h"
#include "chrome/chrome_cleaner/strings/string_util.h"

namespace chrome_cleaner {

namespace {

bool FileMatchesRule(const base::FilePath& file_path,
                     PUPData::DiskMatchRule rule) {
  if (rule == PUPData::DISK_MATCH_ANY_FILE ||
      (PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1 <= rule &&
       rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END)) {
    return !file_path.empty();
  }
  // This is the only other rule we support so far.
  DCHECK(rule == PUPData::DISK_MATCH_BINARY_FILE);
  return PathHasActiveExtension(file_path);
}

base::FilePath FolderToRemove(const base::FilePath& file,
                              PUPData::DiskMatchRule rule) {
  DCHECK(PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1 <= rule &&
         rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END);
  base::FilePath folder_to_remove(file);
  for (int i = PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1; i <= rule; ++i) {
    folder_to_remove = folder_to_remove.DirName();
  }
  return folder_to_remove;
}

// Return true if the file matches |rule| or if at least one file that matches
// |rule| can be found in the hierarchy of descendant if |file_path| is a
// directory. Also update |expanded_disk_footprints| with the full path of all
// matched files, or just the first one if |options.only_one_footprint()| is
// true.
bool AnyFileMatches(const base::FilePath& file_path,
                    PUPData::DiskMatchRule rule,
                    const MatchingOptions& options,
                    PUPData::PUP* pup) {
  DCHECK(pup);
  if (!base::PathExists(file_path))
    return false;
  // If the path isn't a folder, then simply check if it matches |rule|.
  if (!base::DirectoryExists(file_path)) {
    if (FileMatchesRule(file_path, rule)) {
      // When we match a file in a folder structure, remove the appropriate
      // folder.
      if (PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1 <= rule &&
          rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END) {
        if (options.only_one_footprint())
          pup->AddDiskFootprint(file_path);
        else
          CollectPathsRecursively(FolderToRemove(file_path, rule), pup);
      } else {
        // No need to know if the file had already been added or not, it will
        // simply be ignored by the file path set of |pup|.
        pup->AddDiskFootprint(file_path);
      }
      return true;
    } else {
      return false;
    }
  }

  // Look for files recursively, and find at least one that matches |rule|, yet
  // collect all the files, since we'll need to delete them all if a match is
  // found. Unless this folder has already been matched (|AddDiskFootprint|
  // returns false when the item already exists).
  if (!pup->AddDiskFootprint(file_path))
    return true;

  base::FileEnumerator file_enum(
      file_path, true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  bool found_file = false;
  for (base::FilePath file = file_enum.Next(); !file.empty();
       file = file_enum.Next()) {
    pup->AddDiskFootprint(file);
    if (!base::DirectoryExists(file) && FileMatchesRule(file, rule)) {
      if (options.only_one_footprint())
        return true;
      found_file = true;
    }
  }
  return found_file;
}

// Performs expansion of |path|. If |csidl| is not |kInvalidCsidl|, |path| is
// expanded under that CSIDL. If a path can be expanded to multiple paths (e.g.
// if it's under Program Files), all expansions will be returned.
FilePathSet ExpandPath(int csidl, const base::FilePath& path) {
  FilePathSet expanded_paths;

  base::FilePath expanded_path;
  if (!ExpandEnvPath(path, &expanded_path))
    expanded_path = path;

  if (csidl == PUPData::kInvalidCsidl) {
    expanded_paths.Insert(expanded_path);
  } else {
    expanded_paths.Insert(ExpandSpecialFolderPath(csidl, expanded_path));
    switch (csidl) {
      case CSIDL_PROGRAM_FILES:
#if defined(_WIN64)
        expanded_paths.Insert(GetX86ProgramFilesPath(expanded_path));
#else
        expanded_paths.Insert(GetX64ProgramFilesPath(expanded_path));
#endif  // defined(_WIN64)
        break;
      case CSIDL_SYSTEM: {
        const base::FilePath system_path =
            PreFetchedPaths::GetInstance()->GetWindowsFolder();
        expanded_paths.Insert(system_path.Append(L"sysnative").Append(path));
        break;
      }
    }
  }

  return expanded_paths;
}

// Fill the |expanded_disk_footprints| field of |pup| and set
// |footprint_found| to true when a scan and remove footprint was found.
// When |options.only_one_footprint()| is true, this function returns as soon as
// one footprint is found (or immediately if |footprint_found| == true).
// Return false if an error occurred and scanning |pup| must be cancelled and
// marked as not found.
bool ExpandDiskFootprint(const MatchingOptions& options,
                         PUPData::PUP* pup,
                         bool* footprint_found) {
  DCHECK(pup);
  DCHECK(footprint_found);

  if (options.only_one_footprint() && *footprint_found)
    return true;

  // String format: Expanding disk footprints for: '%s'
  const std::string logging_text = base::StrCat(
      {"Expanding disk footprints for: '", PUPData::GetPUPName(pup), "'"});
  ScopedTimedTaskLogger scoped_timed_task_logger(logging_text.c_str());

  const PUPData::StaticDiskFootprint* disk_footprints =
      pup->signature().disk_footprints;
  for (size_t i = 0; disk_footprints[i].path != nullptr; ++i) {
    // Make sure to never try to remove a whole CSIDL folder.
    if (disk_footprints[i].csidl != PUPData::kInvalidCsidl &&
        wcslen(disk_footprints[i].path) == 0) {
      NOTREACHED() << "A CSIDL with no path!!!";
      pup->ClearDiskFootprints();
      return false;
    }

    const FilePathSet expanded_paths = ExpandPath(
        disk_footprints[i].csidl, base::FilePath(disk_footprints[i].path));

    std::vector<base::FilePath> matches;
    for (const auto& expanded_path : expanded_paths.file_paths())
      CollectMatchingPaths(expanded_path, &matches);
    for (const auto& file_path : matches) {
      if (AnyFileMatches(file_path, disk_footprints[i].rule, options, pup)) {
        *footprint_found = true;
        LOG(INFO) << "Found disk footprint: " << SanitizePath(file_path);
        if (options.only_one_footprint())
          return true;
      }
    }
  }
  return true;
}

bool DoesValueMatchAgainstRegistryRule(const base::string16& value,
                                       const base::string16& value_substring,
                                       RegistryMatchRule rule) {
  switch (rule) {
    case REGISTRY_VALUE_MATCH_EXACT:
      if (String16EqualsCaseInsensitive(value, value_substring))
        return true;
      break;
    case REGISTRY_VALUE_MATCH_CONTAINS:
    case REGISTRY_VALUE_MATCH_PARTIAL:
      if (String16ContainsCaseInsensitive(value, value_substring))
        return true;
      break;
    case REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT: {
      // This variable is needed to call the string constructor with size to
      // keep the null characters.
      base::string16 delimiters(PUPData::kCommonDelimiters,
                                PUPData::kCommonDelimitersLength);
      if (String16SetMatchEntry(value, delimiters, value_substring,
                                String16EqualsCaseInsensitive)) {
        return true;
      }
      break;
    }
    case REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_CONTAINS: {
      // This variable is needed to call the string constructor with size to
      // keep the null characters.
      base::string16 delimiters(PUPData::kCommonDelimiters,
                                PUPData::kCommonDelimitersLength);
      if (String16SetMatchEntry(value, delimiters, value_substring,
                                String16ContainsCaseInsensitive)) {
        return true;
      }
      break;
    }
    case REGISTRY_VALUE_MATCH_COMMA_SEPARATED_SET_EXACT:
      if (String16SetMatchEntry(value, PUPData::kCommaDelimiter,
                                value_substring,
                                String16EqualsCaseInsensitive)) {
        return true;
      }
      break;
    case REGISTRY_VALUE_MATCH_COMMA_SEPARATED_SET_CONTAINS:
      if (String16SetMatchEntry(value, PUPData::kCommaDelimiter,
                                value_substring,
                                String16ContainsCaseInsensitive)) {
        return true;
      }
      FALLTHROUGH;
    case REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_CONTAINS_PATH:
      if (String16SetMatchEntry(value, PUPData::kCommonDelimiters,
                                value_substring,
                                ShortPathContainsCaseInsensitive)) {
        return true;
      }
      break;
    default:
      LOG(ERROR) << "Missing rule to match registry key value.";
      return false;
  }

  return false;
}

// Set |footprint_found| to true when a scan and remove registry
// footprint was found. When |options.only_one_footprint()| is true, this
// function returns as
// soon as one footprint is found (or immediately if |footprint_found|
// == true). Return false if an error occurred and scanning |pup| must be
// cancelled and marked as not found.
bool ExpandRegistryFootprint(const MatchingOptions& options,
                             PUPData::PUP* pup,
                             bool* footprint_found) {
  DCHECK(pup);
  DCHECK(footprint_found);

  if (options.only_one_footprint() && *footprint_found)
    return true;

  // String format: Expanding registry footprints for: '%s'
  const std::string logging_text = base::StrCat(
      {"Expanding registry footprints for: '", PUPData::GetPUPName(pup), "'"});
  ScopedTimedTaskLogger scoped_timed_task_logger(logging_text.c_str());

  const PUPData::StaticRegistryFootprint* registry_footprints =
      pup->signature().registry_footprints;
  for (const PUPData::StaticRegistryFootprint* footprint =
           &registry_footprints[0];
       footprint->key_path != nullptr; ++footprint) {
    RegistryRoot registry_root = footprint->registry_root;
    base::FilePath policy_file;
    HKEY hkroot = nullptr;

    bool success = PUPData::GetRootKeyFromRegistryRoot(registry_root, &hkroot,
                                                       &policy_file);
    CHECK(success) << "Internal data should not have invalid registry roots.";

    // The function |CollectMatchingRegistryPaths| enumerates registry paths
    // matching the regular expression in both 32 and 64-bit view.
    std::vector<RegKeyPath> key_paths;
    CollectMatchingRegistryPaths(hkroot, footprint->key_path,
                                 PUPData::kRegistryPatternEscapeCharacter,
                                 &key_paths);

    for (const auto& key_path : key_paths) {
      base::win::RegKey reg_key;
      base::FilePath policy_file;

      if (!key_path.Open(KEY_READ, &reg_key))
        continue;
      LOG_IF(WARNING, !policy_file.empty())
          << "Group Policies are not supported.";
      DCHECK(reg_key.Valid());

      // If we are not looking for a value name, then we have already found the
      // footprint, which is simply a valid key path.
      if (footprint->rule == REGISTRY_VALUE_MATCH_KEY) {
        PUPData::DeleteRegistryKey(key_path, pup);

        LOG(INFO) << "Found registry key: " << key_path.FullPath();
        *footprint_found = true;
        if (options.only_one_footprint())
          return true;

        continue;
      }

      // Collect the matching value names from the registry key.
      std::vector<base::string16> value_names;
      CollectMatchingRegistryNames(reg_key, footprint->value_name,
                                   PUPData::kRegistryPatternEscapeCharacter,
                                   &value_names);

      for (const auto& value_name : value_names) {
        // If the value doesn't exists, do not perform any rules matching.
        if (!reg_key.HasValue(value_name.c_str()))
          continue;

        base::string16 value;
        if (!ReadRegistryValue(reg_key, value_name.c_str(), &value, nullptr,
                               nullptr)) {
          DVPLOG(1) << "Failed to read value: " << key_path.FullPath();
        }

        // When looking for a value name, collect the footprint as soon as the
        // value name is matching.
        if (footprint->rule == REGISTRY_VALUE_MATCH_VALUE_NAME) {
          DCHECK(!footprint->value_substring);
          PUPData::DeleteRegistryValue(key_path, value_name.c_str(), pup);

          LOG(INFO) << "Found registry value: " << key_path.FullPath() << ", "
                    << value_name << ", with value: " << value;
          *footprint_found = true;
          if (options.only_one_footprint())
            return true;
          continue;
        }
        DCHECK(footprint->value_substring);

        // Match against pre-defined PUPData registry rules.
        if (DoesValueMatchAgainstRegistryRule(value, footprint->value_substring,
                                              footprint->rule)) {
          PUPData::UpdateRegistryValue(key_path, value_name.c_str(),
                                       footprint->value_substring,
                                       footprint->rule, pup);

          // TODO(csharp): Sanitize footprint->value_substring which is a
          // path.
          LOG(INFO) << "Found registry value: " << key_path.FullPath() << ", "
                    << value_name << ", " << footprint->value_substring;
          *footprint_found = true;
          if (options.only_one_footprint())
            return true;

          continue;
        }
      }
    }
  }

  // No footprint found, but no errors occurred.
  return true;
}

// Runs the custom matchers to fill the fields of |pup| and set
// |footprint_found| to true when a scan and remove footprint was found.
// When |options.only_one_footprint()| is true, returns as soon as one
// footprint is found (or immediately if |footprint_found| == true).
// Returns false if an error occurred and scanning |pup| must be cancelled and
// marked as not found.
bool RunCustomMatchers(const MatchingOptions& options,
                       const SignatureMatcherAPI* signature_matcher,
                       PUPData::PUP* pup,
                       bool* footprint_found) {
  DCHECK(pup);
  DCHECK(footprint_found);

  if (options.only_one_footprint() && *footprint_found)
    return true;

  int custom_matcher_index = 0;
  const PUPData::CustomMatcher* custom_matchers =
      pup->signature().custom_matchers;
  for (const PUPData::CustomMatcher *custom_matcher = custom_matchers;
       *custom_matcher; ++custom_matcher, ++custom_matcher_index) {
    // String format: Running custom matcher %d for UwS '%s'
    const std::string logging_text = base::StrCat(
        {"Running custom matcher ", base::NumberToString(custom_matcher_index),
         " for UwS '", PUPData::GetPUPName(pup), "'"});
    ScopedTimedTaskLogger scoped_timed_task_logger(logging_text.c_str());

    // Pass a copy of |footprint_found| to ensure custom matchers don't
    // change the state from true to false.
    bool current_matcher_footprint_found = *footprint_found;
    if (!(*custom_matcher)(options, signature_matcher, pup,
                           &current_matcher_footprint_found))
      return false;

    // Try and detect if any custom matchers incorrectly set
    // |current_matcher_footprint_found| to false.
    DCHECK(!(*footprint_found && !current_matcher_footprint_found));

    if (current_matcher_footprint_found) {
      *footprint_found = true;
      if (options.only_one_footprint())
        return true;
    }
  }
  return true;
}

void LogAndWriteScanTimeToRegistry(RegistryLogger* registry_logger,
                                   const PUPData::PUP* const pup,
                                   const base::TimeDelta& scan_time) {
  DCHECK(pup);
  // String format: Scanning of: '%s'
  const std::string logging_text =
      base::StrCat({"Scanning of: '", PUPData::GetPUPName(pup), "'"});
  ScopedTimedTaskLogger::LogIfExceedThreshold(
      logging_text.c_str(), base::TimeDelta::FromSeconds(1), scan_time);

  if (registry_logger)
    registry_logger->WriteScanTime(pup->signature().id, scan_time);
}

// Scan the footprint of the PUP identified by |pup_id|, run all the collectors
// by calling |run_collectors|. If the UwS was found, call |uws_found|.
// The collectors are run via a callback to enable tests to skip running the
// collectors for performance reasons.
// |task_done| will run whenever it goes out of scope, either when this function
// ends, or when |uws_found| ends (since it is passed as an argument to it).
void ScanThisPUP(
    UwSId pup_id,
    const MatchingOptions& options,
    const SignatureMatcherAPI* signature_matcher,
    scoped_refptr<base::TaskRunner> task_runner,
    RegistryLogger* registry_logger,
    base::RepeatingCallback<void(UwSId, base::ScopedClosureRunner)> uws_found,
    base::ScopedClosureRunner task_done) {
  DCHECK(task_runner);
  DCHECK(signature_matcher);
  PUPData::PUP* pup = PUPData::GetPUP(pup_id);

  ScopedTimedTaskLogger scoped_timed_task_logger(
      base::BindOnce(&LogAndWriteScanTimeToRegistry, registry_logger, pup));

  // Make sure to start from scratch, mainly important for tests for now.
  pup->ClearDiskFootprints();
  pup->expanded_registry_footprints.clear();
  pup->expanded_scheduled_tasks.clear();

  UwSId found_pup_id = PUPData::kInvalidUwSId;
  bool footprint_found = false;

  // The functions below will return true immediately if
  // |options.only_one_footprint()| and |uws_detected| are both true.
  if (ExpandDiskFootprint(options, pup, &footprint_found) &&
      ExpandRegistryFootprint(options, pup, &footprint_found) &&
      RunCustomMatchers(options, signature_matcher, pup, &footprint_found) &&
      footprint_found) {
    found_pup_id = pup_id;
  }

  bool uws_detected = found_pup_id != PUPData::kInvalidUwSId;
  if (uws_detected) {
    UwSDetectedFlags flags = kUwSDetectedFlagsNone;

    if (options.only_one_footprint())
      flags |= kUwSDetectedFlagsOnlyOneFootprint;

    LoggingServiceAPI::GetInstance()->AddDetectedUwS(pup, flags);
  }

  if (uws_detected) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindRepeating(uws_found, found_pup_id, base::Passed(&task_done)));
  }
}

}  // namespace

UrzaScannerImpl::UrzaScannerImpl(const MatchingOptions& options,
                                 SignatureMatcherAPI* signature_matcher,
                                 RegistryLogger* registry_logger)
    : options_(options),
      signature_matcher_(signature_matcher),
      registry_logger_(registry_logger),
      all_tasks_done_(false),
      num_pups_(0),
      stopped_(false) {}

UrzaScannerImpl::~UrzaScannerImpl() {}

bool UrzaScannerImpl::Start(const FoundUwSCallback& found_uws_callback,
                            DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stopped_ = false;
  found_uws_callback_ = found_uws_callback;
  done_callback_ = std::move(done_callback);

  // It's not a valid state to have no PUPs at all.
  num_pups_ = PUPData::GetUwSIds()->size();
  DCHECK(num_pups_);

  // We currently create one task per PUP to scan.
  auto task_runner = base::CreateTaskRunnerWithTraits({base::MayBlock()});
  all_tasks_done_ = false;
  base::RepeatingClosure task_done_closure = base::BarrierClosure(
      num_pups_, base::BindOnce(&UrzaScannerImpl::InvokeAllTasksDone,
                                base::Unretained(this),
                                base::SequencedTaskRunnerHandle::Get()));
  for (const auto& pup_id : *PUPData::GetUwSIds()) {
    // The ScopedClosureRunner will be executed whenever it falls out of scope,
    // which ensures that |task_done_closure| will be executed whenever the task
    // finishes or is cancelled.
    cancelable_task_tracker_.PostTask(
        task_runner.get(), FROM_HERE,
        base::BindOnce(
            &ScanThisPUP, pup_id, options_, signature_matcher_,
            base::SequencedTaskRunnerHandle::Get(), registry_logger_,
            base::BindRepeating(&UrzaScannerImpl::UwSFound,
                                base::Unretained(this)),
            base::Passed(base::ScopedClosureRunner(task_done_closure))));
  }
  return true;
}

void UrzaScannerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelable_task_tracker_.TryCancelAll();
  stopped_ = true;
}

bool UrzaScannerImpl::IsCompletelyDone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If some cleanup code does not run we get hangs in ScannerTest.Nonexistent*
  return all_tasks_done_ && !cancelable_task_tracker_.HasTrackedTasks();
}

// |task_done| will run whenever it falls out of scoped, so it doesn't need to
// be directly called.
void UrzaScannerImpl::UwSFound(UwSId found_pup_id,
                               base::ScopedClosureRunner task_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(found_pup_id, PUPData::kInvalidUwSId);
  if (stopped_)
    return;

  found_pups_.push_back(found_pup_id);

  // Report the detected PUP.
  const PUPData::UwSSignature& signature =
      PUPData::GetPUP(found_pup_id)->signature();

  if (signature.name) {
    LoggingServiceAPI::GetInstance()->AddFoundUwS(signature.name);
    if (PUPData::HasReportOnlyFlag(signature.flags)) {
      LOG(INFO) << "Report only UwS detected: " << signature.name;
    } else {
      LOG(INFO) << "UwS detected: " << signature.name;
    }
  } else {
    LOG(ERROR) << "Missing name for detected UwS with id " << found_pup_id;
  }

  found_uws_callback_.Run(found_pup_id);
}

void UrzaScannerImpl::InvokeAllTasksDone(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UrzaScannerImpl::AllTasksDone, base::Unretained(this)));
}

void UrzaScannerImpl::AllTasksDone() {
  all_tasks_done_ = true;
  if (stopped_)
    return;

  std::move(done_callback_).Run(RESULT_CODE_SUCCESS, found_pups_);
  found_pups_.clear();
  done_callback_.Reset();
}

}  // namespace chrome_cleaner
