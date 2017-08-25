// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/url_download_handler.h"

namespace content {

UrlDownloadHandler::InputStream::InputStream(
    std::unique_ptr<ByteStreamReader> stream_reader)
    : stream_reader_(std::move(stream_reader)) {}

UrlDownloadHandler::InputStream::InputStream(
    mojo::ScopedDataPipeConsumerHandle body)
    : body_(std::move(body)) {}

UrlDownloadHandler::InputStream::~InputStream() = default;

}  // namespace content
