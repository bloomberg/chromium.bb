// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "dart/runtime/include/dart_api.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/platform/native/system_thunks.h"


static Dart_NativeFunction ResolveName(
    Dart_Handle name, int argc, bool* auto_setup_scope);


DART_EXPORT Dart_Handle mojo_dart_core_Init(Dart_Handle parent_library) {
  if (Dart_IsError(parent_library)) {
    return parent_library;
  }

  Dart_Handle result_code = Dart_SetNativeResolver(
      parent_library, ResolveName, NULL);
  if (Dart_IsError(result_code)) {
    return result_code;
  }

  return Dart_Null();
}


static Dart_Handle HandleError(Dart_Handle handle) {
  if (Dart_IsError(handle)) {
    Dart_PropagateError(handle);
  }
  return handle;
}


static void SetNullReturn(Dart_NativeArguments arguments) {
  Dart_SetReturnValue(arguments, Dart_Null());
}


static void SetInvalidArgumentReturn(Dart_NativeArguments arguments) {
  Dart_SetIntegerReturnValue(
      arguments, static_cast<int64_t>(MOJO_RESULT_INVALID_ARGUMENT));
}


#define CHECK_INTEGER_ARGUMENT(args, num, result, failure)                     \
  {                                                                            \
    Dart_Handle __status;                                                      \
    __status = Dart_GetNativeIntegerArgument(args, num, result);               \
    if (Dart_IsError(__status)) {                                              \
      Set##failure##Return(arguments);                                         \
      return;                                                                  \
    }                                                                          \
  }                                                                            \


extern "C" {
  extern size_t MojoSetSystemThunks(const MojoSystemThunks* system_thunks);
}


static void MojoSystemThunks_Set(Dart_NativeArguments arguments) {
  int64_t thunks_addr = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &thunks_addr, Null);

  MojoSystemThunks* thunks = reinterpret_cast<MojoSystemThunks*>(thunks_addr);
  MojoSetSystemThunks(thunks);

  Dart_SetReturnValue(arguments, Dart_Null());
}


struct CloserCallbackPeer {
  MojoHandle handle;
};


static void MojoHandleCloserCallback(void* isolate_data,
                                     Dart_WeakPersistentHandle handle,
                                     void* peer) {
  CloserCallbackPeer* callback_peer =
      reinterpret_cast<CloserCallbackPeer*>(peer);
  if (callback_peer->handle != MOJO_HANDLE_INVALID) {
    MojoClose(callback_peer->handle);
  }
  delete callback_peer;
}


// Setup a weak persistent handle for a Dart MojoHandle that calls MojoClose
// on the handle when the MojoHandle is GC'd or the VM is going down.
static void MojoHandle_Register(Dart_NativeArguments arguments) {
  // An instance of Dart class MojoHandle.
  Dart_Handle mojo_handle_instance = Dart_GetNativeArgument(arguments, 0);
  if (!Dart_IsInstance(mojo_handle_instance)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }
  // TODO(zra): Here, we could check that mojo_handle_instance is really a
  // MojoHandle instance, but with the Dart API it's not too easy to get a Type
  // object from the class name outside of the root library. For now, we'll rely
  // on the existence of the right fields to be sufficient.

  Dart_Handle raw_mojo_handle_instance = Dart_GetField(
      mojo_handle_instance, Dart_NewStringFromCString("_handle"));
  if (Dart_IsError(raw_mojo_handle_instance)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  Dart_Handle mojo_handle = Dart_GetField(
      raw_mojo_handle_instance, Dart_NewStringFromCString("h"));
  if (Dart_IsError(mojo_handle)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  int64_t raw_handle = static_cast<int64_t>(MOJO_HANDLE_INVALID);
  Dart_Handle result = Dart_IntegerToInt64(mojo_handle, &raw_handle);
  if (Dart_IsError(result)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  if (raw_handle == static_cast<int64_t>(MOJO_HANDLE_INVALID)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  CloserCallbackPeer* callback_peer = new CloserCallbackPeer();
  callback_peer->handle = static_cast<MojoHandle>(raw_handle);
  Dart_NewWeakPersistentHandle(mojo_handle_instance,
                               reinterpret_cast<void*>(callback_peer),
                               sizeof(CloserCallbackPeer),
                               MojoHandleCloserCallback);
  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(MOJO_RESULT_OK));
}


static void MojoHandle_Close(Dart_NativeArguments arguments) {
  int64_t handle;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, InvalidArgument);

  MojoResult res = MojoClose(static_cast<MojoHandle>(handle));

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoHandle_Wait(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t signals = 0;
  int64_t deadline = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &signals, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &deadline, InvalidArgument);

  MojoResult r = MojoWait(static_cast<MojoHandle>(handle),
                          static_cast<MojoHandleSignals>(signals),
                          static_cast<MojoDeadline>(deadline));

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(r));
}


static void MojoHandle_WaitMany(Dart_NativeArguments arguments) {
  int64_t num_handles = 0;
  int64_t deadline = 0;
  Dart_Handle handles = Dart_GetNativeArgument(arguments, 0);
  Dart_Handle signals = Dart_GetNativeArgument(arguments, 1);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_handles, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 3, &deadline, InvalidArgument);

  if (!Dart_IsList(handles) || !Dart_IsList(signals)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  intptr_t handles_len = 0;
  intptr_t signals_len = 0;
  Dart_ListLength(handles, &handles_len);
  Dart_ListLength(signals, &signals_len);
  if ((handles_len != num_handles) || (signals_len != num_handles)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  scoped_ptr<MojoHandle[]> mojo_handles(new MojoHandle[num_handles]);
  scoped_ptr<MojoHandleSignals[]> mojo_signals(
      new MojoHandleSignals[num_handles]);

  for (int i = 0; i < num_handles; i++) {
    Dart_Handle dart_handle = Dart_ListGetAt(handles, i);
    Dart_Handle dart_signal = Dart_ListGetAt(signals, i);
    if (!Dart_IsInteger(dart_handle) || !Dart_IsInteger(dart_signal)) {
      SetInvalidArgumentReturn(arguments);
      return;
    }
    int64_t mojo_handle = 0;
    int64_t mojo_signal = 0;
    Dart_IntegerToInt64(dart_handle, &mojo_handle);
    Dart_IntegerToInt64(dart_signal, &mojo_signal);
    mojo_handles[i] = static_cast<MojoHandle>(mojo_handle);
    mojo_signals[i] = static_cast<MojoHandleSignals>(mojo_signal);
  }

  MojoResult res = MojoWaitMany(mojo_handles.get(), mojo_signals.get(),
                                static_cast<uint32_t>(num_handles),
                                static_cast<MojoDeadline>(deadline));
  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoSharedBuffer_Create(Dart_NativeArguments arguments) {
  int64_t num_bytes = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &num_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &flags, Null);

  MojoCreateSharedBufferOptions options;
  options.struct_size = sizeof(MojoCreateSharedBufferOptions);
  options.flags = static_cast<MojoCreateSharedBufferOptionsFlags>(flags);

  MojoHandle out = MOJO_HANDLE_INVALID;;
  MojoResult res = MojoCreateSharedBuffer(
      &options, static_cast<int32_t>(num_bytes), &out);

  Dart_Handle list = Dart_NewList(2);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(out));
  Dart_SetReturnValue(arguments, list);
}


static void MojoSharedBuffer_Duplicate(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &flags, Null);

  MojoDuplicateBufferHandleOptions options;
  options.struct_size = sizeof(MojoDuplicateBufferHandleOptions);
  options.flags = static_cast<MojoDuplicateBufferHandleOptionsFlags>(flags);

  MojoHandle out = MOJO_HANDLE_INVALID;;
  MojoResult res = MojoDuplicateBufferHandle(
      static_cast<MojoHandle>(handle), &options, &out);

  Dart_Handle list = Dart_NewList(2);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(out));
  Dart_SetReturnValue(arguments, list);
}


static void MojoSharedBuffer_Map(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t offset = 0;
  int64_t num_bytes = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &offset, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 3, &flags, Null);

  void* out;
  MojoResult res = MojoMapBuffer(static_cast<MojoHandle>(handle),
                                 offset,
                                 num_bytes,
                                 &out,
                                 static_cast<MojoMapBufferFlags>(flags));

  Dart_Handle list = Dart_NewList(2);
  Dart_Handle typed_data;
  if (res == MOJO_RESULT_OK) {
    typed_data = Dart_NewExternalTypedData(
        Dart_TypedData_kByteData, out, num_bytes);
  } else {
    typed_data = Dart_Null();
  }
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, typed_data);
  Dart_SetReturnValue(arguments, list);
}


static void MojoSharedBuffer_Unmap(Dart_NativeArguments arguments) {
  Dart_Handle typed_data = Dart_GetNativeArgument(arguments, 0);
  if (!Dart_IsTypedData(typed_data)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  Dart_TypedData_Type typ;
  void *data;
  intptr_t len;
  Dart_TypedDataAcquireData(typed_data, &typ, &data, &len);
  MojoResult res = MojoUnmapBuffer(data);
  Dart_TypedDataReleaseData(typed_data);

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoDataPipe_Create(Dart_NativeArguments arguments) {
  int64_t element_bytes = 0;
  int64_t capacity_bytes = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &element_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &capacity_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &flags, Null);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = static_cast<MojoCreateDataPipeOptionsFlags>(flags);
  options.element_num_bytes = static_cast<uint32_t>(element_bytes);
  options.capacity_num_bytes = static_cast<uint32_t>(capacity_bytes);

  MojoHandle producer = MOJO_HANDLE_INVALID;
  MojoHandle consumer = MOJO_HANDLE_INVALID;
  MojoResult res = MojoCreateDataPipe(&options, &producer, &consumer);

  Dart_Handle list = Dart_NewList(3);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(producer));
  Dart_ListSetAt(list, 2, Dart_NewInteger(consumer));
  Dart_SetReturnValue(arguments, list);
}


static void MojoDataPipe_WriteData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);

  Dart_Handle typed_data = Dart_GetNativeArgument(arguments, 1);
  if (!Dart_IsTypedData(typed_data)) {
    SetNullReturn(arguments);
    return;
  }

  int64_t num_bytes = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_bytes, Null);

  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 3, &flags, Null);

  Dart_TypedData_Type type;
  void* data;
  intptr_t data_length;
  Dart_TypedDataAcquireData(typed_data, &type, &data, &data_length);
  uint32_t length = static_cast<uint32_t>(num_bytes);
  MojoResult res = MojoWriteData(
      static_cast<MojoHandle>(handle),
      data,
      &length,
      static_cast<MojoWriteDataFlags>(flags));
  Dart_TypedDataReleaseData(typed_data);

  Dart_Handle list = Dart_NewList(2);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(length));
  Dart_SetReturnValue(arguments, list);
}


static void MojoDataPipe_BeginWriteData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t buffer_bytes = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &buffer_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &flags, Null);

  void* buffer;
  uint32_t size = static_cast<uint32_t>(buffer_bytes);
  MojoResult res = MojoBeginWriteData(
      static_cast<MojoHandle>(handle),
      &buffer,
      &size,
      static_cast<MojoWriteDataFlags>(flags));

  Dart_Handle list = Dart_NewList(2);
  Dart_Handle typed_data;
  if (res == MOJO_RESULT_OK) {
    typed_data = Dart_NewExternalTypedData(
        Dart_TypedData_kByteData, buffer, size);
  } else {
    typed_data = Dart_Null();
  }
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, typed_data);
  Dart_SetReturnValue(arguments, list);
}


static void MojoDataPipe_EndWriteData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t num_bytes_written = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &num_bytes_written, InvalidArgument);

  MojoResult res = MojoEndWriteData(
      static_cast<MojoHandle>(handle),
      static_cast<uint32_t>(num_bytes_written));

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoDataPipe_ReadData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);

  Dart_Handle typed_data = Dart_GetNativeArgument(arguments, 1);
  if (!Dart_IsTypedData(typed_data) && !Dart_IsNull(typed_data)) {
    SetNullReturn(arguments);
    return;
  }

  int64_t num_bytes = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_bytes, Null);

  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 3, &flags, Null);

  Dart_TypedData_Type typ;
  void* data = NULL;
  intptr_t bdlen = 0;
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataAcquireData(typed_data, &typ, &data, &bdlen);
  }
  uint32_t len = static_cast<uint32_t>(num_bytes);
  MojoResult res = MojoReadData(
      static_cast<MojoHandle>(handle),
      data,
      &len,
      static_cast<MojoReadDataFlags>(flags));
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataReleaseData(typed_data);
  }

  Dart_Handle list = Dart_NewList(2);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(len));
  Dart_SetReturnValue(arguments, list);
}


static void MojoDataPipe_BeginReadData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t buffer_bytes = 0;
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &buffer_bytes, Null);
  CHECK_INTEGER_ARGUMENT(arguments, 2, &flags, Null);

  void* buffer;
  uint32_t size = static_cast<uint32_t>(buffer_bytes);
  MojoResult res = MojoBeginReadData(
      static_cast<MojoHandle>(handle),
      const_cast<const void**>(&buffer),
      &size,
      static_cast<MojoWriteDataFlags>(flags));

  Dart_Handle list = Dart_NewList(2);
  Dart_Handle typed_data;
  if (res == MOJO_RESULT_OK) {
    typed_data = Dart_NewExternalTypedData(
        Dart_TypedData_kByteData, buffer, size);
  } else {
    typed_data = Dart_Null();
  }
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, typed_data);
  Dart_SetReturnValue(arguments, list);
}


static void MojoDataPipe_EndReadData(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  int64_t num_bytes_read = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &num_bytes_read, InvalidArgument);

  MojoResult res = MojoEndReadData(
      static_cast<MojoHandle>(handle),
      static_cast<uint32_t>(num_bytes_read));

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoMessagePipe_Create(Dart_NativeArguments arguments) {
  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &flags, Null);

  MojoCreateMessagePipeOptions options;
  options.struct_size = sizeof(MojoCreateMessagePipeOptions);
  options.flags = static_cast<MojoCreateMessagePipeOptionsFlags>(flags);

  MojoHandle end1 = MOJO_HANDLE_INVALID;
  MojoHandle end2 = MOJO_HANDLE_INVALID;
  MojoResult res = MojoCreateMessagePipe(&options, &end1, &end2);

  Dart_Handle list = Dart_NewList(3);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(end1));
  Dart_ListSetAt(list, 2, Dart_NewInteger(end2));
  Dart_SetReturnValue(arguments, list);
}


static void MojoMessagePipe_Write(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, InvalidArgument);

  Dart_Handle typed_data = Dart_GetNativeArgument(arguments, 1);
  if (!Dart_IsTypedData(typed_data) && !Dart_IsNull(typed_data)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  int64_t num_bytes = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_bytes, InvalidArgument);

  Dart_Handle handles = Dart_GetNativeArgument(arguments, 3);
  if (!Dart_IsList(handles) && !Dart_IsNull(handles)) {
    SetInvalidArgumentReturn(arguments);
    return;
  }

  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 4, &flags, InvalidArgument);

  // Grab the data if there is any.
  Dart_TypedData_Type typ;
  void* bytes = NULL;
  intptr_t bdlen = 0;
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataAcquireData(typed_data, &typ, &bytes, &bdlen);
  }

  // Grab the handles if there are any.
  scoped_ptr<MojoHandle[]> mojo_handles;
  intptr_t handles_len = 0;
  if (!Dart_IsNull(handles)) {
    Dart_ListLength(handles, &handles_len);
    mojo_handles.reset(new MojoHandle[handles_len]);
    for (int i = 0; i < handles_len; i++) {
      Dart_Handle dart_handle = Dart_ListGetAt(handles, i);
      if (!Dart_IsInteger(dart_handle)) {
        SetInvalidArgumentReturn(arguments);
        return;
      }
      int64_t mojo_handle = 0;
      Dart_IntegerToInt64(dart_handle, &mojo_handle);
      mojo_handles[i] = static_cast<MojoHandle>(mojo_handle);
    }
  }

  MojoResult res = MojoWriteMessage(
      static_cast<MojoHandle>(handle),
      const_cast<const void*>(bytes),
      static_cast<uint32_t>(num_bytes),
      mojo_handles.get(),
      static_cast<uint32_t>(handles_len),
      static_cast<MojoWriteMessageFlags>(flags));

  // Release the data.
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataReleaseData(typed_data);
  }

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoMessagePipe_Read(Dart_NativeArguments arguments) {
  int64_t handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &handle, Null);

  Dart_Handle typed_data = Dart_GetNativeArgument(arguments, 1);
  if (!Dart_IsTypedData(typed_data) && !Dart_IsNull(typed_data)) {
    SetNullReturn(arguments);
    return;
  }

  int64_t num_bytes = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 2, &num_bytes, Null);

  Dart_Handle handles = Dart_GetNativeArgument(arguments, 3);
  if (!Dart_IsList(handles) && !Dart_IsNull(handles)) {
    SetNullReturn(arguments);
    return;
  }

  int64_t flags = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 4, &flags, Null);

  // Grab the data if there is any.
  Dart_TypedData_Type typ;
  void* bytes = NULL;
  intptr_t byte_data_len = 0;
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataAcquireData(typed_data, &typ, &bytes, &byte_data_len);
  }
  uint32_t blen = static_cast<uint32_t>(num_bytes);

  // Grab the handles if there are any.
  scoped_ptr<MojoHandle[]> mojo_handles;
  intptr_t handles_len = 0;
  if (!Dart_IsNull(handles)) {
    Dart_ListLength(handles, &handles_len);
    mojo_handles.reset(new MojoHandle[handles_len]);
  }
  uint32_t hlen = static_cast<uint32_t>(handles_len);

  MojoResult res = MojoReadMessage(
      static_cast<MojoHandle>(handle),
      bytes,
      &blen,
      mojo_handles.get(),
      &hlen,
      static_cast<MojoReadMessageFlags>(flags));

  // Release the data.
  if (!Dart_IsNull(typed_data)) {
    Dart_TypedDataReleaseData(typed_data);
  }

  if (!Dart_IsNull(handles)) {
    for (int i = 0; i < handles_len; i++) {
      Dart_ListSetAt(handles, i, Dart_NewInteger(mojo_handles[i]));
    }
  }

  Dart_Handle list = Dart_NewList(3);
  Dart_ListSetAt(list, 0, Dart_NewInteger(res));
  Dart_ListSetAt(list, 1, Dart_NewInteger(blen));
  Dart_ListSetAt(list, 2, Dart_NewInteger(hlen));
  Dart_SetReturnValue(arguments, list);
}


struct ControlData {
  int64_t handle;
  Dart_Port port;
  int64_t data;
};


static void MojoHandleWatcher_SendControlData(Dart_NativeArguments arguments) {
  int64_t control_handle = 0;
  int64_t client_handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &control_handle, InvalidArgument);
  CHECK_INTEGER_ARGUMENT(arguments, 1, &client_handle, InvalidArgument);

  Dart_Handle send_port_handle = Dart_GetNativeArgument(arguments, 2);
  Dart_Port send_port_id = 0;
  if (!Dart_IsNull(send_port_handle)) {
    Dart_Handle result = Dart_SendPortGetId(send_port_handle, &send_port_id);
    if (Dart_IsError(result)) {
      SetInvalidArgumentReturn(arguments);
      return;
    }
  }

  int64_t data = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 3, &data, InvalidArgument);

  ControlData cd;
  cd.handle = client_handle;
  cd.port = send_port_id;
  cd.data = data;
  const void* bytes = reinterpret_cast<const void*>(&cd);
  MojoResult res = MojoWriteMessage(
      control_handle, bytes, sizeof(cd), NULL, 0, 0);
  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(res));
}


static void MojoHandleWatcher_RecvControlData(Dart_NativeArguments arguments) {
  int64_t control_handle = 0;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &control_handle, Null);

  ControlData cd;
  void* bytes = reinterpret_cast<void*>(&cd);
  uint32_t num_bytes = sizeof(cd);
  uint32_t num_handles = 0;
  MojoResult res = MojoReadMessage(
      control_handle, bytes, &num_bytes, NULL, &num_handles, 0);
  if (res != MOJO_RESULT_OK) {
    SetNullReturn(arguments);
    return;
  }

  Dart_Handle list = Dart_NewList(3);
  Dart_ListSetAt(list, 0, Dart_NewInteger(cd.handle));
  Dart_ListSetAt(list, 1, Dart_NewSendPort(cd.port));
  Dart_ListSetAt(list, 2, Dart_NewInteger(cd.data));
  Dart_SetReturnValue(arguments, list);
}


static int64_t mojo_control_handle = MOJO_HANDLE_INVALID;
static void MojoHandleWatcher_SetControlHandle(Dart_NativeArguments arguments) {
  int64_t control_handle;
  CHECK_INTEGER_ARGUMENT(arguments, 0, &control_handle, InvalidArgument);

  if (mojo_control_handle == static_cast<int64_t>(MOJO_HANDLE_INVALID)) {
    mojo_control_handle = control_handle;
  }

  Dart_SetIntegerReturnValue(arguments, static_cast<int64_t>(MOJO_RESULT_OK));
}


static void MojoHandleWatcher_GetControlHandle(Dart_NativeArguments arguments) {
  Dart_SetIntegerReturnValue(arguments, mojo_control_handle);
}


#define SCOPE_FUNCTIONS(V)                                                     \
  V(MojoSharedBuffer_Create)                                                   \
  V(MojoSharedBuffer_Duplicate)                                                \
  V(MojoSharedBuffer_Map)                                                      \
  V(MojoSharedBuffer_Unmap)                                                    \
  V(MojoDataPipe_Create)                                                       \
  V(MojoDataPipe_WriteData)                                                    \
  V(MojoDataPipe_BeginWriteData)                                               \
  V(MojoDataPipe_ReadData)                                                     \
  V(MojoDataPipe_BeginReadData)                                                \
  V(MojoDataPipe_EndReadData)                                                  \
  V(MojoMessagePipe_Create)                                                    \
  V(MojoMessagePipe_Write)                                                     \
  V(MojoMessagePipe_Read)                                                      \
  V(MojoHandle_Register)                                                       \
  V(MojoHandle_WaitMany)                                                       \
  V(MojoHandleWatcher_SendControlData)                                         \
  V(MojoHandleWatcher_RecvControlData)                                         \

#define NOSCOPE_FUNCTIONS(V)                                                   \
  V(MojoSystemThunks_Set)                                                      \
  V(MojoHandle_Close)                                                          \
  V(MojoHandle_Wait)                                                           \
  V(MojoDataPipe_EndWriteData)                                                 \
  V(MojoHandleWatcher_SetControlHandle)                                        \
  V(MojoHandleWatcher_GetControlHandle)                                        \

#define FUNCTION_STRING_MAP(name) {#name, name},

struct FunctionLookup {
  const char* name;
  Dart_NativeFunction function;
};

FunctionLookup function_list[] = {
  SCOPE_FUNCTIONS(FUNCTION_STRING_MAP)
  {NULL, NULL}
};

FunctionLookup no_scope_function_list[] = {
  NOSCOPE_FUNCTIONS(FUNCTION_STRING_MAP)
  {NULL, NULL}
};

#undef FUNCTION_STRING_MAP


Dart_NativeFunction ResolveName(Dart_Handle name,
                                int argc,
                                bool* auto_setup_scope) {
  if (!Dart_IsString(name)) {
    return NULL;
  }
  Dart_NativeFunction result = NULL;
  if (auto_setup_scope == NULL) {
    return NULL;
  }

  Dart_EnterScope();
  const char* cname;
  HandleError(Dart_StringToCString(name, &cname));

  for (int i=0; function_list[i].name != NULL; ++i) {
    if (strcmp(function_list[i].name, cname) == 0) {
      *auto_setup_scope = true;
      Dart_ExitScope();
      return function_list[i].function;
    }
  }

  for (int i=0; no_scope_function_list[i].name != NULL; ++i) {
    if (strcmp(no_scope_function_list[i].name, cname) == 0) {
      *auto_setup_scope = false;
      result = no_scope_function_list[i].function;
      break;
    }
  }

  Dart_ExitScope();
  return result;
}
