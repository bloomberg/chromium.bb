// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_html5fs.h"

#include <errno.h>
#include <fcntl.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "nacl_io/mount_node_html5fs.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

#if defined(WIN32)
int64_t strtoull(const char* nptr, char** endptr, int base) {
  return _strtoui64(nptr, endptr, base);
}
#endif

}  // namespace

Error MountHtml5Fs::Access(const Path& path, int a_mode) {
  // a_mode is unused, since all files are readable, writable and executable.
  ScopedMountNode node;
  return Open(path, O_RDONLY, &node);
}

Error MountHtml5Fs::Open(const Path& path,
                         int open_flags,
                         ScopedMountNode* out_node) {
  out_node->reset(NULL);
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  PP_Resource fileref = ppapi()->GetFileRefInterface()
      ->Create(filesystem_resource_, path.Join().c_str());
  if (!fileref)
    return ENOENT;

  ScopedMountNode node(new MountNodeHtml5Fs(this, fileref));
  error = node->Init(open_flags);
  if (error)
    return error;

  *out_node = node;
  return 0;
}

Error MountHtml5Fs::Unlink(const Path& path) { return Remove(path); }

Error MountHtml5Fs::Mkdir(const Path& path, int permissions) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  // FileRef returns PP_ERROR_NOACCESS which is translated to EACCES if you
  // try to create the root directory. EEXIST is a better errno here.
  if (path.Top())
    return EEXIST;

  ScopedResource fileref_resource(
      ppapi(),
      ppapi()->GetFileRefInterface()->Create(filesystem_resource_,
                                             path.Join().c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  int32_t result = ppapi()->GetFileRefInterface()->MakeDirectory(
      fileref_resource.pp_resource(), PP_FALSE, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  return 0;
}

Error MountHtml5Fs::Rmdir(const Path& path) { return Remove(path); }

Error MountHtml5Fs::Remove(const Path& path) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  ScopedResource fileref_resource(
      ppapi(),
      ppapi()->GetFileRefInterface()->Create(filesystem_resource_,
                                             path.Join().c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  int32_t result = ppapi()->GetFileRefInterface()
      ->Delete(fileref_resource.pp_resource(), PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  return 0;
}

Error MountHtml5Fs::Rename(const Path& path, const Path& newpath) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  ScopedResource fileref_resource(
      ppapi(),
      ppapi()->GetFileRefInterface()->Create(filesystem_resource_,
                                             path.Join().c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  return EACCES;
}

MountHtml5Fs::MountHtml5Fs()
    : filesystem_resource_(0),
      filesystem_open_has_result_(false),
      filesystem_open_error_(0) {}

Error MountHtml5Fs::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  Error error = Mount::Init(dev, args, ppapi);
  if (error)
    return error;

  if (!ppapi)
    return ENOSYS;

  pthread_cond_init(&filesystem_open_cond_, NULL);

  // Parse mount args.
  PP_FileSystemType filesystem_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
  int64_t expected_size = 0;
  for (StringMap_t::iterator iter = args.begin(), end = args.end(); iter != end;
       ++iter) {
    if (iter->first == "type") {
      if (iter->second == "PERSISTENT") {
        filesystem_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
      } else if (iter->second == "TEMPORARY") {
        filesystem_type = PP_FILESYSTEMTYPE_LOCALTEMPORARY;
      }
    } else if (iter->first == "expected_size") {
      expected_size = strtoull(iter->second.c_str(), NULL, 10);
    }
  }

  // Initialize filesystem.
  filesystem_resource_ = ppapi->GetFileSystemInterface()
      ->Create(ppapi_->GetInstance(), filesystem_type);
  if (filesystem_resource_ == 0)
    return ENOSYS;

  // We can't block the main thread, so make an asynchronous call if on main
  // thread. If we are off-main-thread, then don't make an asynchronous call;
  // otherwise we require a message loop.
  bool main_thread = ppapi->GetCoreInterface()->IsMainThread();
  PP_CompletionCallback cc =
      main_thread ? PP_MakeCompletionCallback(
                        &MountHtml5Fs::FilesystemOpenCallbackThunk, this)
                  : PP_BlockUntilComplete();

  int32_t result = ppapi->GetFileSystemInterface()
      ->Open(filesystem_resource_, expected_size, cc);

  if (!main_thread) {
    filesystem_open_has_result_ = true;
    filesystem_open_error_ = PPErrorToErrno(result);

    return filesystem_open_error_;
  }

  // We have to assume the call to Open will succeed; there is no better
  // result to return here.
  return 0;
}

void MountHtml5Fs::Destroy() {
  ppapi_->ReleaseResource(filesystem_resource_);
  pthread_cond_destroy(&filesystem_open_cond_);
}

Error MountHtml5Fs::BlockUntilFilesystemOpen() {
  AUTO_LOCK(filesysem_open_lock_);
  while (!filesystem_open_has_result_) {
    pthread_cond_wait(&filesystem_open_cond_, filesysem_open_lock_.mutex());
  }
  return filesystem_open_error_;
}

// static
void MountHtml5Fs::FilesystemOpenCallbackThunk(void* user_data,
                                               int32_t result) {
  MountHtml5Fs* self = static_cast<MountHtml5Fs*>(user_data);
  self->FilesystemOpenCallback(result);
}

void MountHtml5Fs::FilesystemOpenCallback(int32_t result) {
  AUTO_LOCK(filesysem_open_lock_);
  filesystem_open_has_result_ = true;
  filesystem_open_error_ = PPErrorToErrno(result);
  pthread_cond_signal(&filesystem_open_cond_);
}

}  // namespace nacl_io

