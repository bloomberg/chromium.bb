/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_mounts/real_pepper_interface.h"
#include <assert.h>
#include <stdio.h>

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_var.h>

#define DEFINE_CONSTRUCTOR(Class, Interface) \
    Class::Class(const Interface* interface) : interface_(interface) {}

#define DEFINE_METHOD1(Class, ReturnType, MethodName, Type0) \
    ReturnType Class::MethodName(Type0 arg0) { \
      return interface_->MethodName(arg0); \
    }

#define DEFINE_METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    ReturnType Class::MethodName(Type0 arg0, Type1 arg1) { \
      return interface_->MethodName(arg0, arg1); \
    }

#define DEFINE_METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    ReturnType Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2) { \
      return interface_->MethodName(arg0, arg1, arg2); \
    }

#define DEFINE_METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, \
                       Type3) \
    ReturnType Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                 Type3 arg3) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3); \
    }

#define DEFINE_METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, \
                       Type3, Type4) \
    ReturnType Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                 Type3 arg3, Type4 arg4) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3, arg4); \
    }


#define DEFINE_VMETHOD1(Class, MethodName, Type0) \
    void Class::MethodName(Type0 arg0) { \
      interface_->MethodName(arg0); \
    }

#define DEFINE_VMETHOD2(Class, MethodName, Type0, Type1) \
    void Class::MethodName(Type0 arg0, Type1 arg1) { \
      interface_->MethodName(arg0, arg1); \
    }

#define DEFINE_VMETHOD3(Class, MethodName, Type0, Type1, Type2) \
    void Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2) { \
      interface_->MethodName(arg0, arg1, arg2); \
    }

#define DEFINE_VMETHOD4(Class, MethodName, Type0, Type1, Type2, Type3) \
    void Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                 Type3 arg3) { \
      interface_->MethodName(arg0, arg1, arg2, arg3); \
    }

#define DEFINE_VMETHOD5(Class, MethodName, Type0, Type1, Type2, Type3, Type4) \
    void Class::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                 Type3 arg3, Type4 arg4) { \
      interface_->MethodName(arg0, arg1, arg2, arg3, arg4); \
    }



class RealFileSystemInterface : public FileSystemInterface {
 public:
  explicit RealFileSystemInterface(const PPB_FileSystem* filesystem_interface);
  virtual PP_Resource Create(PP_Instance, PP_FileSystemType);
  virtual int32_t Open(PP_Resource, int64_t, PP_CompletionCallback);

 private:
  const PPB_FileSystem* interface_;
};
DEFINE_CONSTRUCTOR(RealFileSystemInterface, PPB_FileSystem)
DEFINE_METHOD2(RealFileSystemInterface, PP_Resource, Create, PP_Instance,
    PP_FileSystemType)
DEFINE_METHOD3(RealFileSystemInterface, int32_t, Open, PP_Resource, int64_t,
    PP_CompletionCallback)


class RealConsoleInterface : public ConsoleInterface {
 public:
  explicit RealConsoleInterface(const PPB_Console* console_interface);
  virtual void Log(PP_Instance, PP_LogLevel, struct PP_Var);
 private:
  const PPB_Console* interface_;
};
DEFINE_CONSTRUCTOR(RealConsoleInterface, PPB_Console)
DEFINE_VMETHOD3(RealConsoleInterface, Log, PP_Instance, PP_LogLevel,
    struct PP_Var);


class RealFileRefInterface : public FileRefInterface {
 public:
  explicit RealFileRefInterface(const PPB_FileRef* fileref_interface);

  virtual PP_Resource Create(PP_Resource, const char*);
  virtual int32_t Delete(PP_Resource, PP_CompletionCallback);
  virtual PP_Var GetName(PP_Resource);
  virtual int32_t MakeDirectory(PP_Resource, PP_Bool,
      PP_CompletionCallback);

 private:
  const PPB_FileRef* interface_;
};
DEFINE_CONSTRUCTOR(RealFileRefInterface, PPB_FileRef)
DEFINE_METHOD2(RealFileRefInterface, PP_Resource, Create, PP_Resource,
    const char*)
DEFINE_METHOD2(RealFileRefInterface, int32_t, Delete, PP_Resource,
    PP_CompletionCallback)
DEFINE_METHOD1(RealFileRefInterface, PP_Var, GetName, PP_Resource)
DEFINE_METHOD3(RealFileRefInterface, int32_t, MakeDirectory, PP_Resource,
    PP_Bool, PP_CompletionCallback);


class RealFileIoInterface : public FileIoInterface {
 public:
  explicit RealFileIoInterface(const PPB_FileIO* fileio_interface);

  virtual void Close(PP_Resource);
  virtual PP_Resource Create(PP_Instance);
  virtual int32_t Flush(PP_Resource, PP_CompletionCallback);
  virtual int32_t Open(PP_Resource, PP_Resource, int32_t,
      PP_CompletionCallback);
  virtual int32_t Query(PP_Resource, PP_FileInfo*, PP_CompletionCallback);
  virtual int32_t Read(PP_Resource, int64_t, char*, int32_t,
      PP_CompletionCallback);
  virtual int32_t SetLength(PP_Resource, int64_t, PP_CompletionCallback);
  virtual int32_t Write(PP_Resource, int64_t, const char*, int32_t,
      PP_CompletionCallback);

 private:
  const PPB_FileIO* interface_;
};
DEFINE_CONSTRUCTOR(RealFileIoInterface, PPB_FileIO)
DEFINE_METHOD1(RealFileIoInterface, void, Close, PP_Resource)
DEFINE_METHOD1(RealFileIoInterface, PP_Resource, Create, PP_Resource)
DEFINE_METHOD2(RealFileIoInterface, int32_t, Flush, PP_Resource,
    PP_CompletionCallback)
DEFINE_METHOD4(RealFileIoInterface, int32_t, Open, PP_Resource, PP_Resource,
    int32_t, PP_CompletionCallback);
DEFINE_METHOD3(RealFileIoInterface, int32_t, Query, PP_Resource, PP_FileInfo*,
    PP_CompletionCallback);
DEFINE_METHOD5(RealFileIoInterface, int32_t, Read, PP_Resource, int64_t, char*,
    int32_t, PP_CompletionCallback);
DEFINE_METHOD3(RealFileIoInterface, int32_t, SetLength, PP_Resource, int64_t,
    PP_CompletionCallback);
DEFINE_METHOD5(RealFileIoInterface, int32_t, Write, PP_Resource, int64_t,
    const char*, int32_t, PP_CompletionCallback);


class RealDirectoryReaderInterface : public DirectoryReaderInterface {
 public:
  explicit RealDirectoryReaderInterface(
      const PPB_DirectoryReader_Dev* directory_reader_interface);

  virtual PP_Resource Create(PP_Resource);
  virtual int32_t GetNextEntry(PP_Resource, PP_DirectoryEntry_Dev*,
      PP_CompletionCallback);

 private:
  const PPB_DirectoryReader_Dev* interface_;
};
DEFINE_CONSTRUCTOR(RealDirectoryReaderInterface, PPB_DirectoryReader_Dev)
DEFINE_METHOD1(RealDirectoryReaderInterface, PP_Resource, Create, PP_Resource)
DEFINE_METHOD3(RealDirectoryReaderInterface, int32_t, GetNextEntry, PP_Resource,
    PP_DirectoryEntry_Dev*, PP_CompletionCallback)

class RealVarInterface : public VarInterface {
 public:
  explicit RealVarInterface(const PPB_Var* var_interface);

  virtual struct PP_Var VarFromUtf8(const char *, uint32_t);
  virtual const char* VarToUtf8(PP_Var, uint32_t*);

 private:
  const PPB_Var* interface_;
};
DEFINE_CONSTRUCTOR(RealVarInterface, PPB_Var)
DEFINE_METHOD2(RealVarInterface, struct PP_Var, VarFromUtf8, const char *,
    uint32_t)
DEFINE_METHOD2(RealVarInterface, const char*, VarToUtf8, PP_Var, uint32_t*)

class RealMessagingInterface : public MessagingInterface {
 public:
  explicit RealMessagingInterface(const PPB_Messaging* messaging_interface);

  virtual void PostMessage(PP_Instance, struct PP_Var);

private:
  const PPB_Messaging* interface_;
};
DEFINE_CONSTRUCTOR(RealMessagingInterface, PPB_Messaging);
DEFINE_VMETHOD2(RealMessagingInterface, PostMessage, PP_Instance, struct
    PP_Var);


RealPepperInterface::RealPepperInterface(PP_Instance instance,
                                         PPB_GetInterface get_browser_interface)
    : instance_(instance),
      core_interface_(NULL),
      message_loop_interface_(NULL) {
  core_interface_ = static_cast<const PPB_Core*>(
      get_browser_interface(PPB_CORE_INTERFACE));
  message_loop_interface_ = static_cast<const PPB_MessageLoop*>(
      get_browser_interface(PPB_MESSAGELOOP_INTERFACE));
  assert(core_interface_);
  assert(message_loop_interface_);

  console_interface_ = new RealConsoleInterface(
      static_cast<const PPB_Console*>(get_browser_interface(
          PPB_CONSOLE_INTERFACE)));
  directory_reader_interface_ = new RealDirectoryReaderInterface(
      static_cast<const PPB_DirectoryReader_Dev*>(get_browser_interface(
          PPB_DIRECTORYREADER_DEV_INTERFACE)));
  fileio_interface_ = new RealFileIoInterface(static_cast<const PPB_FileIO*>(
      get_browser_interface(PPB_FILEIO_INTERFACE)));
  fileref_interface_ = new RealFileRefInterface(static_cast<const PPB_FileRef*>(
      get_browser_interface(PPB_FILEREF_INTERFACE)));
  filesystem_interface_ = new RealFileSystemInterface(
      static_cast<const PPB_FileSystem*>(get_browser_interface(
          PPB_FILESYSTEM_INTERFACE)));
  messaging_interface_ = new RealMessagingInterface(
      static_cast<const PPB_Messaging*>(get_browser_interface(
          PPB_MESSAGING_INTERFACE)));
  var_interface_= new RealVarInterface(
      static_cast<const PPB_Var*>(get_browser_interface(
          PPB_VAR_INTERFACE)));
}

PP_Instance RealPepperInterface::GetInstance() {
  return instance_;
}

void RealPepperInterface::AddRefResource(PP_Resource resource) {
  if (resource)
    core_interface_->AddRefResource(resource);
}

void RealPepperInterface::ReleaseResource(PP_Resource resource) {
  if (resource)
    core_interface_->ReleaseResource(resource);
}

ConsoleInterface* RealPepperInterface::GetConsoleInterface() {
  return console_interface_;
}

FileSystemInterface* RealPepperInterface::GetFileSystemInterface() {
  return filesystem_interface_;
}

FileRefInterface* RealPepperInterface::GetFileRefInterface() {
  return fileref_interface_;
}

FileIoInterface* RealPepperInterface::GetFileIoInterface() {
  return fileio_interface_;
}

DirectoryReaderInterface* RealPepperInterface::GetDirectoryReaderInterface() {
  return directory_reader_interface_;
}

MessagingInterface* RealPepperInterface::GetMessagingInterface() {
  return messaging_interface_;
}

VarInterface* RealPepperInterface::GetVarInterface() {
  return var_interface_;
}

int32_t RealPepperInterface::InitializeMessageLoop() {
  int32_t result;
  PP_Resource message_loop = 0;
  if (core_interface_->IsMainThread()) {
    // TODO(binji): Spin up the main thread's ppapi work thread.
    assert(0);
  } else {
    message_loop = message_loop_interface_->GetCurrent();
    if (!message_loop) {
      message_loop = message_loop_interface_->Create(instance_);
      result = message_loop_interface_->AttachToCurrentThread(message_loop);
      assert(result == PP_OK);
    }
  }

  return PP_OK;
}
