/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_MOUNTS_MOUNT_HTTP_H_
#define LIBRARIES_NACL_MOUNTS_MOUNT_HTTP_H_

#include <string>
#include "nacl_mounts/mount.h"
#include "nacl_mounts/pepper_interface.h"

class MountNode;

class MountHttp : public Mount {
 public:
  virtual MountNode *Open(const Path& path, int mode);
  virtual int Close(MountNode* node);
  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int permissions);
  virtual int Rmdir(const Path& path);
  virtual int Remove(const Path& path);

  PP_Resource MakeUrlRequestInfo(const std::string& url,
                                 const char* method,
                                 StringMap_t* additional_headers);

 protected:
  MountHttp();

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 private:
  std::string url_root_;
  StringMap_t headers_;
  bool allow_cors_;
  bool allow_credentials_;

  friend class Mount;
};

#endif  // LIBRARIES_NACL_MOUNTS_MOUNT_HTTP_H_
