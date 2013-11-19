// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/core.h"

#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"
#include "mojo/public/bindings/js/handle.h"

namespace mojo {
namespace js {

namespace {

void Close(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle = mojo::kInvalidHandle;
  if (!args.GetNext(&handle))
    return args.ThrowError();

  args.Return(mojo::Close(handle));
}

void Wait(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle = mojo::kInvalidHandle;
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

  if (handles.size() != flags.size())
    return args.ThrowTypeError("Arrays must have the same length.");

  args.Return(mojo::WaitMany(handles.data(), flags.data(),
                             static_cast<uint32_t>(handles.size()), deadline));
}

void CreateMessagePipe(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle_0 = mojo::kInvalidHandle;
  mojo::Handle handle_1 = mojo::kInvalidHandle;
  MojoResult result = mojo::CreateMessagePipe(&handle_0, &handle_1);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(info.GetIsolate());
  dictionary.Set("result", result);
  dictionary.Set("handle0", handle_0);
  dictionary.Set("handle1", handle_1);
  args.Return(dictionary);
}

void WriteMessage(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle = mojo::kInvalidHandle;
  gin::ArrayBufferView buffer(args.isolate());
  std::vector<mojo::Handle> handles;
  MojoWriteMessageFlags flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  if (!args.GetNext(&handle) ||
      !args.GetNext(&buffer) ||
      !args.GetNext(&handles) ||
      !args.GetNext(&flags)) {
    return args.ThrowError();
  }

  args.Return(mojo::WriteMessage(handle, buffer.bytes(),
                                 static_cast<uint32_t>(buffer.num_bytes()),
                                 handles.data(),
                                 static_cast<uint32_t>(handles.size()),
                                 flags));
}

void ReadMessage(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  mojo::Handle handle = mojo::kInvalidHandle;
  gin::ArrayBufferView buffer(args.isolate());
  uint32_t num_handles = 0;
  MojoReadMessageFlags flags = MOJO_READ_MESSAGE_FLAG_NONE;

  if (!args.GetNext(&handle) ||
      !args.GetNext(&buffer) ||
      !args.GetNext(&num_handles) ||
      !args.GetNext(&flags)) {
    return args.ThrowError();
  }

  uint32_t num_bytes = static_cast<uint32_t>(buffer.num_bytes());
  std::vector<mojo::Handle> handles(num_handles);
  MojoResult result = mojo::ReadMessage(handle, buffer.bytes(), &num_bytes,
                                        handles.data(), &num_handles, flags);
  handles.resize(num_handles);

  // TODO(abarth): We should benchmark this codepath to make sure it's ok to
  // allocate all this memory on each read.
  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(info.GetIsolate());
  dictionary.Set("result", result);
  dictionary.Set("bytesRead", num_bytes);
  dictionary.Set("handles", handles);
  args.Return(dictionary);
}

gin::WrapperInfo g_core_wrapper_info = {};

}  // namespace

const char Core::kModuleName[] = "mojo/public/bindings/js/core";

v8::Local<v8::ObjectTemplate> Core::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_core_wrapper_info);

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

    templ->Set(gin::StringToSymbol(isolate, "kInvalidHandle"),
               gin::ConvertToV8(isolate, mojo::kInvalidHandle));

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

    data->SetObjectTemplate(&g_core_wrapper_info, templ);
  }

  return templ;
}

}  // namespace js
}  // namespace mojo
