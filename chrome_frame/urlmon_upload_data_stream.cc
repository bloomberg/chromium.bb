// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_upload_data_stream.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

void UrlmonUploadDataStream::Initialize(net::UploadData* upload_data) {
  upload_data_ = upload_data;
  request_body_stream_.reset(net::UploadDataStream::Create(upload_data, NULL));
  DCHECK(request_body_stream_.get());
}

STDMETHODIMP UrlmonUploadDataStream::Read(void* pv, ULONG cb, ULONG* read) {
  if (pv == NULL) {
    NOTREACHED();
    return E_POINTER;
  }

  // Have we already read past the end of the stream?
  if (request_body_stream_->eof()) {
    if (read) {
      *read = 0;
    }
    return S_FALSE;
  }

  uint64 total_bytes_to_copy = std::min(static_cast<uint64>(cb),
      static_cast<uint64>(request_body_stream_->buf_len()));

  uint64 bytes_copied = 0;

  char* write_pointer = reinterpret_cast<char*>(pv);
  while (bytes_copied < total_bytes_to_copy) {
    net::IOBuffer* buf = request_body_stream_->buf();

    // Make sure our length doesn't run past the end of the available data.
    size_t bytes_to_copy_now = static_cast<size_t>(
        std::min(static_cast<uint64>(request_body_stream_->buf_len()),
                 total_bytes_to_copy - bytes_copied));

    memcpy(write_pointer, buf->data(), bytes_to_copy_now);

    // Advance our copy tally
    bytes_copied += bytes_to_copy_now;

    // Advance our write pointer
    write_pointer += bytes_to_copy_now;

    // Advance the UploadDataStream read pointer:
    request_body_stream_->MarkConsumedAndFillBuffer(bytes_to_copy_now);
  }

  DCHECK(bytes_copied == total_bytes_to_copy);

  if (read) {
    *read = static_cast<ULONG>(total_bytes_to_copy);
  }

  return S_OK;
}

STDMETHODIMP UrlmonUploadDataStream::Seek(LARGE_INTEGER move, DWORD origin,
                                          ULARGE_INTEGER* new_pos) {
  // UploadDataStream is really not very seek-able, so for now allow
  // STREAM_SEEK_SETs to work with a 0 offset, but fail on everything else.
  if (origin == STREAM_SEEK_SET && move.QuadPart == 0) {
    if (request_body_stream_->position() != 0) {
      request_body_stream_.reset(
          net::UploadDataStream::Create(upload_data_, NULL));
      DCHECK(request_body_stream_.get());
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
  stat_stg->cbSize.QuadPart = upload_data_->GetContentLength();
  return S_OK;
}
