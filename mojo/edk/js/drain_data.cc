// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/js/drain_data.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/per_context_data.h"
#include "gin/per_isolate_data.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace edk {
namespace js {

DrainData::DrainData(v8::Isolate* isolate, mojo::Handle handle)
    : isolate_(isolate),
      handle_(DataPipeConsumerHandle(handle.value())),
      handle_watcher_(FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  v8::Local<v8::Context> context(isolate_->GetCurrentContext());
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();

  WaitForData();
}

v8::Local<v8::Value> DrainData::GetPromise() {
  CHECK(resolver_.IsEmpty());
  v8::Local<v8::Promise::Resolver> resolver(
      v8::Promise::Resolver::New(isolate_));
  resolver_.Reset(isolate_, resolver);
  return resolver->GetPromise();
}

DrainData::~DrainData() {
  resolver_.Reset();
}

void DrainData::WaitForData() {
  handle_watcher_.Watch(
      handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&DrainData::DataReady, base::Unretained(this)));
}

void DrainData::DataReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    DeliverData(result);
    return;
  }
  while (result == MOJO_RESULT_OK) {
    result = ReadData();
    if (result == MOJO_RESULT_SHOULD_WAIT)
      WaitForData();
    else if (result != MOJO_RESULT_OK)
      DeliverData(result);
  }
}

MojoResult DrainData::ReadData() {
  const void* buffer;
  uint32_t num_bytes = 0;
  MojoResult result =
      handle_->BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return result;
  const char* p = static_cast<const char*>(buffer);
  data_buffers_.push_back(std::make_unique<DataBuffer>(p, p + num_bytes));
  return handle_->EndReadData(num_bytes);
}

void DrainData::DeliverData(MojoResult result) {
  if (!runner_) {
    delete this;
    return;
  }

  size_t total_bytes = 0;
  for (unsigned i = 0; i < data_buffers_.size(); i++)
    total_bytes += data_buffers_[i]->size();

  // Create a total_bytes length ArrayBuffer return value.
  gin::Runner::Scope scope(runner_.get());
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate_, total_bytes);
  gin::ArrayBuffer buffer;
  ConvertFromV8(isolate_, array_buffer, &buffer);
  CHECK_EQ(total_bytes, buffer.num_bytes());

  // Copy the data_buffers into the ArrayBuffer.
  char* array_buffer_ptr = static_cast<char*>(buffer.bytes());
  size_t offset = 0;
  for (size_t i = 0; i < data_buffers_.size(); i++) {
    size_t num_bytes = data_buffers_[i]->size();
    if (num_bytes == 0)
      continue;
    const char* data_buffer_ptr = &((*data_buffers_[i])[0]);
    memcpy(array_buffer_ptr + offset, data_buffer_ptr, num_bytes);
    offset += num_bytes;
  }

  // The "settled" value of the promise always includes all of the data
  // that was read before either an error occurred or the remote pipe handle
  // was closed. The latter is indicated by MOJO_RESULT_FAILED_PRECONDITION.

  v8::Local<v8::Promise::Resolver> resolver(
      v8::Local<v8::Promise::Resolver>::New(isolate_, resolver_));

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(isolate_);
  dictionary.Set("result", result);
  dictionary.Set("buffer", array_buffer);
  v8::Local<v8::Value> settled_value(ConvertToV8(isolate_, dictionary));

  if (result == MOJO_RESULT_FAILED_PRECONDITION)
    resolver->Resolve(settled_value);
  else
    resolver->Reject(settled_value);

  delete this;
}

}  // namespace js
}  // namespace edk
}  // namespace mojo
