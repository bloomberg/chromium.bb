// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_html5fs.h"

#include <errno.h>
#include <fcntl.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_file_io.h>
#include <string.h>
#include <vector>
#include "nacl_io/mount.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

struct OutputBuffer {
  void* data;
  int element_count;
};

void* GetOutputBuffer(void* user_data, uint32_t count, uint32_t size) {
  OutputBuffer* output = static_cast<OutputBuffer*>(user_data);
  output->element_count = count;
  if (count) {
    output->data = malloc(count * size);
    if (!output->data)
      output->element_count = 0;
  } else {
    output->data = NULL;
  }
  return output->data;
}

int32_t ModeToOpenFlags(int mode) {
  int32_t open_flags = 0;

  switch (mode & 3) {
    default:
    case O_RDONLY:
      open_flags = PP_FILEOPENFLAG_READ;
      break;
    case O_WRONLY:
      open_flags = PP_FILEOPENFLAG_WRITE;
      break;
    case O_RDWR:
      open_flags = PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE;
      break;
  }

  if (mode & O_CREAT)
    open_flags |= PP_FILEOPENFLAG_CREATE;
  if (mode & O_TRUNC)
    open_flags |= PP_FILEOPENFLAG_TRUNCATE;
  if (mode & O_EXCL)
    open_flags |= PP_FILEOPENFLAG_EXCLUSIVE;

  return open_flags;
}

}  // namespace

Error MountNodeHtml5Fs::FSync() {
  // Cannot call Flush on a directory; simply do nothing.
  if (IsDirectory())
    return 0;

  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->Flush(fileio_resource_, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);
  return 0;
}

Error MountNodeHtml5Fs::GetDents(size_t offs,
                                 struct dirent* pdir,
                                 size_t size,
                                 int* out_bytes) {
  *out_bytes = 0;

  // If the buffer pointer is invalid, fail
  if (NULL == pdir)
    return EINVAL;

  // If the buffer is too small, fail
  if (size < sizeof(struct dirent))
    return EINVAL;

  // If this is not a directory, fail
  if (!IsDirectory())
    return ENOTDIR;

  OutputBuffer output_buf = {NULL, 0};
  PP_ArrayOutput output = {&GetOutputBuffer, &output_buf};
  int32_t result = mount_->ppapi()->GetFileRefInterface()->ReadDirectoryEntries(
      fileref_resource_, output, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  std::vector<struct dirent> dirents;
  PP_DirectoryEntry* entries = static_cast<PP_DirectoryEntry*>(output_buf.data);

  for (int i = 0; i < output_buf.element_count; ++i) {
    PP_Var file_name_var =
        mount_->ppapi()->GetFileRefInterface()->GetName(entries[i].file_ref);

    // Release the file reference.
    mount_->ppapi()->ReleaseResource(entries[i].file_ref);

    if (file_name_var.type != PP_VARTYPE_STRING)
      continue;

    uint32_t file_name_length;
    const char* file_name = mount_->ppapi()->GetVarInterface()
        ->VarToUtf8(file_name_var, &file_name_length);
    if (!file_name)
      continue;

    file_name_length = std::min(
        static_cast<size_t>(file_name_length),
        sizeof(static_cast<struct dirent*>(0)->d_name) - 1);  // -1 for NULL.

    dirents.push_back(dirent());
    struct dirent& direntry = dirents.back();
    direntry.d_ino = 1;  // Must be > 0.
    direntry.d_off = sizeof(struct dirent);
    direntry.d_reclen = sizeof(struct dirent);
    strncpy(direntry.d_name, file_name, file_name_length);
    direntry.d_name[file_name_length] = 0;
  }

  // Release the output buffer.
  free(output_buf.data);

  // Force size to a multiple of dirent
  size -= size % sizeof(struct dirent);
  size_t max = dirents.size() * sizeof(struct dirent);

  if (offs >= max)
    return 0;

  if (offs + size >= max)
    size = max - offs;

  memcpy(pdir, reinterpret_cast<char*>(dirents.data()) + offs, size);
  *out_bytes = size;
  return 0;
}

Error MountNodeHtml5Fs::GetStat(struct stat* stat) {
  AUTO_LOCK(node_lock_);

  PP_FileInfo info;
  int32_t result = mount_->ppapi()->GetFileRefInterface()->Query(
      fileref_resource_, &info, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  // Fill in known info here.
  memcpy(stat, &stat_, sizeof(stat_));

  // Fill in the additional info from ppapi.
  switch (info.type) {
    case PP_FILETYPE_REGULAR:
      stat->st_mode |= S_IFREG;
      break;
    case PP_FILETYPE_DIRECTORY:
      stat->st_mode |= S_IFDIR;
      break;
    case PP_FILETYPE_OTHER:
    default:
      break;
  }
  stat->st_size = static_cast<off_t>(info.size);
  stat->st_atime = info.last_access_time;
  stat->st_mtime = info.last_modified_time;
  stat->st_ctime = info.creation_time;

  return 0;
}

Error MountNodeHtml5Fs::Read(size_t offs,
                             void* buf,
                             size_t count,
                             int* out_bytes) {
  *out_bytes = 0;

  if (IsDirectory())
    return EISDIR;

  int32_t result =
      mount_->ppapi()->GetFileIoInterface()->Read(fileio_resource_,
                                                  offs,
                                                  static_cast<char*>(buf),
                                                  static_cast<int32_t>(count),
                                                  PP_BlockUntilComplete());
  if (result < 0)
    return PPErrorToErrno(result);

  *out_bytes = result;
  return 0;
}

Error MountNodeHtml5Fs::FTruncate(off_t size) {
  if (IsDirectory())
    return EISDIR;

  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->SetLength(fileio_resource_, size, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);
  return 0;
}

Error MountNodeHtml5Fs::Write(size_t offs,
                              const void* buf,
                              size_t count,
                              int* out_bytes) {
  *out_bytes = 0;

  if (IsDirectory())
    return EISDIR;

  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->Write(fileio_resource_,
              offs,
              static_cast<const char*>(buf),
              static_cast<int32_t>(count),
              PP_BlockUntilComplete());
  if (result < 0)
    return PPErrorToErrno(result);

  *out_bytes = result;
  return 0;
}

Error MountNodeHtml5Fs::GetSize(size_t* out_size) {
  *out_size = 0;

  AUTO_LOCK(node_lock_);

  PP_FileInfo info;
  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->Query(fileio_resource_, &info, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  *out_size = static_cast<size_t>(info.size);
  return 0;
}

MountNodeHtml5Fs::MountNodeHtml5Fs(Mount* mount, PP_Resource fileref_resource)
    : MountNode(mount),
      fileref_resource_(fileref_resource),
      fileio_resource_(0) {}

Error MountNodeHtml5Fs::Init(int perm) {
  Error error = MountNode::Init(Mount::OpenModeToPermission(perm));
  if (error)
    return error;

  // First query the FileRef to see if it is a file or directory.
  PP_FileInfo file_info;
  int32_t query_result =
      mount_->ppapi()->GetFileRefInterface()->Query(fileref_resource_,
                                                    &file_info,
                                                    PP_BlockUntilComplete());
  // If this is a directory, do not get a FileIO.
  if (query_result == PP_OK && file_info.type == PP_FILETYPE_DIRECTORY)
    return 0;

  fileio_resource_ = mount_->ppapi()->GetFileIoInterface()
      ->Create(mount_->ppapi()->GetInstance());
  if (!fileio_resource_)
    return ENOSYS;

  int32_t open_result =
      mount_->ppapi()->GetFileIoInterface()->Open(fileio_resource_,
                                                  fileref_resource_,
                                                  ModeToOpenFlags(perm),
                                                  PP_BlockUntilComplete());
  if (open_result != PP_OK)
    return PPErrorToErrno(open_result);
  return 0;
}

void MountNodeHtml5Fs::Destroy() {
  FSync();

  if (fileio_resource_) {
    mount_->ppapi()->GetFileIoInterface()->Close(fileio_resource_);
    mount_->ppapi()->ReleaseResource(fileio_resource_);
  }

  mount_->ppapi()->ReleaseResource(fileref_resource_);
  fileio_resource_ = 0;
  fileref_resource_ = 0;
  MountNode::Destroy();
}

}  // namespace nacl_io

