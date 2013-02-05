/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Given an interface like this:
 *
 *   struct PPB_Frob {
 *     void (*Flange)(int32_t param1, char* param2);
 *     int32_t (*Shlep)(PP_CompletionCallback);
 *   };
 *
 * Write a set of macros like this:
 *
 *   BEGIN_INTERFACE(FrobInterface, PPB_Frob, PPB_FROB_INTERFACE)
 *     METHOD2(FrobInterface, void, Flange, int32_t, char*)
 *     METHOD1(FrobInterface, int32_t, Shlep, PP_CompletionCallback)
 *   END_INTERFACE(FrobInterface, PPB_Frob)
 */

BEGIN_INTERFACE(ConsoleInterface, PPB_Console, PPB_CONSOLE_INTERFACE)
  METHOD3(ConsoleInterface, void, Log, PP_Instance, PP_LogLevel, struct PP_Var)
END_INTERFACE(ConsoleInterface, PPB_Console)

BEGIN_INTERFACE(DirectoryReaderInterface, PPB_DirectoryReader_Dev,
                PPB_DIRECTORYREADER_DEV_INTERFACE)
  METHOD1(DirectoryReaderInterface, PP_Resource, Create, PP_Resource)
  METHOD3(DirectoryReaderInterface, int32_t, GetNextEntry, PP_Resource,
          PP_DirectoryEntry_Dev*, PP_CompletionCallback)
END_INTERFACE(DirectoryReaderInterface, PPB_DirectoryReader_Dev)

BEGIN_INTERFACE(FileIoInterface, PPB_FileIO, PPB_FILEIO_INTERFACE)
  METHOD1(FileIoInterface, void, Close, PP_Resource)
  METHOD1(FileIoInterface, PP_Resource, Create, PP_Resource)
  METHOD2(FileIoInterface, int32_t, Flush, PP_Resource,
          PP_CompletionCallback)
  METHOD4(FileIoInterface, int32_t, Open, PP_Resource, PP_Resource, int32_t,
          PP_CompletionCallback)
  METHOD3(FileIoInterface, int32_t, Query, PP_Resource, PP_FileInfo*,
          PP_CompletionCallback)
  METHOD5(FileIoInterface, int32_t, Read, PP_Resource, int64_t, char*,
          int32_t, PP_CompletionCallback)
  METHOD3(FileIoInterface, int32_t, SetLength, PP_Resource, int64_t,
          PP_CompletionCallback)
  METHOD5(FileIoInterface, int32_t, Write, PP_Resource, int64_t,
          const char*, int32_t, PP_CompletionCallback)
END_INTERFACE(FileIoInterface, PPB_FileIO)

BEGIN_INTERFACE(FileRefInterface, PPB_FileRef, PPB_FILEREF_INTERFACE)
  METHOD2(FileRefInterface, PP_Resource, Create, PP_Resource, const char*)
  METHOD2(FileRefInterface, int32_t, Delete, PP_Resource, PP_CompletionCallback)
  METHOD1(FileRefInterface, PP_Var, GetName, PP_Resource)
  METHOD3(FileRefInterface, int32_t, MakeDirectory, PP_Resource, PP_Bool,
          PP_CompletionCallback)
END_INTERFACE(FileRefInterface, PPB_FileRef)

BEGIN_INTERFACE(FileSystemInterface, PPB_FileSystem, PPB_FILESYSTEM_INTERFACE)
  METHOD2(FileSystemInterface, PP_Resource, Create, PP_Instance,
          PP_FileSystemType)
  METHOD3(FileSystemInterface, int32_t, Open, PP_Resource, int64_t,
          PP_CompletionCallback)
END_INTERFACE(FileSystemInterface, PPB_FileSystem)

BEGIN_INTERFACE(MessagingInterface, PPB_Messaging, PPB_MESSAGING_INTERFACE)
  METHOD2(MessagingInterface, void, PostMessage, PP_Instance, struct PP_Var)
END_INTERFACE(MessagingInterface, PPB_Messaging)

BEGIN_INTERFACE(VarInterface, PPB_Var, PPB_VAR_INTERFACE)
  METHOD2(VarInterface, struct PP_Var, VarFromUtf8, const char *, uint32_t)
  METHOD2(VarInterface, const char*, VarToUtf8, PP_Var, uint32_t*)
END_INTERFACE(VarInterface, PPB_Var)

BEGIN_INTERFACE(URLLoaderInterface, PPB_URLLoader, PPB_URLLOADER_INTERFACE)
  METHOD1(URLLoaderInterface, PP_Resource, Create, PP_Instance)
  METHOD3(URLLoaderInterface, int32_t, Open, PP_Resource, PP_Resource,
          PP_CompletionCallback)
  METHOD1(URLLoaderInterface, PP_Resource, GetResponseInfo, PP_Resource)
  METHOD4(URLLoaderInterface, int32_t, ReadResponseBody, PP_Resource, void*,
          int32_t, PP_CompletionCallback)
  METHOD1(URLLoaderInterface, void, Close, PP_Resource)
END_INTERFACE(URLLoaderInterface, PPB_URLLoader)

BEGIN_INTERFACE(URLRequestInfoInterface, PPB_URLRequestInfo,
                PPB_URLREQUESTINFO_INTERFACE)
  METHOD1(URLRequestInfoInterface, PP_Resource, Create, PP_Instance)
  METHOD3(URLRequestInfoInterface, PP_Bool, SetProperty, PP_Resource,
          PP_URLRequestProperty, PP_Var)
END_INTERFACE(URLRequestInfoInterface, PPB_URLRequestInfo)

BEGIN_INTERFACE(URLResponseInfoInterface, PPB_URLResponseInfo,
                PPB_URLRESPONSEINFO_INTERFACE)
  METHOD2(URLResponseInfoInterface, PP_Var, GetProperty, PP_Resource,
          PP_URLResponseProperty)
END_INTERFACE(URLResponseInfoInterface, PPB_URLResponseInfo)
