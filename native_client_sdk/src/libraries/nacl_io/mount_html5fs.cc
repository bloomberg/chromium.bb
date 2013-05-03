/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_io/mount_html5fs.h"

#include <errno.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "nacl_io/mount_node_html5fs.h"
#include "utils/auto_lock.h"

namespace {

#if defined(WIN32)
int64_t strtoull(const char* nptr, char** endptr, int base) {
  return _strtoui64(nptr, endptr, base);
}
#endif

}  // namespace

MountNode *MountHtml5Fs::Open(const Path& path, int mode) {
  if (BlockUntilFilesystemOpen() != PP_OK) {
    errno = ENODEV;
    return NULL;
  }

  PP_Resource fileref = ppapi()->GetFileRefInterface()->Create(
      filesystem_resource_, path.Join().c_str());
  if (!fileref)
    return NULL;

  MountNodeHtml5Fs* node = new MountNodeHtml5Fs(this, fileref);
  if (!node->Init(mode)) {
    node->Release();
    return NULL;
  }

  return node;
}

int MountHtml5Fs::Unlink(const Path& path) {
  return Remove(path);
}

int MountHtml5Fs::Mkdir(const Path& path, int permissions) {
  if (BlockUntilFilesystemOpen() != PP_OK) {
    errno = ENODEV;
    return -1;
  }

  ScopedResource fileref_resource(
      ppapi(), ppapi()->GetFileRefInterface()->Create(filesystem_resource_,
                                                      path.Join().c_str()));
  if (!fileref_resource.pp_resource()) {
    errno = EINVAL;
    return -1;
  }

  int32_t result = ppapi()->GetFileRefInterface()->MakeDirectory(
      fileref_resource.pp_resource(), PP_FALSE, PP_BlockUntilComplete());
  if (result != PP_OK) {
    errno = PPErrorToErrno(result);
    return -1;
  }

  return 0;
}

int MountHtml5Fs::Rmdir(const Path& path) {
  return Remove(path);
}

int MountHtml5Fs::Remove(const Path& path) {
  if (BlockUntilFilesystemOpen() != PP_OK) {
    errno = ENODEV;
    return -1;
  }

  ScopedResource fileref_resource(
      ppapi(), ppapi()->GetFileRefInterface()->Create(filesystem_resource_,
                                                      path.Join().c_str()));
  if (!fileref_resource.pp_resource()) {
    errno = EINVAL;
    return -1;
  }

  int32_t result = ppapi()->GetFileRefInterface()->Delete(
      fileref_resource.pp_resource(),
      PP_BlockUntilComplete());
  if (result != PP_OK) {
    errno = PPErrorToErrno(result);
    return -1;
  }

  return 0;
}


MountHtml5Fs::MountHtml5Fs()
    : filesystem_resource_(0),
      filesystem_open_has_result_(false),
      filesystem_open_result_(PP_OK) {
}

bool MountHtml5Fs::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  if (!Mount::Init(dev, args, ppapi))
    return false;

  if (!ppapi)
    return false;

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
  filesystem_resource_ = ppapi->GetFileSystemInterface()->Create(
      ppapi_->GetInstance(), filesystem_type);

  if (filesystem_resource_ == 0)
    return false;

  // We can't block the main thread, so make an asynchronous call if on main
  // thread. If we are off-main-thread, then don't make an asynchronous call;
  // otherwise we require a message loop.
  bool main_thread = ppapi->IsMainThread();
  PP_CompletionCallback cc = main_thread ?
      PP_MakeCompletionCallback(&MountHtml5Fs::FilesystemOpenCallbackThunk,
                                this) :
      PP_BlockUntilComplete();

  int32_t result = ppapi->GetFileSystemInterface()->Open(
      filesystem_resource_, expected_size, cc);

  if (!main_thread) {
    filesystem_open_has_result_ = true;
    filesystem_open_result_ = result;

    return filesystem_open_result_ == PP_OK;
  } else {
    // We have to assume the call to Open will succeed; there is no better
    // result to return here.
    return true;
  }
}

void MountHtml5Fs::Destroy() {
  ppapi_->ReleaseResource(filesystem_resource_);
  pthread_cond_destroy(&filesystem_open_cond_);
}

int32_t MountHtml5Fs::BlockUntilFilesystemOpen() {
  AutoLock lock(&lock_);
  while (!filesystem_open_has_result_) {
    pthread_cond_wait(&filesystem_open_cond_, &lock_);
  }
  return filesystem_open_result_;
}

// static
void MountHtml5Fs::FilesystemOpenCallbackThunk(void* user_data,
                                               int32_t result) {
  MountHtml5Fs* self = static_cast<MountHtml5Fs*>(user_data);
  self->FilesystemOpenCallback(result);
}

void MountHtml5Fs::FilesystemOpenCallback(int32_t result) {
  AutoLock lock(&lock_);
  filesystem_open_has_result_ = true;
  filesystem_open_result_ = result;
  pthread_cond_signal(&filesystem_open_cond_);
}
