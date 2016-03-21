// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_SERVICE_PROFILE_SERVICE_IMPL_H_
#define COMPONENTS_PROFILE_SERVICE_PROFILE_SERVICE_IMPL_H_

#include "base/files/file_path.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/profile_service/public/interfaces/profile.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/connection.h"

namespace filesystem {
class LockTable;
}

namespace mojo {
class MessageLoopRef;
}

namespace profile {

// A service which serves directories to callers.
class ProfileServiceImpl : public ProfileService {
 public:
  ProfileServiceImpl(const base::FilePath& base_profile_dir,
                     const scoped_refptr<filesystem::LockTable>& lock_table);
  ~ProfileServiceImpl() override;

  // Overridden from ProfileService:
  void GetDirectory(filesystem::DirectoryRequest request,
                    const GetDirectoryCallback& callback) override;
  void GetSubDirectory(const mojo::String& sub_directory_path,
                       filesystem::DirectoryRequest request,
                       const GetSubDirectoryCallback& callback) override;

 private:
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ProfileServiceImpl);
};

}  // namespace profile

#endif  // COMPONENTS_PROFILE_SERVICE_PROFILE_SERVICE_IMPL_H_
