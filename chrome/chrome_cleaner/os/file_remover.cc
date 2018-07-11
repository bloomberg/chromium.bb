// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_remover.h"

#include <stdint.h>

#include <unordered_set>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/whitelisted_directory.h"

namespace chrome_cleaner {

namespace {

bool RegisterFileForPostRebootRemoval(const base::FilePath path) {
  // Don't allow directories to be deleted post-reboot. The directory will only
  // be deleted if it is empty, and we can't ensure it will be so don't worry
  // about it.
  if (base::DirectoryExists(path))
    return false;

  // MoveFileEx with MOVEFILE_DELAY_UNTIL_REBOOT flag and null destination
  // registers |file_path| to be deleted on the next system restarts.
  constexpr DWORD flags = MOVEFILE_DELAY_UNTIL_REBOOT;
  return ::MoveFileEx(path.value().c_str(), nullptr, flags) != 0;
}

void DeleteEmptyDirectories(base::FilePath directory) {
  while (
      base::DirectoryExists(directory) && base::IsDirectoryEmpty(directory) &&
      !WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(directory)) {
    // Empty directories deleted in this cleanup are not logged in the matched
    // folders list for the corresponding UwS, because they are not necessarily
    // matched by any rule by the scanner.
    VLOG(1) << "Deleting empty directory " << SanitizePath(directory);
    if (!base::DeleteFile(directory, /*recursive=*/false))
      break;
    directory = directory.DirName();
  }
}

// Sanity checks file names to ensure they don't contain ".." or specify a
// drive root.
bool IsSafeNameForDeletion(const base::FilePath& path) {
  // Empty path isn't allowed
  if (path.empty())
    return false;

  const base::string16& path_str = path.value();
  // Disallow anything with "\..\".
  if (path_str.find(L"\\..\\") != base::string16::npos)
    return false;

  // Ensure the path does not specify a drive root: require a character other
  // than \/:. after the last :
  size_t last_colon_pos = path_str.rfind(L':');
  if (last_colon_pos == base::string16::npos)
    return true;
  for (size_t index = last_colon_pos + 1; index < path_str.size(); ++index) {
    wchar_t character = path_str[index];
    if (character != L'\\' && character != L'/' && character != L'.')
      return true;
  }
  return false;
}

}  // namespace

FileRemover::FileRemover(std::shared_ptr<DigestVerifier> digest_verifier,
                         const LayeredServiceProviderAPI& lsp,
                         const FilePathSet& deletion_allowed_paths,
                         base::RepeatingClosure reboot_needed_callback)
    : digest_verifier_(digest_verifier),
      deletion_allowed_paths_(deletion_allowed_paths),
      reboot_needed_callback_(reboot_needed_callback) {
  LSPPathToGUIDs providers;
  GetLayeredServiceProviders(lsp, &providers);
  for (const auto& provider : providers)
    deletion_forbidden_paths_.Insert(provider.first);
  deletion_forbidden_paths_.Insert(
      PreFetchedPaths::GetInstance()->GetExecutablePath());
}

FileRemover::~FileRemover() = default;

bool FileRemover::RemoveNow(const base::FilePath& path) const {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  switch (CanRemove(path)) {
    case DeletionValidationStatus::FORBIDDEN:
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
      return false;
    case DeletionValidationStatus::INACTIVE:
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_NOT_REMOVED_INACTIVE_EXTENSION);
      return true;
    case DeletionValidationStatus::ALLOWED:
      // No-op. Proceed to removal.
      break;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (!base::PathExists(path)) {
    removal_status_updater->UpdateRemovalStatus(path, REMOVAL_STATUS_NOT_FOUND);
    return true;
  }
  if (!base::DeleteFile(path, /*recursive=*/false)) {
    // If the attempt to delete the file fails, propagate the failure as
    // normal so that the engine knows about it and can try a backup action,
    // but also register the file for post-reboot removal in case the engine
    // doesn't have any effective backup action.
    //
    // A potential downside to this implementation is that if the file is
    // later successfully deleted, we might ask users to reboot when no
    // reboot is needed.
    if (RegisterFileForPostRebootRemoval(path)) {
      reboot_needed_callback_.Run();
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK);
    } else {
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_FAILED_TO_REMOVE);
    }
    return false;
  }
  removal_status_updater->UpdateRemovalStatus(path, REMOVAL_STATUS_REMOVED);
  DeleteEmptyDirectories(path.DirName());
  return true;
}

bool FileRemover::RegisterPostRebootRemoval(
    const base::FilePath& file_path) const {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  switch (CanRemove(file_path)) {
    case DeletionValidationStatus::FORBIDDEN:
      removal_status_updater->UpdateRemovalStatus(
          file_path, REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
      return false;
    case DeletionValidationStatus::INACTIVE:
      removal_status_updater->UpdateRemovalStatus(
          file_path, REMOVAL_STATUS_NOT_REMOVED_INACTIVE_EXTENSION);
      return true;
    case DeletionValidationStatus::ALLOWED:
      // No-op. Proceed to removal.
      break;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (!base::PathExists(file_path)) {
    removal_status_updater->UpdateRemovalStatus(file_path,
                                                REMOVAL_STATUS_NOT_FOUND);
    return true;
  }
  if (!RegisterFileForPostRebootRemoval(file_path)) {
    PLOG(ERROR) << "Failed to schedule delete file: "
                << chrome_cleaner::SanitizePath(file_path);
    removal_status_updater->UpdateRemovalStatus(
        file_path, REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL);
    return false;
  }
  reboot_needed_callback_.Run();
  removal_status_updater->UpdateRemovalStatus(
      file_path, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  return true;
}

FileRemoverAPI::DeletionValidationStatus FileRemover::CanRemove(
    const base::FilePath& file) const {
  // Allow removing of all files if |digest_verifier_| is unavailable.
  // Otherwise, allow removing only files unknown to |digest_verifier_|.
  if (digest_verifier_ && digest_verifier_->IsKnownFile(file)) {
    LOG(ERROR) << "Cannot remove known file " << SanitizePath(file);
    return DeletionValidationStatus::FORBIDDEN;
  }
  if (WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(file)) {
    // We are logging the path in both sanitized and non-sanitized form since
    // this should never happen unless we are breaking something, in which
    // case we will need precise information.
    LOG(ERROR) << "Cannot remove a CSIDL path: " << file.value()
               << "', sanitized: '" << SanitizePath(file);
    return DeletionValidationStatus::FORBIDDEN;
  }
  return IsFileRemovalAllowed(file, deletion_allowed_paths_,
                              deletion_forbidden_paths_);
}

// static
FileRemoverAPI::DeletionValidationStatus FileRemover::IsFileRemovalAllowed(
    const base::FilePath& file_path,
    const FilePathSet& allow_deletion,
    const FilePathSet& forbid_deletion) {
  if (!IsSafeNameForDeletion(file_path) || !file_path.IsAbsolute() ||
      forbid_deletion.Contains(file_path)) {
    return DeletionValidationStatus::FORBIDDEN;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (base::DirectoryExists(file_path))
    return DeletionValidationStatus::FORBIDDEN;

  // If the file was blacklisted, allow its deletion regardless of the extension
  if (allow_deletion.Contains(file_path))
    return DeletionValidationStatus::ALLOWED;

  // Allow deletion of files with active (i.e. executable) extensions, files
  // with explicit alternate file streams specified and files with DOS
  // executable headers regardless of the extension.
  if (chrome_cleaner::PathHasActiveExtension(file_path) ||
      chrome_cleaner::HasAlternateFileStream(file_path) ||
      chrome_cleaner::HasDosExecutableHeader(file_path)) {
    return DeletionValidationStatus::ALLOWED;
  }

  // If this line is reached, the file has a non-executable file extension.
  LOG(ERROR) << "Cannot delete non-executable file with extension '"
             << file_path.Extension() << "'. Full path '"
             << chrome_cleaner::SanitizePath(file_path) << "'";
  return DeletionValidationStatus::INACTIVE;
}

}  // namespace chrome_cleaner
