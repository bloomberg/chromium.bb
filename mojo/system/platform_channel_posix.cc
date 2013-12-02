// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace mojo {
namespace system {

namespace {

void CloseIfNecessary(PlatformChannelHandle* handle) {
  if (!handle->is_valid())
    return;

  PCHECK(close(handle->fd) == 0);
  *handle = PlatformChannelHandle();
}

class PlatformServerChannelPosix : public PlatformServerChannel {
 public:
  PlatformServerChannelPosix(const std::string& name);
  virtual ~PlatformServerChannelPosix();

  // |PlatformServerChannel| implementation:
  virtual scoped_ptr<PlatformClientChannel> CreateClientChannel() OVERRIDE;
  virtual void PrepareToPassClientChannelToChildProcess(
      CommandLine* command_line,
      base::FileHandleMappingVector* file_handle_mapping) OVERRIDE;
  virtual void ChildProcessLaunched() OVERRIDE;

 private:
  PlatformChannelHandle client_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformServerChannelPosix);
};

PlatformServerChannelPosix::PlatformServerChannelPosix(
    const std::string& name)
    : PlatformServerChannel(name) {
  // Create the Unix domain socket and set the ends to nonblocking.
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);

  mutable_handle()->fd = fds[0];
  DCHECK(is_valid());
  client_handle_.fd = fds[1];
  DCHECK(client_handle_.is_valid());
}

PlatformServerChannelPosix::~PlatformServerChannelPosix() {
  if (is_valid())
    CloseIfNecessary(mutable_handle());
  if (client_handle_.is_valid())
    CloseIfNecessary(&client_handle_);
}

scoped_ptr<PlatformClientChannel>
    PlatformServerChannelPosix::CreateClientChannel() {
  if (!client_handle_.is_valid()) {
    NOTREACHED();
    return scoped_ptr<PlatformClientChannel>();
  }

  scoped_ptr<PlatformClientChannel> rv =
      PlatformClientChannel::CreateFromHandle(client_handle_);
  DCHECK(rv->is_valid());
  client_handle_ = PlatformChannelHandle();
  return rv.Pass();
}

void PlatformServerChannelPosix::PrepareToPassClientChannelToChildProcess(
    CommandLine* command_line,
    base::FileHandleMappingVector* file_handle_mapping) {
  // TODO(vtl)
  NOTIMPLEMENTED();
}

void PlatformServerChannelPosix::ChildProcessLaunched() {
  // TODO(vtl)
  NOTIMPLEMENTED();
}

}  // namespace

// -----------------------------------------------------------------------------

// Static factory method declared in platform_channel.h.
// static
scoped_ptr<PlatformServerChannel> PlatformServerChannel::Create(
    const std::string& name) {
  return scoped_ptr<PlatformServerChannel>(
      new PlatformServerChannelPosix(name));
}

// -----------------------------------------------------------------------------

// Static factory method declared in platform_channel.h.
// static
scoped_ptr<PlatformClientChannel>
    PlatformClientChannel::CreateFromParentProcess(
        const CommandLine& command_line) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return scoped_ptr<PlatformClientChannel>();
}

}  // namespace system
}  // namespace mojo
