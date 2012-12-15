/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_

#include <ppapi/c/dev/ppb_directory_reader_dev.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_var.h>
#include <utils/macros.h>

class DirectoryReaderInterface;
class FileIoInterface;
class FileRefInterface;
class FileSystemInterface;
class VarInterface;

class PepperInterface {
 public:
  virtual ~PepperInterface() {}
  virtual PP_Instance GetInstance() = 0;
  virtual void AddRefResource(PP_Resource) = 0;
  virtual void ReleaseResource(PP_Resource) = 0;
  virtual FileSystemInterface* GetFileSystemInterface() = 0;
  virtual FileRefInterface* GetFileRefInterface() = 0;
  virtual FileIoInterface* GetFileIoInterface() = 0;
  virtual DirectoryReaderInterface* GetDirectoryReaderInterface() = 0;
  virtual VarInterface* GetVarInterface() = 0;
};

class FileSystemInterface {
 public:
  virtual ~FileSystemInterface() {}
  virtual PP_Resource Create(PP_Instance, PP_FileSystemType) = 0;
  virtual int32_t Open(PP_Resource, int64_t, PP_CompletionCallback) = 0;
};

class FileRefInterface {
 public:
  virtual ~FileRefInterface() {}
  virtual PP_Resource Create(PP_Resource, const char*) = 0;
  virtual int32_t Delete(PP_Resource, PP_CompletionCallback) = 0;
  virtual PP_Var GetName(PP_Resource) = 0;
  virtual int32_t MakeDirectory(PP_Resource, PP_Bool,
      PP_CompletionCallback) = 0;
};

class FileIoInterface {
 public:
  virtual ~FileIoInterface() {}
  virtual void Close(PP_Resource) = 0;
  virtual PP_Resource Create(PP_Instance) = 0;
  virtual int32_t Flush(PP_Resource, PP_CompletionCallback) = 0;
  virtual int32_t Open(PP_Resource, PP_Resource, int32_t,
      PP_CompletionCallback) = 0;
  virtual int32_t Query(PP_Resource, PP_FileInfo*,
      PP_CompletionCallback) = 0;
  virtual int32_t Read(PP_Resource, int64_t, char*, int32_t,
      PP_CompletionCallback) = 0;
  virtual int32_t SetLength(PP_Resource, int64_t,
      PP_CompletionCallback) = 0;
  virtual int32_t Write(PP_Resource, int64_t, const char*, int32_t,
      PP_CompletionCallback) = 0;
};

class DirectoryReaderInterface {
 public:
  virtual ~DirectoryReaderInterface() {}
  virtual PP_Resource Create(PP_Resource) = 0;
  virtual int32_t GetNextEntry(PP_Resource, PP_DirectoryEntry_Dev*,
      PP_CompletionCallback) = 0;
};

class VarInterface {
 public:
  virtual ~VarInterface() {}
  virtual const char* VarToUtf8(PP_Var, uint32_t*) = 0;
};


class ScopedResource {
 public:
  struct NoAddRef {};

  ScopedResource(PepperInterface* ppapi, PP_Resource resource);
  ScopedResource(PepperInterface* ppapi, PP_Resource resource, NoAddRef);
  ~ScopedResource();

 private:
  PepperInterface* ppapi_;
  PP_Resource resource_;

  DISALLOW_COPY_AND_ASSIGN(ScopedResource);
};

#endif  // LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_
