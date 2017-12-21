// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/sync_load_response.h"

namespace content {

SyncLoadResponse::SyncLoadResponse() {}

SyncLoadResponse::SyncLoadResponse(SyncLoadResponse&& other) {
  *this = std::move(other);
}

SyncLoadResponse::~SyncLoadResponse() {}

SyncLoadResponse& SyncLoadResponse::operator=(SyncLoadResponse&& other) {
  this->info = other.info;
  this->error_code = other.error_code;
  this->cors_error = other.cors_error;
  this->url = std::move(other.url);
  this->data = std::move(other.data);
  this->downloaded_file_length = other.downloaded_file_length;
  this->downloaded_tmp_file = std::move(other.downloaded_tmp_file);
  return *this;
}

}  // namespace content
