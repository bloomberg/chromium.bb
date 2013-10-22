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

#include "nacl_io/getdents_helper.h"
#include "nacl_io/kernel_handle.h"
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

int32_t OpenFlagsToPPAPIOpenFlags(int open_flags) {
  int32_t ppapi_flags = 0;

  switch (open_flags & 3) {
    default:
    case O_RDONLY:
      ppapi_flags = PP_FILEOPENFLAG_READ;
      break;
    case O_WRONLY:
      ppapi_flags = PP_FILEOPENFLAG_WRITE;
      break;
    case O_RDWR:
      ppapi_flags = PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE;
      break;
  }

  if (open_flags & O_CREAT)
    ppapi_flags |= PP_FILEOPENFLAG_CREATE;
  if (open_flags & O_TRUNC)
    ppapi_flags |= PP_FILEOPENFLAG_TRUNCATE;
  if (open_flags & O_EXCL)
    ppapi_flags |= PP_FILEOPENFLAG_EXCLUSIVE;

  return ppapi_flags;
}

}  // namespace

Error MountNodeHtml5Fs::FSync() {
  // Cannot call Flush on a directory; simply do nothing.
  if (IsaDir())
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

  // If this is not a directory, fail
  if (!IsaDir())
    return ENOTDIR;

  // TODO(binji): Better handling of ino numbers.
  const ino_t kCurDirIno = -1;
  const ino_t kParentDirIno = -2;
  GetDentsHelper helper(kCurDirIno, kParentDirIno);

  OutputBuffer output_buf = {NULL, 0};
  PP_ArrayOutput output = {&GetOutputBuffer, &output_buf};
  int32_t result = mount_->ppapi()->GetFileRefInterface()->ReadDirectoryEntries(
      fileref_resource_, output, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

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

    if (file_name) {
      file_name_length = std::min(
          static_cast<size_t>(file_name_length),
          MEMBER_SIZE(dirent, d_name) - 1);  // -1 for NULL.

      // TODO(binji): Better handling of ino numbers.
      helper.AddDirent(1, file_name, file_name_length);
    }

    mount_->ppapi()->GetVarInterface()->Release(file_name_var);
  }

  // Release the output buffer.
  free(output_buf.data);

  return helper.GetDents(offs, pdir, size, out_bytes);
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

Error MountNodeHtml5Fs::Read(const HandleAttr& attr,
                             void* buf,
                             size_t count,
                             int* out_bytes) {
  *out_bytes = 0;

  if (IsaDir())
    return EISDIR;

  int32_t result =
      mount_->ppapi()->GetFileIoInterface()->Read(fileio_resource_,
                                                  attr.offs,
                                                  static_cast<char*>(buf),
                                                  static_cast<int32_t>(count),
                                                  PP_BlockUntilComplete());
  if (result < 0)
    return PPErrorToErrno(result);

  *out_bytes = result;
  return 0;
}

Error MountNodeHtml5Fs::FTruncate(off_t size) {
  if (IsaDir())
    return EISDIR;

  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->SetLength(fileio_resource_, size, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);
  return 0;
}

Error MountNodeHtml5Fs::Write(const HandleAttr& attr,
                              const void* buf,
                              size_t count,
                              int* out_bytes) {
  *out_bytes = 0;

  if (IsaDir())
    return EISDIR;

  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->Write(fileio_resource_,
              attr.offs,
              static_cast<const char*>(buf),
              static_cast<int32_t>(count),
              PP_BlockUntilComplete());
  if (result < 0)
    return PPErrorToErrno(result);

  *out_bytes = result;
  return 0;
}

int MountNodeHtml5Fs::GetType() {
  return fileio_resource_ ? S_IFREG : S_IFDIR;
}

Error MountNodeHtml5Fs::GetSize(size_t* out_size) {
  *out_size = 0;

  if (IsaDir())
    return 0;

  AUTO_LOCK(node_lock_);

  PP_FileInfo info;
  int32_t result = mount_->ppapi()->GetFileIoInterface()
      ->Query(fileio_resource_, &info, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  *out_size = static_cast<size_t>(info.size);
  return 0;
}

bool MountNodeHtml5Fs::IsaDir() {
  return !fileio_resource_;
}

bool MountNodeHtml5Fs::IsaFile() {
  return fileio_resource_;
}

MountNodeHtml5Fs::MountNodeHtml5Fs(Mount* mount, PP_Resource fileref_resource)
    : MountNode(mount),
      fileref_resource_(fileref_resource),
      fileio_resource_(0) {}

Error MountNodeHtml5Fs::Init(int open_flags) {
  Error error = MountNode::Init(open_flags);
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

  FileIoInterface* file_io = mount_->ppapi()->GetFileIoInterface();
  fileio_resource_ = file_io->Create(mount_->ppapi()->GetInstance());
  if (!fileio_resource_)
    return ENOSYS;

  int32_t open_result = file_io->Open(fileio_resource_,
                                      fileref_resource_,
                                      OpenFlagsToPPAPIOpenFlags(open_flags),
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

