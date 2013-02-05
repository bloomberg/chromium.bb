/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_

#include <map>
#include <string>

#include "nacl_io/mount_node.h"

struct dirent;

class MountDev;
class MountHtml5Fs;
class MountHttp;
class MountMem;

class MountNodeDir : public MountNode {
 protected:
  explicit MountNodeDir(Mount* mnt);
  virtual ~MountNodeDir();

 public:
  typedef std::map<std::string, MountNode*> MountNodeMap_t;

  virtual int GetDents(size_t offs, struct dirent* pdir, size_t count);
  virtual int Read(size_t offs, void *buf, size_t count);
  virtual int Truncate(size_t size);
  virtual int Write(size_t offs, void *buf, size_t count);

  // Adds a finds or adds a directory entry as an INO, updating the refcount
  virtual int AddChild(const std::string& name, MountNode *node);
  virtual int RemoveChild(const std::string& name);
  virtual MountNode* FindChild(const std::string& name);
  virtual int ChildCount();


protected:
   void ClearCache();
   void BuildCache();

private:
  struct dirent* cache_;
  MountNodeMap_t map_;

  friend class MountDev;
  friend class MountMem;
  friend class MountHttp;
  friend class MountHtml5Fs;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_
