// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_metrics.h"

#include <windows.h>  // NOLINT
#include <atlsecurity.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// This is duplicated from components/metrics/file_metrics_provider.h which is
// not accessible from setup code.
const base::FilePath::CharType MetricsFileExtension[] =
    FILE_PATH_LITERAL(".pma");

// Add to the ACL of an object on disk. This follows the method from MSDN:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa379283.aspx
// This is done using explicit flags rather than the "security string" format
// because strings do not necessarily read what is written which makes it
// difficult to de-dup. Working with the binary format is always exact and
// the system libraries will properly ignore duplicate ACL entries.
bool AddAclToPath(const base::FilePath& path,
                  const CSid& trustee,
                  ACCESS_MASK access_mask,
                  BYTE ace_flags) {
  DCHECK(!path.empty());
  DCHECK(trustee);

  // Get the existing DACL.
  ATL::CDacl dacl;
  if (!ATL::AtlGetDacl(path.value().c_str(), SE_FILE_OBJECT, &dacl)) {
    DPLOG(ERROR) << "Failed getting DACL for path \"" << path.value() << "\"";
    return false;
  }

  // Check if the requested access already exists and return if so.
  for (UINT i = 0; i < dacl.GetAceCount(); ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    BYTE type = 0;
    BYTE flags = 0;
    dacl.GetAclEntry(i, &sid, &mask, &type, &flags);
    if (sid == trustee && type == ACCESS_ALLOWED_ACE_TYPE &&
        (flags & ace_flags) == ace_flags &&
        (mask & access_mask) == access_mask) {
      return true;
    }
  }

  // Add the new access to the DACL.
  if (!dacl.AddAllowedAce(trustee, access_mask, ace_flags)) {
    DPLOG(ERROR) << "Failed adding ACE to DACL";
    return false;
  }

  // Attach the updated ACL as the object's DACL.
  if (!ATL::AtlSetDacl(path.value().c_str(), SE_FILE_OBJECT, dacl)) {
    DPLOG(ERROR) << "Failed setting DACL for path \"" << path.value() << "\"";
    return false;
  }

  return true;
}

}  // namespace

base::FilePath GetPersistentHistogramStorageDir(
    const base::FilePath& target_path) {
  return target_path.AppendASCII(kSetupHistogramAllocatorName);
}

void BeginPersistentHistogramStorage() {
  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      1 << 20,  // 1 MiB
      0,        // No identifier.
      installer::kSetupHistogramAllocatorName);
  base::GlobalHistogramAllocator::Get()->CreateTrackingHistograms(
      kSetupHistogramAllocatorName);

  // This can't be enabled until after the allocator is configured because
  // there is no other reporting out of setup other than persistent memory.
  base::HistogramBase::EnableActivityReportHistogram("setup");
}

void EndPersistentHistogramStorage(const base::FilePath& target_path,
                                   bool system_install) {
  base::PersistentHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  allocator->UpdateTrackingHistograms();

  // Allocator dumps are saved to a directory that was created earlier. Stop
  // now if that directory has been removed because an uninstall has happened.
  base::FilePath dir_path = GetPersistentHistogramStorageDir(target_path);
  if (!base::DirectoryExists(dir_path))
    return;

  // Set permissions on the directory that allows other processes, including
  // non-privileged ones, to read and delete the files stored there. This
  // allows the browser process to remove the metrics files once it's done
  // reading them. This is only done for system-level installs; user-level
  // installs already provide delete-file access to the browser process.
  if (system_install) {
    if (!AddAclToPath(dir_path, ATL::Sids::AuthenticatedUser(),
                      FILE_GENERIC_READ | FILE_DELETE_CHILD,
                      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)) {
      PLOG(ERROR) << "Could not set \"delete\" permission for metrics directory"
                  << " \"" << dir_path.value() << "\"";
    }
  }

  // Remove any existing metrics file at the old (non-subdir) pathname.
  base::DeleteFile(dir_path.AddExtension(MetricsFileExtension), false);

  // Save data using the current time as the filename. The actual filename
  // doesn't matter (so long as it ends with the correct extension) but this
  // works as well as anything.
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  base::FilePath file_path =
      dir_path
          .AppendASCII(base::StringPrintf("%04d%02d%02d%02d%02d%02d",
                                          exploded.year, exploded.month,
                                          exploded.day_of_month, exploded.hour,
                                          exploded.minute, exploded.second))
          .AddExtension(MetricsFileExtension);

  base::StringPiece contents(static_cast<const char*>(allocator->data()),
                             allocator->used());
  if (base::ImportantFileWriter::WriteFileAtomically(file_path, contents))
    VLOG(1) << "Persistent histograms saved in file: " << file_path.value();
}

}  // namespace installer
