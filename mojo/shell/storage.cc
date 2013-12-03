// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/storage.h"

#include "base/environment.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#endif

namespace mojo {
namespace shell {

Storage::Storage() {
#if defined(OS_WIN)
  CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &profile_path_));
  profile_path_ = profile_path_.Append(FILE_PATH_LITERAL("mojo_shell"));
#elif defined(OS_LINUX)
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(
      base::nix::GetXDGDirectory(env.get(),
                                 base::nix::kXdgConfigHomeEnvVar,
                                 base::nix::kDotConfigDir));
  profile_path_ = config_dir.Append("mojo_shell");
#elif defined(OS_MACOSX)
  CHECK(PathService::Get(base::DIR_APP_DATA, &profile_path_));
  profile_path_ = profile_path_.Append("Chromium Mojo Shell");
#elif defined(OS_ANDROID)
  CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &profile_path_));
  profile_path_ = profile_path_.Append(FILE_PATH_LITERAL("mojo_shell"));
#else
  NOTIMPLEMENTED();
#endif

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);
}

Storage::~Storage() {
}

}  // namespace shell
}  // namespace mojo
