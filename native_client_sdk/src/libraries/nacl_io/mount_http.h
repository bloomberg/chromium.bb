/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_IO_MOUNT_HTTP_H_
#define LIBRARIES_NACL_IO_MOUNT_HTTP_H_

#include <string>
#include "nacl_io/mount.h"
#include "nacl_io/pepper_interface.h"

class MountNode;
class MountNodeDir;
class MountNodeHttp;
class MountHttpMock;

class MountHttp : public Mount {
 public:
  typedef std::map<std::string, MountNode*> NodeMap_t;

  virtual MountNode *Open(const Path& path, int mode);
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
  MountNodeDir* FindOrCreateDir(const Path& path);
  char *LoadManifest(const std::string& path);
  bool ParseManifest(char *text);

 private:
  std::string url_root_;
  StringMap_t headers_;
  NodeMap_t node_cache_;
  bool allow_cors_;
  bool allow_credentials_;
  bool cache_stat_;
  bool cache_content_;

  friend class Mount;
  friend class MountNodeHttp;
  friend class MountHttpMock;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_HTTP_H_
