// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_file_stream_reader.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/platform_file.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "webkit/browser/fileapi/file_system_context.h"

using webkit_blob::FileStreamReader;

namespace {

// Called on the IO thread.
MTPDeviceAsyncDelegate* GetMTPDeviceDelegate(
    const fileapi::FileSystemURL& url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(
      url.filesystem_id());
}

void CallCompletionCallbackWithPlatformFileError(
    const net::CompletionCallback& callback,
    base::File::Error file_error) {
  callback.Run(net::FileErrorToNetError(file_error));
}

void CallInt64CompletionCallbackWithPlatformFileError(
    const net::Int64CompletionCallback& callback,
    base::File::Error file_error) {
  callback.Run(net::FileErrorToNetError(file_error));
}

void ReadBytes(
    const fileapi::FileSystemURL& url, net::IOBuffer* buf, int64 offset,
    int buf_len,
    const MTPDeviceAsyncDelegate::ReadBytesSuccessCallback& success_callback,
    const net::CompletionCallback& error_callback) {
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    error_callback.Run(net::ERR_FAILED);
    return;
  }

  delegate->ReadBytes(
      url.path(),
      make_scoped_refptr(buf),
      offset,
      buf_len,
      success_callback,
      base::Bind(&CallCompletionCallbackWithPlatformFileError, error_callback));
}

}  // namespace

MTPFileStreamReader::MTPFileStreamReader(
    fileapi::FileSystemContext* file_system_context,
    const fileapi::FileSystemURL& url,
    int64 initial_offset,
    const base::Time& expected_modification_time)
    : file_system_context_(file_system_context),
      url_(url),
      current_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      media_header_validated_(false),
      weak_factory_(this) {
}

MTPFileStreamReader::~MTPFileStreamReader() {
}

int MTPFileStreamReader::Read(net::IOBuffer* buf, int buf_len,
                              const net::CompletionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url_);
  if (!delegate)
    return net::ERR_FAILED;

  if (!media_header_validated_) {
    scoped_refptr<net::IOBuffer> header_buf(
        new net::IOBuffer(net::kMaxBytesToSniff));
    ReadBytes(url_, header_buf, 0, net::kMaxBytesToSniff,
              base::Bind(&MTPFileStreamReader::FinishValidateMediaHeader,
                         weak_factory_.GetWeakPtr(), header_buf,
                         make_scoped_refptr(buf), buf_len, callback),
              callback);
    return net::ERR_IO_PENDING;
  }

  ReadBytes(url_, buf, current_offset_, buf_len,
            base::Bind(&MTPFileStreamReader::FinishRead,
                       weak_factory_.GetWeakPtr(), callback),
            callback);

  return net::ERR_IO_PENDING;
}

int64 MTPFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url_);
  if (!delegate)
    return net::ERR_FAILED;

  delegate->GetFileInfo(
        url_.path(),
        base::Bind(&MTPFileStreamReader::FinishGetLength,
                   weak_factory_.GetWeakPtr(), callback),
        base::Bind(&CallInt64CompletionCallbackWithPlatformFileError,
                   callback));

  return net::ERR_IO_PENDING;
}

void MTPFileStreamReader::FinishValidateMediaHeader(
    net::IOBuffer* header_buf,
    net::IOBuffer* buf, int buf_len,
    const net::CompletionCallback& callback,
    const base::File::Info& file_info,
    int header_bytes_read) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK_GE(header_bytes_read, 0);
  base::File::Error error = NativeMediaFileUtil::BufferIsMediaHeader(
      header_buf, header_bytes_read);
  if (error != base::File::FILE_OK) {
    CallCompletionCallbackWithPlatformFileError(callback, error);
    return;
  }

  media_header_validated_ = true;

  // Complete originally requested read.
  ReadBytes(url_, buf, current_offset_, buf_len,
            base::Bind(&MTPFileStreamReader::FinishRead,
                       weak_factory_.GetWeakPtr(), callback),
            callback);
}

void MTPFileStreamReader::FinishRead(const net::CompletionCallback& callback,
                                     const base::File::Info& file_info,
                                     int bytes_read) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  DCHECK_GE(bytes_read, 0);
  current_offset_ += bytes_read;
  callback.Run(bytes_read);
}

void MTPFileStreamReader::FinishGetLength(
    const net::Int64CompletionCallback& callback,
    const base::File::Info& file_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  callback.Run(file_info.size);
}
