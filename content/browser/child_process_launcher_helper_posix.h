// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_

#include <memory>

namespace base {
class CommandLine;
}  // namespace base

namespace mojo {
namespace edk {
struct PlatformHandle;
}  // namespace mojo
}  // namespace edk

// Contains the common functionalities between the various POSIX child process
// launcher implementations.

namespace content {

class FileDescriptorInfo;

namespace internal {

std::unique_ptr<FileDescriptorInfo> CreateDefaultPosixFilesToMap(
    const base::CommandLine& command_line,
    int child_process_id,
    const mojo::edk::PlatformHandle& mojo_client_handle);

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_