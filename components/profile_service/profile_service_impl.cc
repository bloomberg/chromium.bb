// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_service/profile_service_impl.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/message_loop_ref.h"

namespace profile {

ProfileServiceImpl::ProfileServiceImpl(
    const base::FilePath& base_profile_dir,
    const scoped_refptr<filesystem::LockTable>& lock_table)
    : lock_table_(lock_table), path_(base_profile_dir) {
  base::CreateDirectory(path_);
}

ProfileServiceImpl::~ProfileServiceImpl() {}

void ProfileServiceImpl::GetDirectory(filesystem::DirectoryRequest request,
                                      const GetDirectoryCallback& callback) {
  new filesystem::DirectoryImpl(std::move(request),
                                path_,
                                scoped_ptr<base::ScopedTempDir>(),
                                lock_table_);
  callback.Run();
}

void ProfileServiceImpl::GetSubDirectory(
    const mojo::String& sub_directory_path,
    filesystem::DirectoryRequest request,
    const GetSubDirectoryCallback& callback) {
  // Ensure that we've made |subdirectory| recursively under our profile.
  base::FilePath subdir = path_.Append(
#if defined(OS_WIN)
      base::UTF8ToWide(sub_directory_path.To<std::string>()));
#else
      sub_directory_path.To<std::string>());
#endif
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(subdir, &error)) {
    callback.Run(static_cast<filesystem::FileError>(error));
    return;
  }

  new filesystem::DirectoryImpl(std::move(request), subdir,
                                scoped_ptr<base::ScopedTempDir>(), lock_table_);
  callback.Run(filesystem::FileError::OK);
}

}  // namespace profile
