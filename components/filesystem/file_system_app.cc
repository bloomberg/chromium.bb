// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_app.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace filesystem {

FileSystemApp::FileSystemApp() : app_(nullptr), in_shutdown_(false) {}

FileSystemApp::~FileSystemApp() {}

void FileSystemApp::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
}

bool FileSystemApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<FileSystem>(this);
  return true;
}

void FileSystemApp::RegisterDirectoryToClient(DirectoryImpl* directory,
                                              FileSystemClientPtr client) {
  directory->set_connection_error_handler(
      base::Bind(&FileSystemApp::OnDirectoryConnectionError,
                 base::Unretained(this),
                 directory));
  client_mapping_.emplace_back(directory, client.Pass());
}

bool FileSystemApp::OnShellConnectionError() {
  if (client_mapping_.empty()) {
    // If we have no current connections, we can shutdown immediately.
    return true;
  }

  in_shutdown_ = true;

  // We have live connections. Send a notification to each one indicating that
  // they should shutdown.
  for (std::vector<Client>::iterator it = client_mapping_.begin();
       it != client_mapping_.end(); ++it) {
    it->fs_client_->OnFileSystemShutdown();
  }

  return false;
}

// |InterfaceFactory<Files>| implementation:
void FileSystemApp::Create(mojo::ApplicationConnection* connection,
                           mojo::InterfaceRequest<FileSystem> request) {
  new FileSystemImpl(this, connection, request.Pass());
}

void FileSystemApp::OnDirectoryConnectionError(DirectoryImpl* directory) {
  for (std::vector<Client>::iterator it = client_mapping_.begin();
       it != client_mapping_.end(); ++it) {
    if (it->directory_ == directory) {
      client_mapping_.erase(it);

      if (in_shutdown_ && client_mapping_.empty()) {
        // We just cleared the last directory after our shell connection went
        // away. Time to shut ourselves down.
        app_->Quit();
      }

      return;
    }
  }
}

FileSystemApp::Client::Client(DirectoryImpl* directory,
                              FileSystemClientPtr fs_client)
    : directory_(directory),
      fs_client_(fs_client.Pass()) {
}

FileSystemApp::Client::Client(Client&& rhs)
    : directory_(rhs.directory_),
      fs_client_(rhs.fs_client_.Pass()) {
}

FileSystemApp::Client::~Client() {}

FileSystemApp::Client& FileSystemApp::Client::operator=(
    FileSystemApp::Client&& rhs) {
  directory_ = rhs.directory_;
  fs_client_ = rhs.fs_client_.Pass();
  return *this;
}

}  // namespace filesystem
