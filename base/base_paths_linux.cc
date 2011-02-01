// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <unistd.h>
#if defined(OS_FREEBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/nix/xdg_util.h"

namespace base {

#if defined(OS_LINUX)
const char kSelfExe[] = "/proc/self/exe";
#elif defined(OS_SOLARIS)
const char kSelfExe[] = getexecname();
#endif

// The name of this file relative to the source root. This is used for checking
// that the source checkout is in the correct place.
static const char kThisSourceFile[] = "base/base_paths_linux.cc";

bool PathProviderPosix(int key, FilePath* result) {
  FilePath path;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {  // TODO(evanm): is this correct?
#if defined(OS_LINUX)
      FilePath bin_dir;
      if (!file_util::ReadSymbolicLink(FilePath(kSelfExe), &bin_dir)) {
        NOTREACHED() << "Unable to resolve " << kSelfExe << ".";
        return false;
      }
      *result = bin_dir;
      return true;
#elif defined(OS_FREEBSD)
      int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
      char bin_dir[PATH_MAX + 1];
      size_t length = sizeof(bin_dir);
      int error = sysctl(name, 4, bin_dir, &length, NULL, 0);
      if (error < 0 || length == 0 || strlen(bin_dir) == 0) {
        NOTREACHED() << "Unable to resolve path.";
        return false;
      }
      bin_dir[strlen(bin_dir)] = 0;
      *result = FilePath(bin_dir);
      return true;
#endif
    }
    case base::DIR_SOURCE_ROOT: {
      // Allow passing this in the environment, for more flexibility in build
      // tree configurations (sub-project builds, gyp --output_dir, etc.)
      scoped_ptr<base::Environment> env(base::Environment::Create());
      std::string cr_source_root;
      if (env->GetVar("CR_SOURCE_ROOT", &cr_source_root)) {
        path = FilePath(cr_source_root);
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        } else {
          LOG(WARNING) << "CR_SOURCE_ROOT is set, but it appears to not "
                       << "point to the correct source root directory.";
        }
      }
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  out/{Debug|Release}/net_unittest
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName();
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        }
      }
      // In a case of WebKit-only checkout, executable files are put into
      // <root of checkout>/out/{Debug|Release}, and we should return
      // <root of checkout>/Source/WebKit/chromium for DIR_SOURCE_ROOT.
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName().Append("Source/WebKit/chromium");
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        }
      }
      // If that failed (maybe the build output is symlinked to a different
      // drive) try assuming the current directory is the source root.
      if (file_util::GetCurrentDirectory(&path) &&
          file_util::PathExists(path.Append(kThisSourceFile))) {
        *result = path;
        return true;
      }
      LOG(ERROR) << "Couldn't find your source root.  "
                 << "Try running from your chromium/src directory.";
      return false;
    }
    case base::DIR_CACHE:
      scoped_ptr<base::Environment> env(base::Environment::Create());
      FilePath cache_dir(base::nix::GetXDGDirectory(env.get(), "XDG_CACHE_HOME",
                                                    ".cache"));
      *result = cache_dir;
      return true;
  }
  return false;
}

}  // namespace base
