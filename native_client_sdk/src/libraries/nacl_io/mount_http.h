// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LIBRARIES_NACL_IO_MOUNT_HTTP_H_
#define LIBRARIES_NACL_IO_MOUNT_HTTP_H_

#include <string>
#include "nacl_io/mount.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/typed_mount_factory.h"

class MountHttpMock;

namespace nacl_io {

class MountNode;

std::string NormalizeHeaderKey(const std::string& s);

class MountHttp : public Mount {
 public:
  typedef std::map<std::string, ScopedMountNode> NodeMap_t;

  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int mode, ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

  PP_Resource MakeUrlRequestInfo(const std::string& url,
                                 const char* method,
                                 StringMap_t* additional_headers);

 protected:
  MountHttp();

  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();
  Error FindOrCreateDir(const Path& path, ScopedMountNode* out_node);
  Error LoadManifest(const std::string& path, char** out_manifest);
  Error ParseManifest(const char *text);

 private:
  // Gets the URL to fetch for |path|.
  // |path| is relative to the mount point for the HTTP filesystem.
  std::string MakeUrl(const Path& path);

  std::string url_root_;
  StringMap_t headers_;
  NodeMap_t node_cache_;
  bool allow_cors_;
  bool allow_credentials_;
  bool cache_stat_;
  bool cache_content_;

  friend class TypedMountFactory<MountHttp>;
  friend class MountNodeHttp;
  friend class ::MountHttpMock;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_HTTP_H_
