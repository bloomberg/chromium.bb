// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_impl.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/file_system_app.h"
#include "mojo/shell/public/cpp/connection.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#elif defined(OS_ANDROID)
#include "base/base_paths_android.h"
#include "base/path_service.h"
#elif defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#include "base/path_service.h"
#endif

namespace filesystem {

namespace {

const char kEscapeChar = ',';

const char kUserDataDir[] = "user-data-dir";

}  // namespace filesystem

FileSystemImpl::FileSystemImpl(FileSystemApp* app,
                               mojo::Connection* connection,
                               mojo::InterfaceRequest<FileSystem> request)
    : app_(app),
      remote_application_url_(connection->GetRemoteApplicationURL()),
      binding_(this, std::move(request)) {}

FileSystemImpl::~FileSystemImpl() {
}

void FileSystemImpl::OpenFileSystem(const mojo::String& file_system,
                                    mojo::InterfaceRequest<Directory> directory,
                                    FileSystemClientPtr client,
                                    const OpenFileSystemCallback& callback) {
  // Set only if the |DirectoryImpl| will own a temporary directory.
  scoped_ptr<base::ScopedTempDir> temp_dir;
  base::FilePath path;
  if (file_system.get() == std::string("temp")) {
    temp_dir.reset(new base::ScopedTempDir);
    CHECK(temp_dir->CreateUniqueTempDir());
    path = temp_dir->path();
  } else if (file_system.get() == std::string("origin")) {
    base::FilePath base_profile_dir = GetSystemProfileDir();

    // Sanitize the url for disk access.
    //
    // TODO(erg): While it's currently impossible, we need to deal with http://
    // URLs that have a path. (Or make the decision that these file systems are
    // path bound, not origin bound.)
    std::string sanitized_origin;
    BuildSanitizedOrigin(remote_application_url_, &sanitized_origin);

#if defined(OS_WIN)
    path = base_profile_dir.Append(base::UTF8ToWide(sanitized_origin));
#else
    path = base_profile_dir.Append(sanitized_origin);
#endif
    if (!base::PathExists(path))
      base::CreateDirectory(path);
  }

  if (!path.empty()) {
    DirectoryImpl* dir_impl =
        new DirectoryImpl(std::move(directory), path, std::move(temp_dir));
    app_->RegisterDirectoryToClient(dir_impl, std::move(client));
    callback.Run(FileError::OK);
  } else {
    callback.Run(FileError::FAILED);
  }
}

base::FilePath FileSystemImpl::GetSystemProfileDir() const {
  base::FilePath path;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserDataDir)) {
    path = command_line->GetSwitchValuePath(kUserDataDir);
  } else {
#if defined(OS_WIN)
    CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
    path = path.Append(FILE_PATH_LITERAL("mandoline"));
#elif defined(OS_LINUX)
    scoped_ptr<base::Environment> env(base::Environment::Create());
    base::FilePath config_dir(
        base::nix::GetXDGDirectory(env.get(),
                                   base::nix::kXdgConfigHomeEnvVar,
                                   base::nix::kDotConfigDir));
    path = config_dir.Append("mandoline");
#elif defined(OS_MACOSX)
    CHECK(PathService::Get(base::DIR_APP_DATA, &path));
    path = path.Append("Mandoline Shell");
#elif defined(OS_ANDROID)
    CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
    path = path.Append(FILE_PATH_LITERAL("mandoline"));
#else
    NOTIMPLEMENTED();
#endif
  }

  if (!base::PathExists(path))
    base::CreateDirectory(path);

  return path;
}

void FileSystemImpl::BuildSanitizedOrigin(
    const std::string& origin,
    std::string* sanitized_origin) {
  // We take the origin string, and encode it in a way safe for filesystem
  // access. This is vaguely based on //net/tools/dump_cache/
  // url_to_filename_encoder.h; that file strips out schemes, and does weird
  // things with subdirectories. We do follow the basic algorithm used there,
  // including using ',' as our escape character.
  for (size_t i = 0; i < origin.length(); ++i) {
    unsigned char ch = origin[i];
    char encoded[3];
    int encoded_len;
    if ((ch == '_') || (ch == '.') || (ch == '=') || (ch == '+') ||
        (ch == '-') || (('0' <= ch) && (ch <= '9')) ||
        (('A' <= ch) && (ch <= 'Z')) || (('a' <= ch) && (ch <= 'z'))) {
      encoded[0] = ch;
      encoded_len = 1;
    } else {
      encoded[0] = kEscapeChar;
      encoded[1] = ch / 16;
      encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
      encoded[2] = ch % 16;
      encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
      encoded_len = 3;
    }
    sanitized_origin->append(encoded, encoded_len);
  }
}

}  // namespace filesystem
