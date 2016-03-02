// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_service/profile_service_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "mojo/shell/public/cpp/connection.h"

namespace profile {

ProfileServiceImpl::ProfileServiceImpl(
    mojo::Connection* connection,
    mojo::InterfaceRequest<ProfileService> request,
    base::FilePath base_profile_dir,
    filesystem::LockTable* lock_table)
    : binding_(this, std::move(request)),
      lock_table_(lock_table),
      path_(base_profile_dir) {
  if (!base::PathExists(path_))
    base::CreateDirectory(path_);
}

ProfileServiceImpl::~ProfileServiceImpl() {
}

void ProfileServiceImpl::GetDirectory(
    mojo::InterfaceRequest<filesystem::Directory> request) {
  new filesystem::DirectoryImpl(std::move(request),
                                path_,
                                scoped_ptr<base::ScopedTempDir>(),
                                lock_table_);
}

}  // namespace profile
