// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_MOUNT_MOCK_H_
#define LIBRARIES_NACL_IO_TEST_MOUNT_MOCK_H_

#include "gmock/gmock.h"

#include "nacl_io/mount.h"

class MountMock : public nacl_io::Mount {
 public:
  typedef nacl_io::Error Error;
  typedef nacl_io::Path Path;
  typedef nacl_io::PepperInterface PepperInterface;
  typedef nacl_io::ScopedMountNode ScopedMountNode;
  typedef nacl_io::StringMap_t StringMap_t;

  MountMock();
  virtual ~MountMock();

  MOCK_METHOD3(Init, Error(int, StringMap_t&, PepperInterface*));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD2(Access, Error(const Path&, int));
  MOCK_METHOD3(Open, Error(const Path&, int, ScopedMountNode*));
  MOCK_METHOD2(OpenResource, Error(const Path&, ScopedMountNode*));
  MOCK_METHOD1(Unlink, Error(const Path&));
  MOCK_METHOD2(Mkdir, Error(const Path&, int));
  MOCK_METHOD1(Rmdir, Error(const Path&));
  MOCK_METHOD1(Remove, Error(const Path&));
  MOCK_METHOD2(Rename, Error(const Path&, const Path&));
};

#endif  // LIBRARIES_NACL_IO_TEST_MOUNT_MOCK_H_
