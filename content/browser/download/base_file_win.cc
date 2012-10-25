// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include <windows.h>
#include <shellapi.h>

#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/safe_util_win.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Maps the result of a call to |SHFileOperation()| onto a
// |content::DownloadInterruptReason|.
//
// These return codes are *old* (as in, DOS era), and specific to
// |SHFileOperation()|.
// They do not appear in any windows header.
//
// See http://msdn.microsoft.com/en-us/library/bb762164(VS.85).aspx.
content::DownloadInterruptReason MapShFileOperationCodes(int code) {
  // Check these pre-Win32 error codes first, then check for matches
  // in Winerror.h.

  switch (code) {
    // The source and destination files are the same file.
    // DE_SAMEFILE == 0x71
    case 0x71: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The operation was canceled by the user, or silently canceled if the
    // appropriate flags were supplied to SHFileOperation.
    // DE_OPCANCELLED == 0x75
    case 0x75: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // Security settings denied access to the source.
    // DE_ACCESSDENIEDSRC == 0x78
    case 0x78: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The source or destination path exceeded or would exceed MAX_PATH.
    // DE_PATHTOODEEP == 0x79
    case 0x79: return content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;

    // The path in the source or destination or both was invalid.
    // DE_INVALIDFILES == 0x7C
    case 0x7C: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The destination path is an existing file.
    // DE_FLDDESTISFILE == 0x7E
    case 0x7E: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The destination path is an existing folder.
    // DE_FILEDESTISFLD == 0x80
    case 0x80: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The name of the file exceeds MAX_PATH.
    // DE_FILENAMETOOLONG == 0x81
    case 0x81: return content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;

    // The destination is a read-only CD-ROM, possibly unformatted.
    // DE_DEST_IS_CDROM == 0x82
    case 0x82: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The destination is a read-only DVD, possibly unformatted.
    // DE_DEST_IS_DVD == 0x83
    case 0x83: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The destination is a writable CD-ROM, possibly unformatted.
    // DE_DEST_IS_CDRECORD == 0x84
    case 0x84: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The file involved in the operation is too large for the destination
    // media or file system.
    // DE_FILE_TOO_LARGE == 0x85
    case 0x85: return content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE;

    // The source is a read-only CD-ROM, possibly unformatted.
    // DE_SRC_IS_CDROM == 0x86
    case 0x86: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The source is a read-only DVD, possibly unformatted.
    // DE_SRC_IS_DVD == 0x87
    case 0x87: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // The source is a writable CD-ROM, possibly unformatted.
    // DE_SRC_IS_CDRECORD == 0x88
    case 0x88: return content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // MAX_PATH was exceeded during the operation.
    // DE_ERROR_MAX == 0xB7
    case 0xB7: return content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;

    // An unspecified error occurred on the destination.
    // XE_ERRORONDEST == 0x10000
    case 0x10000: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // Multiple file paths were specified in the source buffer, but only one
    // destination file path.
    // DE_MANYSRC1DEST == 0x72
    case 0x72: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // Rename operation was specified but the destination path is
    // a different directory. Use the move operation instead.
    // DE_DIFFDIR == 0x73
    case 0x73: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The source is a root directory, which cannot be moved or renamed.
    // DE_ROOTDIR == 0x74
    case 0x74: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The destination is a subtree of the source.
    // DE_DESTSUBTREE == 0x76
    case 0x76: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The operation involved multiple destination paths,
    // which can fail in the case of a move operation.
    // DE_MANYDEST == 0x7A
    case 0x7A: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // The source and destination have the same parent folder.
    // DE_DESTSAMETREE == 0x7D
    case 0x7D: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // An unknown error occurred.  This is typically due to an invalid path in
    // the source or destination.  This error does not occur on Windows Vista
    // and later.
    // DE_UNKNOWN_ERROR == 0x402
    case 0x402: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    // Destination is a root directory and cannot be renamed.
    // DE_ROOTDIR | ERRORONDEST == 0x10074
    case 0x10074: return content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  }

  // If not one of the above codes, it should be a standard Windows error code.
  return content::ConvertNetErrorToInterruptReason(
      net::MapSystemError(code), content::DOWNLOAD_INTERRUPT_FROM_DISK);
}

} // namespace

// Renames a file using the SHFileOperation API to ensure that the target file
// gets the correct default security descriptor in the new path.
// Returns a network error, or net::OK for success.
content::DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const FilePath& new_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // The parameters to SHFileOperation must be terminated with 2 NULL chars.
  FilePath::StringType source = full_path_.value();
  FilePath::StringType target = new_path.value();

  source.append(1, L'\0');
  target.append(1, L'\0');

  SHFILEOPSTRUCT move_info = {0};
  move_info.wFunc = FO_MOVE;
  move_info.pFrom = source.c_str();
  move_info.pTo = target.c_str();
  move_info.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI |
      FOF_NOCONFIRMMKDIR | FOF_NOCOPYSECURITYATTRIBS;

  int result = SHFileOperation(&move_info);
  content::DownloadInterruptReason interrupt_reason =
      content::DOWNLOAD_INTERRUPT_REASON_NONE;

  if (result == 0 && move_info.fAnyOperationsAborted)
    interrupt_reason = content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  else if (result != 0)
    interrupt_reason = MapShFileOperationCodes(result);

  if (interrupt_reason != content::DOWNLOAD_INTERRUPT_REASON_NONE)
    return LogInterruptReason("SHFileOperation", result, interrupt_reason);
  return interrupt_reason;
}

void BaseFile::AnnotateWithSourceInformation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(!detached_);

  // Sets the Zone to tell Windows that this file comes from the internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_,
                                      UTF8ToWide(source_url_.spec()));
}
