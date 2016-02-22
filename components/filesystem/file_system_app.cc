// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_app.h"

#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"

namespace filesystem {

FileSystemApp::FileSystemApp()
    : shell_(nullptr), lock_table_(new LockTable) {}

FileSystemApp::~FileSystemApp() {}

void FileSystemApp::Initialize(mojo::Shell* shell, const std::string& url,
                               uint32_t id) {
  shell_ = shell;
  tracing_.Initialize(shell, url);
}

bool FileSystemApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<FileSystem>(this);
  return true;
}

// |InterfaceFactory<Files>| implementation:
void FileSystemApp::Create(mojo::Connection* connection,
                           mojo::InterfaceRequest<FileSystem> request) {
  new FileSystemImpl(connection, std::move(request), lock_table_.get());
}

}  // namespace filesystem
