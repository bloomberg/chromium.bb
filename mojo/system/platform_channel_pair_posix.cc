// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel_pair.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/system/platform_channel.h"

namespace mojo {
namespace system {

namespace {

const char kMojoChannelDescriptorSwitch[] = "mojo-channel-descriptor";

bool IsTargetDescriptorUsed(
    const base::FileHandleMappingVector& file_handle_mapping,
    int target_fd) {
  for (size_t i = 0; i < file_handle_mapping.size(); i++) {
    if (file_handle_mapping[i].second == target_fd)
      return true;
  }
  return false;
}

}  // namespace

PlatformChannelPair::PlatformChannelPair() {
  // Create the Unix domain socket and set the ends to nonblocking.
  int fds[2];
  // TODO(vtl): Maybe fail gracefully if |socketpair()| fails.
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);

  server_handle_.fd = fds[0];
  DCHECK(server_handle_.is_valid());
  client_handle_.fd = fds[1];
  DCHECK(client_handle_.is_valid());
}

// static
scoped_ptr<PlatformChannel>
PlatformChannelPair::CreateClientChannelFromParentProcess(
    const CommandLine& command_line) {
  std::string client_fd_string =
      command_line.GetSwitchValueASCII(kMojoChannelDescriptorSwitch);
  int client_fd = -1;
  if (client_fd_string.empty() ||
      !base::StringToInt(client_fd_string, &client_fd) ||
      client_fd < base::GlobalDescriptors::kBaseDescriptor) {
    LOG(ERROR) << "Missing or invalid --" << kMojoChannelDescriptorSwitch;
    return scoped_ptr<PlatformChannel>();
  }

  return PlatformChannel::CreateFromHandle(PlatformChannelHandle(client_fd));
}

void PlatformChannelPair::PrepareToPassClientChannelToChildProcess(
    CommandLine* command_line,
    base::FileHandleMappingVector* file_handle_mapping) const {
  DCHECK(command_line);
  DCHECK(file_handle_mapping);
  // This is an arbitrary sanity check. (Note that this guarantees that the loop
  // below will terminate sanely.)
  CHECK_LT(file_handle_mapping->size(), 1000u);

  DCHECK(client_handle_.is_valid());

  // Find a suitable FD to map our client handle to in the child process.
  // This has quadratic time complexity in the size of |*file_handle_mapping|,
  // but |*file_handle_mapping| should be very small (usually/often empty).
  int target_fd = base::GlobalDescriptors::kBaseDescriptor;
  while (IsTargetDescriptorUsed(*file_handle_mapping, target_fd))
    target_fd++;

  file_handle_mapping->push_back(std::pair<int, int>(client_handle_.fd,
                                                     target_fd));
  // Log a warning if the command line already has the switch, but "clobber" it
  // anyway, since it's reasonably likely that all the switches were just copied
  // from the parent.
  LOG_IF(WARNING, command_line->HasSwitch(kMojoChannelDescriptorSwitch))
      << "Child command line already has switch --"
      << kMojoChannelDescriptorSwitch << "="
      << command_line->GetSwitchValueASCII(kMojoChannelDescriptorSwitch);
  // (Any existing switch won't actually be removed from the command line, but
  // the last one appended takes precedence.)
  command_line->AppendSwitchASCII(kMojoChannelDescriptorSwitch,
                                  base::IntToString(target_fd));
}

void PlatformChannelPair::ChildProcessLaunched() {
  DCHECK(client_handle_.is_valid());
  client_handle_.CloseIfNecessary();
}

}  // namespace system
}  // namespace mojo
