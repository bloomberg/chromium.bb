/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_FACTORY_H_
#define LIBRARIES_NACL_IO_MOUNT_FACTORY_H_

#include <map>
#include <string>

#include "nacl_io/error.h"
#include "sdk_util/scoped_ref.h"

class PepperInterface;
class Mount;

typedef std::map<std::string, std::string> StringMap_t;

class MountFactory {
 public:
  virtual ~MountFactory() {}
  virtual Error CreateMount(int dev,
                            StringMap_t& args,
                            PepperInterface* ppapi,
                            ScopedRef<Mount>* out_mount) = 0;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_FACTORY_H_

