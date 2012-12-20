// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_upload_data_stream.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"

namespace {

// Creates UploadDataStream from UploadData.
net::UploadDataStream* CreateUploadDataStream(net::UploadData* upload_data) {
  net::UploadDataStream* upload_data_stream = NULL;
  const ScopedVector<net::UploadElement>& elements = upload_data->elements();

  if (upload_data->is_chunked()) {
    // Use AppendChunk when data is chunked.
    upload_data_stream = new net::UploadDataStream(
        net::UploadDataStream::CHUNKED, upload_data->identifier());

    for (size_t i = 0; i < elements.size(); ++i) {
      const net::UploadElement& element = *elements[i];
      const bool is_last_chunk =
          i == elements.size() - 1 && upload_data->last_chunk_appended();
      DCHECK_EQ(net::UploadElement::TYPE_BYTES, element.type());
      upload_data_stream->AppendChunk(element.bytes(), element.bytes_length(),
                                      is_last_chunk);
    }
  } else {
    // Not chunked.
    ScopedVector<net::UploadElementReader> element_readers;
    for (size_t i = 0; i < elements.size(); ++i) {
      const net::UploadElement& element = *elements[i];
      net::UploadElementReader* reader = NULL;
      switch (element.type()) {
        case net::UploadElement::TYPE_BYTES:
          reader = new net::UploadBytesElementReader(element.bytes(),
                                                     element.bytes_length());
          break;
        case net::UploadElement::TYPE_FILE:
          reader = new net::UploadFileElementReaderSync(
              element.file_path(),
              element.file_range_offset(),
              element.file_range_length(),
              element.expected_file_modification_time());
          break;
      }
      DCHECK(reader);
      element_readers.push_back(reader);
    }
    upload_data_stream = new net::UploadDataStream(&element_readers,
                                                   upload_data->identifier());
  }
  return upload_data_stream;
}

}  // namespace

void UrlmonUploadDataStream::Initialize(net::UploadData* upload_data) {
  upload_data_ = upload_data;
  request_body_stream_.reset(CreateUploadDataStream(upload_data));
  const int result = request_body_stream_->Init(net::CompletionCallback());
  DCHECK_EQ(net::OK, result);
}

STDMETHODIMP UrlmonUploadDataStream::Read(void* pv, ULONG cb, ULONG* read) {
  if (pv == NULL) {
    NOTREACHED();
    return E_POINTER;
  }

  // Have we already read past the end of the stream?
  if (request_body_stream_->IsEOF()) {
    if (read) {
      *read = 0;
    }
    return S_FALSE;
  }

  // The data in request_body_stream_ can be smaller than 'cb' so it's not
  // guaranteed that we'll be able to read total_bytes_to_copy bytes.
  uint64 total_bytes_to_copy = cb;

  uint64 bytes_copied = 0;

  char* write_pointer = reinterpret_cast<char*>(pv);
  while (bytes_copied < total_bytes_to_copy) {
    size_t bytes_to_copy_now = total_bytes_to_copy - bytes_copied;

    scoped_refptr<net::IOBufferWithSize> buf(
        new net::IOBufferWithSize(bytes_to_copy_now));
    int bytes_read = request_body_stream_->Read(buf, buf->size(),
                                                net::CompletionCallback());
    DCHECK_NE(net::ERR_IO_PENDING, bytes_read);
    if (bytes_read == 0)  // Reached the end of the stream.
      break;

    memcpy(write_pointer, buf->data(), bytes_read);

    // Advance our copy tally
    bytes_copied += bytes_read;

    // Advance our write pointer
    write_pointer += bytes_read;
  }

  DCHECK_LE(bytes_copied, total_bytes_to_copy);

  if (read) {
    *read = static_cast<ULONG>(bytes_copied);
  }

  return S_OK;
}

STDMETHODIMP UrlmonUploadDataStream::Seek(LARGE_INTEGER move, DWORD origin,
                                          ULARGE_INTEGER* new_pos) {
  // UploadDataStream is really not very seek-able, so for now allow
  // STREAM_SEEK_SETs to work with a 0 offset, but fail on everything else.
  if (origin == STREAM_SEEK_SET && move.QuadPart == 0) {
    if (request_body_stream_->position() != 0) {
      request_body_stream_.reset(CreateUploadDataStream(upload_data_));
      const int result = request_body_stream_->Init(net::CompletionCallback());
      DCHECK_EQ(net::OK, result);
    }
    if (new_pos) {
      new_pos->QuadPart = 0;
    }
    return S_OK;
  }

  DCHECK(false) << __FUNCTION__;
  return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP UrlmonUploadDataStream::Stat(STATSTG *stat_stg,
                                          DWORD grf_stat_flag) {
  if (stat_stg == NULL)
    return E_POINTER;

  memset(stat_stg, 0, sizeof(STATSTG));
  if (0 == (grf_stat_flag & STATFLAG_NONAME)) {
    const wchar_t kStreamBuffer[] = L"PostStream";
    stat_stg->pwcsName =
        static_cast<wchar_t*>(::CoTaskMemAlloc(sizeof(kStreamBuffer)));
    lstrcpy(stat_stg->pwcsName, kStreamBuffer);
  }
  stat_stg->type = STGTY_STREAM;
  stat_stg->cbSize.QuadPart = request_body_stream_->size();
  return S_OK;
}
