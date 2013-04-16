/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_HTML5FS_H_
#define LIBRARIES_NACL_IO_MOUNT_HTML5FS_H_

#include <pthread.h>
#include "nacl_io/mount.h"
#include "nacl_io/pepper_interface.h"

class MountNode;

class MountHtml5Fs: public Mount {
 public:
  virtual MountNode *Open(const Path& path, int mode);
  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int permissions);
  virtual int Rmdir(const Path& path);
  virtual int Remove(const Path& path);

  PP_Resource filesystem_resource() { return filesystem_resource_; }

 protected:
  MountHtml5Fs();

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

  int32_t BlockUntilFilesystemOpen();

 private:
  static void FilesystemOpenCallbackThunk(void* user_data, int32_t result);
  void FilesystemOpenCallback(int32_t result);

  PP_Resource filesystem_resource_;
  bool filesystem_open_has_result_;  // protected by lock_.
  int32_t filesystem_open_result_;  // protected by lock_.
  pthread_cond_t filesystem_open_cond_;

  friend class Mount;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_HTML5FS_H_
