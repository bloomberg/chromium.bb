// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <sys/socket.h>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "chrome/utility/image_writer/disk_unmounter_mac.h"
#include "chrome/utility/image_writer/error_messages.h"
#include "chrome/utility/image_writer/image_writer.h"

namespace image_writer {

static const char kAuthOpenPath[] = "/usr/libexec/authopen";

bool ImageWriter::IsValidDevice() {
  base::ScopedCFTypeRef<DASessionRef> session(DASessionCreate(NULL));
  base::ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(
      kCFAllocatorDefault, session, device_path_.value().c_str()));

  if (!disk)
    return false;

  base::ScopedCFTypeRef<CFDictionaryRef> disk_description(
      DADiskCopyDescription(disk));

  CFBooleanRef ejectable = base::mac::GetValueFromDictionary<CFBooleanRef>(
      disk_description, kDADiskDescriptionMediaEjectableKey);
  CFBooleanRef removable = base::mac::GetValueFromDictionary<CFBooleanRef>(
      disk_description, kDADiskDescriptionMediaRemovableKey);
  CFBooleanRef writable = base::mac::GetValueFromDictionary<CFBooleanRef>(
      disk_description, kDADiskDescriptionMediaWritableKey);
  CFBooleanRef whole = base::mac::GetValueFromDictionary<CFBooleanRef>(
      disk_description, kDADiskDescriptionMediaWholeKey);
  CFStringRef kind = base::mac::GetValueFromDictionary<CFStringRef>(
      disk_description, kDADiskDescriptionMediaKindKey);

  // A drive is valid if it is
  // - ejectable
  // - removable
  // - writable
  // - a whole drive
  // - it is of type IOMedia (external DVD drives and the like are IOCDMedia or
  //   IODVDMedia)
  return CFBooleanGetValue(ejectable) && CFBooleanGetValue(removable) &&
         CFBooleanGetValue(writable) && CFBooleanGetValue(whole) &&
         CFStringCompare(kind, CFSTR("IOMedia"), 0) == kCFCompareEqualTo;
}

void ImageWriter::UnmountVolumes(const base::Closure& continuation) {
  if (unmounter_ == NULL) {
    unmounter_.reset(new DiskUnmounterMac());
  }

  unmounter_->Unmount(
      device_path_.value(),
      continuation,
      base::Bind(
          &ImageWriter::Error, base::Unretained(this), error::kUnmountVolumes));
}

bool ImageWriter::OpenDevice() {
  base::LaunchOptions options = base::LaunchOptions();
  options.wait = false;

  // Create a socket pair for communication.
  int sockets[2];
  int result = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
  if (result == -1) {
    PLOG(ERROR) << "Unable to allocate socket pair.";
    return false;
  }
  base::ScopedFD parent_socket(sockets[0]);
  base::ScopedFD child_socket(sockets[1]);

  // Map the client socket to the client's STDOUT.
  base::FileHandleMappingVector fd_map;
  fd_map.push_back(std::pair<int, int>(child_socket.get(), STDOUT_FILENO));
  options.fds_to_remap = &fd_map;

  // Find the file path to open.
  base::FilePath real_device_path;
  if (device_path_.IsAbsolute()) {
    real_device_path = device_path_;
  } else {
    real_device_path = base::FilePath("/dev").Append(device_path_);
  }

  // Build the command line.
  std::string rdwr = base::StringPrintf("%d", O_RDWR);

  base::CommandLine cmd_line = base::CommandLine(base::FilePath(kAuthOpenPath));
  cmd_line.AppendSwitch("-stdoutpipe");
  // Using AppendSwitchNative will use an equal-symbol which we don't want.
  cmd_line.AppendArg("-o");
  cmd_line.AppendArg(rdwr);
  cmd_line.AppendArgPath(real_device_path);

  // Launch the process.
  base::ProcessHandle process_handle;
  if (!base::LaunchProcess(cmd_line, options, &process_handle)) {
    LOG(ERROR) << "Failed to launch authopen process.";
    return false;
  }

  // Receive a file descriptor from authopen which sends a single FD via
  // sendmsg and the SCM_RIGHTS extension.
  int fd = -1;
  const size_t kDataBufferSize = sizeof(struct cmsghdr) + sizeof(int);
  char data_buffer[kDataBufferSize];

  struct iovec io_vec[1];
  io_vec[0].iov_base = data_buffer;
  io_vec[0].iov_len = kDataBufferSize;

  const socklen_t kCmsgSocketSize =
      static_cast<socklen_t>(CMSG_SPACE(sizeof(int)));
  char cmsg_socket[kCmsgSocketSize];

  struct msghdr message = {0};
  message.msg_iov = io_vec;
  message.msg_iovlen = 1;
  message.msg_control = cmsg_socket;
  message.msg_controllen = kCmsgSocketSize;

  ssize_t size = HANDLE_EINTR(recvmsg(parent_socket.get(), &message, 0));
  if (size > 0) {
    struct cmsghdr* cmsg_socket_header = CMSG_FIRSTHDR(&message);

    if (cmsg_socket_header && cmsg_socket_header->cmsg_level == SOL_SOCKET &&
        cmsg_socket_header->cmsg_type == SCM_RIGHTS) {
      fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg_socket_header));
    }
  }

  device_file_ = base::File(fd);

  // Wait for the child.
  int child_exit_status;
  if (!base::WaitForExitCode(process_handle, &child_exit_status)) {
    LOG(ERROR) << "Unable to wait for child.";
    return false;
  }

  if (child_exit_status) {
    LOG(ERROR) << "Child process returned failure.";
    return false;
  }

  return device_file_.IsValid();
}

}  // namespace image_writer
