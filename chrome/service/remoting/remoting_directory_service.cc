// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/remoting/remoting_directory_service.h"

RemotingDirectoryService::RemotingDirectoryService(Client* client)
    : client_(client) {
}

RemotingDirectoryService::~RemotingDirectoryService() {
  // TODO(hclam): Implement.
}

void RemotingDirectoryService::AddHost(const std::string& token) {
    // TODO(hclam): Implement.
}

void RemotingDirectoryService::CancelRequest() {
  // TODO(hclam): Implement.
}

void RemotingDirectoryService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // TODO(hclam): Implement.
}
