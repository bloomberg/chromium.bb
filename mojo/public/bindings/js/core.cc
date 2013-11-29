// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/core.h"

#include "base/bind.h"
#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/bindings/js/handle.h"

namespace mojo {
namespace js {

namespace {

gin::Dictionary CreateMessagePipe(const gin::Arguments& args) {
  MojoHandle handle_0 = MOJO_HANDLE_INVALID;
  MojoHandle handle_1 = MOJO_HANDLE_INVALID;
  MojoResult result = MojoCreateMessagePipe(&handle_0, &handle_1);
  CHECK(result == MOJO_RESULT_OK);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("handle0", handle_0);
  dictionary.Set("handle1", handle_1);
  return dictionary;
}

MojoResult WriteMessage(MojoHandle handle,
                        const gin::ArrayBufferView& buffer,
                        const std::vector<MojoHandle>& handles,
                        MojoWriteMessageFlags flags) {
  return MojoWriteMessage(handle,
                          buffer.bytes(),
                          static_cast<uint32_t>(buffer.num_bytes()),
                          handles.empty() ? NULL : handles.data(),
                          static_cast<uint32_t>(handles.size()),
                          flags);
}

gin::Dictionary ReadMessage(const gin::Arguments& args, MojoHandle handle,
                            MojoReadMessageFlags flags) {
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(
      handle, NULL, &num_bytes, NULL, &num_handles, flags);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED) {
    gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
    dictionary.Set("result", result);
    return dictionary;
  }

  v8::Handle<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(args.isolate(), num_bytes);
  std::vector<MojoHandle> handles(num_handles);

  gin::ArrayBuffer buffer;
  ConvertFromV8(args.isolate(), array_buffer, &buffer);
  CHECK(buffer.num_bytes() == num_bytes);

  result = MojoReadMessage(handle,
                           buffer.bytes(),
                           &num_bytes,
                           handles.empty() ? NULL : handles.data(),
                           &num_handles,
                           flags);

  CHECK(buffer.num_bytes() == num_bytes);
  CHECK(handles.size() == num_handles);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("result", result);
  dictionary.Set("buffer", array_buffer);
  dictionary.Set("handles", handles);
  return dictionary;
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Core::kModuleName[] = "mojo/public/bindings/js/core";

v8::Local<v8::ObjectTemplate> Core::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
     templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("close", mojo::CloseRaw)
        .SetMethod("wait", mojo::Wait)
        .SetMethod("waitMany", mojo::WaitMany<std::vector<mojo::Handle>,
                                              std::vector<MojoWaitFlags> >)
        .SetMethod("createMessagePipe", CreateMessagePipe)
        .SetMethod("writeMessage", WriteMessage)
        .SetMethod("readMessage", ReadMessage)

        // TODO(vtl): Change name of "kInvalidHandle", now that there's no such
        // C++ constant?
        .SetValue("kInvalidHandle", mojo::Handle())

        .SetValue("RESULT_OK", MOJO_RESULT_OK)
        .SetValue("RESULT_CANCELLED", MOJO_RESULT_CANCELLED)
        .SetValue("RESULT_UNKNOWN", MOJO_RESULT_UNKNOWN)
        .SetValue("RESULT_INVALID_ARGUMENT", MOJO_RESULT_INVALID_ARGUMENT)
        .SetValue("RESULT_DEADLINE_EXCEEDED", MOJO_RESULT_DEADLINE_EXCEEDED)
        .SetValue("RESULT_NOT_FOUND", MOJO_RESULT_NOT_FOUND)
        .SetValue("RESULT_ALREADY_EXISTS", MOJO_RESULT_ALREADY_EXISTS)
        .SetValue("RESULT_PERMISSION_DENIED", MOJO_RESULT_PERMISSION_DENIED)
        .SetValue("RESULT_RESOURCE_EXHAUSTED", MOJO_RESULT_RESOURCE_EXHAUSTED)
        .SetValue("RESULT_FAILED_PRECONDITION", MOJO_RESULT_FAILED_PRECONDITION)
        .SetValue("RESULT_ABORTED", MOJO_RESULT_ABORTED)
        .SetValue("RESULT_OUT_OF_RANGE", MOJO_RESULT_OUT_OF_RANGE)
        .SetValue("RESULT_UNIMPLEMENTED", MOJO_RESULT_UNIMPLEMENTED)
        .SetValue("RESULT_INTERNAL", MOJO_RESULT_INTERNAL)
        .SetValue("RESULT_UNAVAILABLE", MOJO_RESULT_UNAVAILABLE)
        .SetValue("RESULT_DATA_LOSS", MOJO_RESULT_DATA_LOSS)

        .SetValue("DEADLINE_INDEFINITE", MOJO_DEADLINE_INDEFINITE)

        .SetValue("WAIT_FLAG_NONE", MOJO_WAIT_FLAG_NONE)
        .SetValue("WAIT_FLAG_READABLE", MOJO_WAIT_FLAG_READABLE)
        .SetValue("WAIT_FLAG_READABLE", MOJO_WAIT_FLAG_READABLE)
        .SetValue("WAIT_FLAG_EVERYTHING", MOJO_WAIT_FLAG_EVERYTHING)

        .SetValue("WRITE_MESSAGE_FLAG_NONE", MOJO_WRITE_MESSAGE_FLAG_NONE)

        .SetValue("READ_MESSAGE_FLAG_NONE", MOJO_READ_MESSAGE_FLAG_NONE)
        .SetValue("READ_MESSAGE_FLAG_MAY_DISCARD",
                  MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)
        .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ;
}

}  // namespace js
}  // namespace mojo
