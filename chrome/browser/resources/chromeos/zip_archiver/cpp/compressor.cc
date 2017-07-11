// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compressor.h"

#include <cstring>
#include <ctime>
#include <sstream>

#include "compressor_archive_minizip.h"
#include "compressor_io_javascript_stream.h"
#include "request.h"

namespace {

// An internal implementation of JavaScriptCompressorRequestorInterface.
class JavaScriptCompressorRequestor
    : public JavaScriptCompressorRequestorInterface {
 public:
  explicit JavaScriptCompressorRequestor(Compressor* compressor)
      : compressor_(compressor) {}

  virtual void WriteChunkRequest(int64_t offset,
                                 int64_t length,
                                 const pp::VarArrayBuffer& buffer) {
    compressor_->message_sender()->SendWriteChunk(compressor_->compressor_id(),
                                                  buffer, offset, length);
  }

  virtual void ReadFileChunkRequest(int64_t length) {
    compressor_->message_sender()->SendReadFileChunk(
        compressor_->compressor_id(), length);
  }

 private:
  Compressor* compressor_;
};

}  // namespace

Compressor::Compressor(const pp::InstanceHandle& instance_handle,
                       int compressor_id,
                       JavaScriptMessageSenderInterface* message_sender)
    : compressor_id_(compressor_id),
      message_sender_(message_sender),
      worker_(instance_handle),
      callback_factory_(this) {
  requestor_ = new JavaScriptCompressorRequestor(this);
  compressor_stream_ = new CompressorIOJavaScriptStream(requestor_);
  compressor_archive_ = new CompressorArchiveMinizip(compressor_stream_);
}

Compressor::~Compressor() {
  worker_.Join();
  delete compressor_archive_;
  delete compressor_stream_;
  delete requestor_;
}

bool Compressor::Init() {
  return worker_.Start();
}

void Compressor::CreateArchive() {
  if (!compressor_archive_->CreateArchive()) {
    message_sender_->SendCompressorError(compressor_id_,
                                         compressor_archive_->error_message());
    return;
  }
  message_sender_->SendCreateArchiveDone(compressor_id_);
}

void Compressor::AddToArchive(const pp::VarDictionary& dictionary) {
  worker_.message_loop().PostWork(callback_factory_.NewCallback(
      &Compressor::AddToArchiveCallback, dictionary));
}

void Compressor::AddToArchiveCallback(int32_t,
                                      const pp::VarDictionary& dictionary) {
  PP_DCHECK(dictionary.Get(request::key::kPathname).is_string());
  std::string pathname = dictionary.Get(request::key::kPathname).AsString();

  PP_DCHECK(dictionary.Get(request::key::kFileSize).is_string());
  int64_t file_size =
      request::GetInt64FromString(dictionary, request::key::kFileSize);
  PP_DCHECK(file_size >= 0);

  PP_DCHECK(dictionary.Get(request::key::kIsDirectory).is_bool());
  bool is_directory = dictionary.Get(request::key::kIsDirectory).AsBool();

  PP_DCHECK(dictionary.Get(request::key::kModificationTime).is_string());
  // Since modification_time is milliseconds, we hold the value in int64_t.
  int64_t modification_time = static_cast<int64_t>(
      request::GetInt64FromString(dictionary, request::key::kModificationTime));

  if (!compressor_archive_->AddToArchive(pathname, file_size, modification_time,
                                         is_directory)) {
    message_sender_->SendCompressorError(compressor_id_,
                                         compressor_archive_->error_message());
    return;
  }
  message_sender_->SendAddToArchiveDone(compressor_id_);
}

void Compressor::ReadFileChunkDone(const pp::VarDictionary& dictionary) {
  PP_DCHECK(dictionary.Get(request::key::kLength).is_string());
  int64_t read_bytes =
      request::GetInt64FromString(dictionary, request::key::kLength);

  PP_DCHECK(dictionary.Get(request::key::kChunkBuffer).is_array_buffer());
  pp::VarArrayBuffer array_buffer(dictionary.Get(request::key::kChunkBuffer));

  compressor_stream_->ReadFileChunkDone(read_bytes, &array_buffer);
}

void Compressor::WriteChunkDone(const pp::VarDictionary& dictionary) {
  PP_DCHECK(dictionary.Get(request::key::kLength).is_string());
  int64_t written_bytes =
      request::GetInt64FromString(dictionary, request::key::kLength);

  compressor_stream_->WriteChunkDone(written_bytes);
}

void Compressor::CloseArchive(const pp::VarDictionary& dictionary) {
  PP_DCHECK(dictionary.Get(request::key::kHasError).is_bool());
  bool has_error = dictionary.Get(request::key::kHasError).AsBool();

  // If an error has occurred, no more write chunk requests are sent and
  // CloseArchive() can be safely called in the main thread.
  if (has_error) {
    if (!compressor_archive_->CloseArchive(has_error)) {
      message_sender_->SendCompressorError(
          compressor_id_, compressor_archive_->error_message());
      return;
    }
    message_sender_->SendCloseArchiveDone(compressor_id_);
  } else {
    worker_.message_loop().PostWork(callback_factory_.NewCallback(
        &Compressor::CloseArchiveCallback, has_error));
  }
}

void Compressor::CloseArchiveCallback(int32_t, bool has_error) {
  if (!compressor_archive_->CloseArchive(has_error)) {
    message_sender_->SendCompressorError(compressor_id_,
                                         compressor_archive_->error_message());
    return;
  }
  message_sender_->SendCloseArchiveDone(compressor_id_);
}
