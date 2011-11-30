// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/secure_hash.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>

#include "content/browser/safe_util_win.h"
#elif defined(OS_MACOSX)
#include "content/browser/file_metadata_mac.h"
#endif

using content::BrowserThread;

namespace {

#define LOG_ERROR(o, e)  LogError(__FILE__, __LINE__, __FUNCTION__, o, e)

// Logs the value and passes error on through, converting to a |net::Error|.
// Returns |ERR_UNEXPECTED| if the value is not in the enum.
net::Error LogError(const char* file, int line, const char* func,
                    const char* operation, int error) {
  const char* err_string = "";
  net::Error net_error = net::OK;

#define NET_ERROR(label, value)  \
  case net::ERR_##label: \
    err_string = #label; \
    net_error = net::ERR_##label; \
    break;

  switch (error) {
    case net::OK:
      return net::OK;

#include "net/base/net_error_list.h"

    default:
      err_string = "Unexpected enum value";
      net_error = net::ERR_UNEXPECTED;
      break;
  }

#undef NET_ERROR

  DVLOG(1) << " " << func << "(): " << operation
           << "() returned error " << error << " (" << err_string << ")";

  return net_error;
}

#if defined(OS_WIN)

#define SHFILE_TO_NET_ERROR(symbol, value, mapping, description) \
  case value: return net::ERR_##mapping;

// Maps the result of a call to |SHFileOperation()| onto a |net::Error|.
//
// These return codes are *old* (as in, DOS era), and specific to
// |SHFileOperation()|.
// They do not appear in any windows header.
//
// See http://msdn.microsoft.com/en-us/library/bb762164(VS.85).aspx.
net::Error MapShFileOperationCodes(int code) {
  // Check these pre-Win32 error codes first, then check for matches
  // in Winerror.h.

  switch (code) {
    // Error Code, Value, Platform Error Mapping, Meaning
    SHFILE_TO_NET_ERROR(DE_SAMEFILE, 0x71, FILE_EXISTS,
        "The source and destination files are the same file.")
    SHFILE_TO_NET_ERROR(DE_OPCANCELLED, 0x75, ABORTED,
        "The operation was canceled by the user, or silently canceled if "
        "the appropriate flags were supplied to SHFileOperation.")
    SHFILE_TO_NET_ERROR(DE_ACCESSDENIEDSRC, 0x78, ACCESS_DENIED,
        "Security settings denied access to the source.")
    SHFILE_TO_NET_ERROR(DE_PATHTOODEEP, 0x79, FILE_PATH_TOO_LONG,
        "The source or destination path exceeded or would exceed MAX_PATH.")
    SHFILE_TO_NET_ERROR(DE_INVALIDFILES, 0x7C, FILE_NOT_FOUND,
        "The path in the source or destination or both was invalid.")
    SHFILE_TO_NET_ERROR(DE_FLDDESTISFILE, 0x7E, FILE_EXISTS,
        "The destination path is an existing file.")
    SHFILE_TO_NET_ERROR(DE_FILEDESTISFLD, 0x80, FILE_EXISTS,
        "The destination path is an existing folder.")
    SHFILE_TO_NET_ERROR(DE_FILENAMETOOLONG, 0x81, FILE_PATH_TOO_LONG,
        "The name of the file exceeds MAX_PATH.")
    SHFILE_TO_NET_ERROR(DE_DEST_IS_CDROM, 0x82, ACCESS_DENIED,
        "The destination is a read-only CD-ROM, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_DEST_IS_DVD, 0x83, ACCESS_DENIED,
        "The destination is a read-only DVD, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_DEST_IS_CDRECORD, 0x84, ACCESS_DENIED,
        "The destination is a writable CD-ROM, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_FILE_TOO_LARGE, 0x85, FILE_TOO_BIG,
        "The file involved in the operation is too large for the destination "
        "media or file system.")
    SHFILE_TO_NET_ERROR(DE_SRC_IS_CDROM, 0x86, ACCESS_DENIED,
        "The source is a read-only CD-ROM, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_SRC_IS_DVD, 0x87, ACCESS_DENIED,
        "The source is a read-only DVD, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_SRC_IS_CDRECORD, 0x88, ACCESS_DENIED,
        "The source is a writable CD-ROM, possibly unformatted.")
    SHFILE_TO_NET_ERROR(DE_ERROR_MAX, 0xB7, FILE_PATH_TOO_LONG,
        "MAX_PATH was exceeded during the operation.")
    SHFILE_TO_NET_ERROR(XE_ERRORONDEST, 0x10000, UNEXPECTED,
        "An unspecified error occurred on the destination.")

    // These are not expected to occur for in our usage.
    SHFILE_TO_NET_ERROR(DE_MANYSRC1DEST, 0x72, FAILED,
        "Multiple file paths were specified in the source buffer, "
        "but only one destination file path.")
    SHFILE_TO_NET_ERROR(DE_DIFFDIR, 0x73, FAILED,
        "Rename operation was specified but the destination path is "
        "a different directory. Use the move operation instead.")
    SHFILE_TO_NET_ERROR(DE_ROOTDIR, 0x74, FAILED,
        "The source is a root directory, which cannot be moved or renamed.")
    SHFILE_TO_NET_ERROR(DE_DESTSUBTREE, 0x76, FAILED,
        "The destination is a subtree of the source.")
    SHFILE_TO_NET_ERROR(DE_MANYDEST, 0x7A, FAILED,
        "The operation involved multiple destination paths, "
        "which can fail in the case of a move operation.")
    SHFILE_TO_NET_ERROR(DE_DESTSAMETREE, 0x7D, FAILED,
        "The source and destination have the same parent folder.")
    SHFILE_TO_NET_ERROR(DE_UNKNOWN_ERROR, 0x402, FAILED,
        "An unknown error occurred. "
        "This is typically due to an invalid path in the source or destination."
        " This error does not occur on Windows Vista and later.")
    SHFILE_TO_NET_ERROR(DE_ROOTDIR | ERRORONDEST, 0x10074, FAILED,
        "Destination is a root directory and cannot be renamed.")
    default:
      break;
  }

  // If not one of the above codes, it should be a standard Windows error code.
  return static_cast<net::Error>(net::MapSystemError(code));
}

#undef SHFILE_TO_NET_ERROR

// Renames a file using the SHFileOperation API to ensure that the target file
// gets the correct default security descriptor in the new path.
// Returns a network error, or net::OK for success.
net::Error RenameFileAndResetSecurityDescriptor(
    const FilePath& source_file_path,
    const FilePath& target_file_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // The parameters to SHFileOperation must be terminated with 2 NULL chars.
  std::wstring source = source_file_path.value();
  std::wstring target = target_file_path.value();

  source.append(1, L'\0');
  target.append(1, L'\0');

  SHFILEOPSTRUCT move_info = {0};
  move_info.wFunc = FO_MOVE;
  move_info.pFrom = source.c_str();
  move_info.pTo = target.c_str();
  move_info.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI |
                     FOF_NOCONFIRMMKDIR | FOF_NOCOPYSECURITYATTRIBS;

  int result = SHFileOperation(&move_info);

  if (result == 0)
    return net::OK;

  return MapShFileOperationCodes(result);
}

#endif

}  // namespace

// This will initialize the entire array to zero.
const unsigned char BaseFile::kEmptySha256Hash[] = { 0 };

BaseFile::BaseFile(const FilePath& full_path,
                   const GURL& source_url,
                   const GURL& referrer_url,
                   int64 received_bytes,
                   const linked_ptr<net::FileStream>& file_stream)
    : full_path_(full_path),
      source_url_(source_url),
      referrer_url_(referrer_url),
      file_stream_(file_stream),
      bytes_so_far_(received_bytes),
      start_tick_(base::TimeTicks::Now()),
      power_save_blocker_(PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep),
      calculate_hash_(false),
      detached_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  memcpy(sha256_hash_, kEmptySha256Hash, kSha256HashLen);
  if (file_stream_.get())
    file_stream_->EnableErrorStatistics();
}

BaseFile::~BaseFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (detached_)
    Close();
  else
    Cancel();  // Will delete the file.
}

net::Error BaseFile::Initialize(bool calculate_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  calculate_hash_ = calculate_hash;

  if (calculate_hash_)
    secure_hash_.reset(crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  if (full_path_.empty()) {
    FilePath temp_file;
    FilePath download_dir =
        content::GetContentClient()->browser()->GetDefaultDownloadDirectory();
    if (!file_util::CreateTemporaryFileInDir(download_dir, &temp_file) &&
        !file_util::CreateTemporaryFile(&temp_file)) {
      return LOG_ERROR("unable to create", net::ERR_FILE_NOT_FOUND);
    }
    full_path_ = temp_file;
  }

  return Open();
}

net::Error BaseFile::AppendDataToFile(const char* data, size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  // NOTE(benwells): The above DCHECK won't be present in release builds,
  // so we log any occurences to see how common this error is in the wild.
  if (detached_)
    download_stats::RecordDownloadCount(
        download_stats::APPEND_TO_DETACHED_FILE_COUNT);

  if (!file_stream_.get())
    return LOG_ERROR("get", net::ERR_INVALID_HANDLE);

  // TODO(phajdan.jr): get rid of this check.
  if (data_len == 0)
    return net::OK;

  // The Write call below is not guaranteed to write all the data.
  size_t write_count = 0;
  size_t len = data_len;
  const char* current_data = data;
  while (len > 0) {
    write_count++;
    int write_result =
        file_stream_->Write(current_data, len, net::CompletionCallback());
    DCHECK_NE(0, write_result);

    // Check for errors.
    if (static_cast<size_t>(write_result) != data_len) {
      // We should never get ERR_IO_PENDING, as the Write above is synchronous.
      DCHECK_NE(net::ERR_IO_PENDING, write_result);

      // Report errors on file writes.
      if (write_result < 0)
        return LOG_ERROR("Write", write_result);
    }

    // Update status.
    size_t write_size = static_cast<size_t>(write_result);
    DCHECK_LE(write_size, len);
    len -= write_size;
    current_data += write_size;
    bytes_so_far_ += write_size;
  }

  // TODO(ahendrickson) -- Uncomment these when the functions are available.
   download_stats::RecordDownloadWriteSize(data_len);
   download_stats::RecordDownloadWriteLoopCount(write_count);

  if (calculate_hash_)
    secure_hash_->Update(data, data_len);

  return net::OK;
}

net::Error BaseFile::Rename(const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool saved_in_progress = in_progress();

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_) {
    // Don't close the file if we're not done (finished or canceled).
    if (!saved_in_progress)
      Close();

    return net::OK;
  }

  Close();

  file_util::CreateDirectory(new_path.DirName());

#if defined(OS_WIN)
  // We cannot rename because rename will keep the same security descriptor
  // on the destination file. We want to recreate the security descriptor
  // with the security that makes sense in the new path.
  // |RenameFileAndResetSecurityDescriptor| returns a windows-specific
  // error, whch we must translate into a net::Error.
  net::Error rename_err =
      RenameFileAndResetSecurityDescriptor(full_path_, new_path);
  if (rename_err != net::OK)
    return LOG_ERROR("RenameFileAndResetSecurityDescriptor", rename_err);
#elif defined(OS_POSIX)
  {
    // Similarly, on Unix, we're moving a temp file created with permissions
    // 600 to |new_path|. Here, we try to fix up the destination file with
    // appropriate permissions.
    struct stat st;
    // First check the file existence and create an empty file if it doesn't
    // exist.
    if (!file_util::PathExists(new_path)) {
      int write_error = file_util::WriteFile(new_path, "", 0);
      if (write_error < 0)
        return LOG_ERROR("WriteFile", net::MapSystemError(errno));
    }
    int stat_error = stat(new_path.value().c_str(), &st);
    bool stat_succeeded = (stat_error == 0);
    if (!stat_succeeded)
      LOG_ERROR("stat", net::MapSystemError(errno));

    // TODO(estade): Move() falls back to copying and deleting when a simple
    // rename fails. Copying sucks for large downloads. crbug.com/8737
    if (!file_util::Move(full_path_, new_path))
      return LOG_ERROR("Move", net::MapSystemError(errno));

    if (stat_succeeded) {
      // On Windows file systems (FAT, NTFS), chmod fails.  This is OK.
      int chmod_error = chmod(new_path.value().c_str(), st.st_mode);
      if (chmod_error < 0)
        LOG_ERROR("chmod", net::MapSystemError(errno));
    }
  }
#endif

  full_path_ = new_path;

  // We don't need to re-open the file if we're done (finished or canceled).
  if (!saved_in_progress)
    return net::OK;

  return Open();
}

void BaseFile::Detach() {
  detached_ = true;
}

void BaseFile::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  Close();

  if (!full_path_.empty())
    file_util::Delete(full_path_, false);
}

void BaseFile::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (calculate_hash_)
    secure_hash_->Finish(sha256_hash_, kSha256HashLen);

  Close();
}

bool BaseFile::GetSha256Hash(std::string* hash) {
  DCHECK(!detached_);
  hash->assign(reinterpret_cast<const char*>(sha256_hash_),
               sizeof(sha256_hash_));
  return (calculate_hash_ && !in_progress());
}

bool BaseFile::IsEmptySha256Hash(const std::string& hash) {
  return (hash.size() == kSha256HashLen &&
          0 == memcmp(hash.data(), kEmptySha256Hash, sizeof(kSha256HashLen)));
}

void BaseFile::AnnotateWithSourceInformation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

#if defined(OS_WIN)
  // Sets the Zone to tell Windows that this file comes from the internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_,
                                      UTF8ToWide(source_url_.spec()));
#elif defined(OS_MACOSX)
  file_metadata::AddQuarantineMetadataToFile(full_path_, source_url_,
                                             referrer_url_);
  file_metadata::AddOriginMetadataToFile(full_path_, source_url_,
                                         referrer_url_);
#endif
}

void BaseFile::CreateFileStream() {
  file_stream_.reset(new net::FileStream);
}

net::Error BaseFile::Open() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  // Create a new file stream if it is not provided.
  if (!file_stream_.get()) {
    CreateFileStream();
    file_stream_->EnableErrorStatistics();
    int open_result = file_stream_->Open(
        full_path_,
        base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE);
    if (open_result != net::OK) {
      file_stream_.reset();
      return LOG_ERROR("Open", open_result);
    }

    // We may be re-opening the file after rename. Always make sure we're
    // writing at the end of the file.
    int64 seek_result = file_stream_->Seek(net::FROM_END, 0);
    if (seek_result < 0) {
      file_stream_.reset();
      return LOG_ERROR("Seek", seek_result);
    }
  }

#if defined(OS_WIN)
  AnnotateWithSourceInformation();
#endif
  return net::OK;
}

void BaseFile::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (file_stream_.get()) {
#if defined(OS_CHROMEOS)
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_stream_->Flush();
#endif
    file_stream_->Close();
    file_stream_.reset();
  }
}

std::string BaseFile::DebugString() const {
  return base::StringPrintf("{ source_url_ = \"%s\""
                            " full_path_ = \"%" PRFilePath "\""
                            " bytes_so_far_ = %" PRId64
                            " detached_ = %c }",
                            source_url_.spec().c_str(),
                            full_path_.value().c_str(),
                            bytes_so_far_,
                            detached_ ? 'T' : 'F');
}

int64 BaseFile::CurrentSpeedAtTime(base::TimeTicks current_time) const {
  base::TimeDelta diff = current_time - start_tick_;
  int64 diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : bytes_so_far() * 1000 / diff_ms;
}

int64 BaseFile::CurrentSpeed() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return CurrentSpeedAtTime(base::TimeTicks::Now());
}
