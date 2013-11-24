// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/core.h"

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/bindings/js/handle.h"

namespace mojo {
namespace js {

namespace {

void Close(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle;
  if (!args.GetNext(&handle))
    return args.ThrowError();

  args.Return(mojo::CloseRaw(handle));
}

void Wait(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle;
  MojoWaitFlags flags = MOJO_WAIT_FLAG_NONE;
  MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE;

  if (!args.GetNext(&handle) ||
      !args.GetNext(&flags) ||
      !args.GetNext(&deadline)) {
    return args.ThrowError();
  }

  args.Return(mojo::Wait(handle, flags, deadline));
}

void WaitMany(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  std::vector<mojo::Handle> handles;
  std::vector<MojoWaitFlags> flags;
  MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE;

  if (!args.GetNext(&handles) ||
      !args.GetNext(&flags) ||
      !args.GetNext(&deadline)) {
    return args.ThrowError();
  }

  args.Return(mojo::WaitMany(handles, flags, deadline));
}

void CreateMessagePipe(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  MojoHandle handle_0 = MOJO_HANDLE_INVALID;
  MojoHandle handle_1 = MOJO_HANDLE_INVALID;
  MojoResult result = MojoCreateMessagePipe(&handle_0, &handle_1);
  CHECK(result == MOJO_RESULT_OK);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(info.GetIsolate());
  dictionary.Set("handle0", handle_0);
  dictionary.Set("handle1", handle_1);
  args.Return(dictionary);
}

void WriteMessage(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  MojoHandle handle = MOJO_HANDLE_INVALID;
  gin::ArrayBufferView buffer(args.isolate());
  std::vector<MojoHandle> handles;
  MojoWriteMessageFlags flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  if (!args.GetNext(&handle) ||
      !args.GetNext(&buffer) ||
      !args.GetNext(&handles) ||
      !args.GetNext(&flags)) {
    return args.ThrowError();
  }

  args.Return(MojoWriteMessage(handle,
                               buffer.bytes(),
                               static_cast<uint32_t>(buffer.num_bytes()),
                               handles.empty() ? NULL : handles.data(),
                               static_cast<uint32_t>(handles.size()),
                               flags));
}

void ReadMessage(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  MojoHandle handle = MOJO_HANDLE_INVALID;
  MojoReadMessageFlags flags = MOJO_READ_MESSAGE_FLAG_NONE;

  if (!args.GetNext(&handle) ||
      !args.GetNext(&flags)) {
    return args.ThrowError();
  }

  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(
      handle, NULL, &num_bytes, NULL, &num_handles, flags);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED) {
    gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(
        info.GetIsolate());
    dictionary.Set("result", result);
    args.Return(dictionary);
  }

  v8::Handle<v8::ArrayBuffer> array_buffer = v8::ArrayBuffer::New(num_bytes);
  std::vector<MojoHandle> handles(num_handles);

  gin::ArrayBuffer buffer(args.isolate());
  ConvertFromV8(array_buffer, &buffer);
  CHECK(buffer.num_bytes() == num_bytes);

  result = MojoReadMessage(handle,
                           buffer.bytes(),
                           &num_bytes,
                           handles.empty() ? NULL : handles.data(),
                           &num_handles,
                           flags);

  CHECK(buffer.num_bytes() == num_bytes);
  CHECK(handles.size() == num_handles);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(info.GetIsolate());
  dictionary.Set("result", result);
  dictionary.Set("buffer", array_buffer);
  dictionary.Set("handles", handles);
  args.Return(dictionary);
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Core::kModuleName[] = "mojo/public/bindings/js/core";

v8::Local<v8::ObjectTemplate> Core::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();

    templ->Set(gin::StringToSymbol(isolate, "close"),
               v8::FunctionTemplate::New(Close));
    templ->Set(gin::StringToSymbol(isolate, "wait"),
               v8::FunctionTemplate::New(Wait));
    templ->Set(gin::StringToSymbol(isolate, "waitMany"),
               v8::FunctionTemplate::New(WaitMany));
    templ->Set(gin::StringToSymbol(isolate, "createMessagePipe"),
               v8::FunctionTemplate::New(CreateMessagePipe));
    templ->Set(gin::StringToSymbol(isolate, "writeMessage"),
               v8::FunctionTemplate::New(WriteMessage));
    templ->Set(gin::StringToSymbol(isolate, "readMessage"),
               v8::FunctionTemplate::New(ReadMessage));

    // TODO(vtl): Change name of "kInvalidHandle", now that there's no such C++
    // constant?
    templ->Set(gin::StringToSymbol(isolate, "kInvalidHandle"),
               gin::ConvertToV8(isolate, mojo::Handle()));

    templ->Set(gin::StringToSymbol(isolate, "RESULT_OK"),
               gin::ConvertToV8(isolate, MOJO_RESULT_OK));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_CANCELLED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_CANCELLED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_UNKNOWN"),
               gin::ConvertToV8(isolate, MOJO_RESULT_UNKNOWN));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_INVALID_ARGUMENT"),
               gin::ConvertToV8(isolate, MOJO_RESULT_INVALID_ARGUMENT));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_DEADLINE_EXCEEDED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_DEADLINE_EXCEEDED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_NOT_FOUND"),
               gin::ConvertToV8(isolate, MOJO_RESULT_NOT_FOUND));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_ALREADY_EXISTS"),
               gin::ConvertToV8(isolate, MOJO_RESULT_ALREADY_EXISTS));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_PERMISSION_DENIED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_PERMISSION_DENIED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_RESOURCE_EXHAUSTED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_RESOURCE_EXHAUSTED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_FAILED_PRECONDITION"),
               gin::ConvertToV8(isolate, MOJO_RESULT_FAILED_PRECONDITION));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_ABORTED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_ABORTED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_OUT_OF_RANGE"),
               gin::ConvertToV8(isolate, MOJO_RESULT_OUT_OF_RANGE));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_UNIMPLEMENTED"),
               gin::ConvertToV8(isolate, MOJO_RESULT_UNIMPLEMENTED));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_INTERNAL"),
               gin::ConvertToV8(isolate, MOJO_RESULT_INTERNAL));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_UNAVAILABLE"),
               gin::ConvertToV8(isolate, MOJO_RESULT_UNAVAILABLE));
    templ->Set(gin::StringToSymbol(isolate, "RESULT_DATA_LOSS"),
               gin::ConvertToV8(isolate, MOJO_RESULT_DATA_LOSS));

    templ->Set(gin::StringToSymbol(isolate, "DEADLINE_INDEFINITE"),
               gin::ConvertToV8(isolate, MOJO_DEADLINE_INDEFINITE));

    templ->Set(gin::StringToSymbol(isolate, "WAIT_FLAG_NONE"),
               gin::ConvertToV8(isolate, MOJO_WAIT_FLAG_NONE));
    templ->Set(gin::StringToSymbol(isolate, "WAIT_FLAG_READABLE"),
               gin::ConvertToV8(isolate, MOJO_WAIT_FLAG_READABLE));
    templ->Set(gin::StringToSymbol(isolate, "WAIT_FLAG_READABLE"),
               gin::ConvertToV8(isolate, MOJO_WAIT_FLAG_READABLE));
    templ->Set(gin::StringToSymbol(isolate, "WAIT_FLAG_EVERYTHING"),
               gin::ConvertToV8(isolate, MOJO_WAIT_FLAG_EVERYTHING));

    templ->Set(gin::StringToSymbol(isolate, "WRITE_MESSAGE_FLAG_NONE"),
               gin::ConvertToV8(isolate, MOJO_WRITE_MESSAGE_FLAG_NONE));

    templ->Set(gin::StringToSymbol(isolate, "READ_MESSAGE_FLAG_NONE"),
               gin::ConvertToV8(isolate, MOJO_READ_MESSAGE_FLAG_NONE));
    templ->Set(gin::StringToSymbol(isolate, "READ_MESSAGE_FLAG_MAY_DISCARD"),
               gin::ConvertToV8(isolate, MOJO_READ_MESSAGE_FLAG_MAY_DISCARD));

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ;
}

}  // namespace js
}  // namespace mojo
