// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/profile/profile_service_impl.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace {

int BaseKeyForMojoKey(mojo::ProfileService::PathKey key) {
  switch(key) {
    case mojo::ProfileService::PATH_KEY_DIR_TEMP:
      return base::DIR_TEMP;
    default:
      return base::PATH_START;
  }
}

}  // namespace

namespace mojo {

ProfileServiceImpl::ProfileServiceImpl(ApplicationConnection* connection) {
}

ProfileServiceImpl::~ProfileServiceImpl() {
}

void ProfileServiceImpl::GetPath(
    PathKey key,
    const mojo::Callback<void(mojo::String)>& callback) {
  int base_key = BaseKeyForMojoKey(key);
  if (base_key == base::PATH_START) {
    callback.Run(mojo::String());
    return;
  }
  base::FilePath path;
  if (!PathService::Get(base_key, &path)) {
    callback.Run(mojo::String());
    return;
  }
#if defined(OS_POSIX)
  callback.Run(path.value());
#elif defined(OS_WIN)
  callback.Run(path.AsUTF8Unsafe());
#else
#error Not implemented
#endif
}

}  // namespace mojo
