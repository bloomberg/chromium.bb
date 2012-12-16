/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_TEST_PEPPER_INTERFACE_MOCK_H_
#define LIBRARIES_NACL_MOUNTS_TEST_PEPPER_INTERFACE_MOCK_H_

#include "gmock/gmock.h"
#include "nacl_mounts/pepper_interface.h"

class FileSystemInterfaceMock;
class FileRefInterfaceMock;
class FileIoInterfaceMock;
class DirectoryReaderInterfaceMock;
class VarInterfaceMock;

class PepperInterfaceMock : public PepperInterface {
 public:
  PepperInterfaceMock();
  ~PepperInterfaceMock();

  MOCK_METHOD0(GetInstance, PP_Instance());
  MOCK_METHOD1(AddRefResource, void(PP_Resource));
  MOCK_METHOD1(ReleaseResource, void(PP_Resource));
  virtual FileSystemInterface* GetFileSystemInterface();
  virtual FileRefInterface* GetFileRefInterface();
  virtual FileIoInterface* GetFileIoInterface();
  virtual DirectoryReaderInterface* GetDirectoryReaderInterface();
  virtual VarInterface* GetVarInterface();

 private:
  FileSystemInterfaceMock* filesystem_interface_;
  FileRefInterfaceMock* fileref_interface_;
  FileIoInterfaceMock* fileio_interface_;
  DirectoryReaderInterfaceMock* directory_reader_interface_;
  VarInterfaceMock* var_interface_;
};

class FileSystemInterfaceMock : public FileSystemInterface {
 public:
  MOCK_METHOD2(Create, PP_Resource(PP_Instance, PP_FileSystemType));
  MOCK_METHOD3(Open, int32_t(PP_Resource, int64_t, PP_CompletionCallback));
};

class FileRefInterfaceMock : public FileRefInterface {
 public:
  MOCK_METHOD2(Create, PP_Resource(PP_Resource, const char*));
  MOCK_METHOD2(Delete, int32_t(PP_Resource, PP_CompletionCallback));
  MOCK_METHOD1(GetName, PP_Var(PP_Resource));
  MOCK_METHOD3(MakeDirectory, int32_t(PP_Resource, PP_Bool,
        PP_CompletionCallback));
};

class FileIoInterfaceMock : public FileIoInterface {
 public:
  MOCK_METHOD1(Close, void(PP_Resource));
  MOCK_METHOD1(Create, PP_Resource(PP_Instance));
  MOCK_METHOD2(Flush, int32_t(PP_Resource, PP_CompletionCallback));
  MOCK_METHOD4(Open, int32_t(PP_Resource, PP_Resource, int32_t,
        PP_CompletionCallback));
  MOCK_METHOD3(Query, int32_t(PP_Resource, PP_FileInfo*,
        PP_CompletionCallback));
  MOCK_METHOD5(Read, int32_t(PP_Resource, int64_t, char*, int32_t,
        PP_CompletionCallback));
  MOCK_METHOD3(SetLength, int32_t(PP_Resource, int64_t, PP_CompletionCallback));
  MOCK_METHOD5(Write, int32_t(PP_Resource, int64_t, const char*, int32_t,
        PP_CompletionCallback));
};

class DirectoryReaderInterfaceMock : public DirectoryReaderInterface {
 public:
  MOCK_METHOD1(Create, PP_Resource(PP_Resource));
  MOCK_METHOD3(GetNextEntry, int32_t(PP_Resource, PP_DirectoryEntry_Dev*,
        PP_CompletionCallback));
};

class VarInterfaceMock : public VarInterface {
 public:
  MOCK_METHOD2(VarFromUtf8, PP_Var(const char*, uint32_t));
  MOCK_METHOD2(VarToUtf8, const char*(PP_Var, uint32_t*));
};

#endif  // LIBRARIES_NACL_MOUNTS_TEST_PEPPER_INTERFACE_MOCK_H_
