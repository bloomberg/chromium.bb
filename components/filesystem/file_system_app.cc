// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_app.h"

#include "mojo/application/public/cpp/application_connection.h"

namespace filesystem {

FileSystemApp::FileSystemApp() {}

FileSystemApp::~FileSystemApp() {}

bool FileSystemApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<FileSystem>(this);
  return true;
}

// |InterfaceFactory<Files>| implementation:
void FileSystemApp::Create(mojo::ApplicationConnection* connection,
                           mojo::InterfaceRequest<FileSystem> request) {
  new FileSystemImpl(connection, request.Pass());
}

}  // namespace filesystem
