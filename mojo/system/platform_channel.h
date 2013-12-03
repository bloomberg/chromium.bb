// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_CHANNEL_H_
#define MOJO_SYSTEM_PLATFORM_CHANNEL_H_

#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "mojo/public/system/system_export.h"
#include "mojo/system/platform_channel_handle.h"

class CommandLine;

namespace mojo {
namespace system {

class MOJO_SYSTEM_EXPORT PlatformChannel {
 public:
  virtual ~PlatformChannel();

  // Returns the channel's handle, passing ownership.
  PlatformChannelHandle PassHandle();

  bool is_valid() const { return handle_.is_valid(); }

 protected:
  PlatformChannel();

  PlatformChannelHandle* mutable_handle() { return &handle_; }

 private:
  PlatformChannelHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannel);
};

class PlatformClientChannel;

// A server channel has an "implicit" client channel created with it. This may
// be a real channel (in the case of POSIX, in which case there's an actual FD
// for it) or fake.
//  - That client channel may then be used in-process (e.g., for single process
//    tests) by getting a |PlatformClientChannel| using |CreateClientChannel()|.
//  - Or it may be "passed" to a new child process using
//    |GetDataNeededToPassClientChannelToChildProcess()|, etc. (see below). The
//    child process would then get a |PlatformClientChannel| by using
//    |PlatformClientChannel::CreateFromParentProcess()|.
//  - In both these cases, "ownership" of the client channel is transferred (to
//    the |PlatformClientChannel| or the child process).
// TODO(vtl): Add ways of passing it to other existing processes.
class MOJO_SYSTEM_EXPORT PlatformServerChannel : public PlatformChannel {
 public:
  virtual ~PlatformServerChannel() {}

  static scoped_ptr<PlatformServerChannel> Create(const std::string& name);

  // For in-process use, from a server channel you can make a corresponding
  // client channel.
  virtual scoped_ptr<PlatformClientChannel> CreateClientChannel() = 0;

  // Prepares to pass the client channel to a new child process, to be launched
  // using |LaunchProcess()| (from base/launch.h). Modifies |*command_line| and
  // |*file_handle_mapping| as needed. (|file_handle_mapping| may be null on
  // platforms that don't need it, like Windows.)
  virtual void GetDataNeededToPassClientChannelToChildProcess(
      CommandLine* command_line,
      base::FileHandleMappingVector* file_handle_mapping) const = 0;
  // To be called once the child process has been successfully launched, to do
  // any cleanup necessary.
  virtual void ChildProcessLaunched() = 0;

  const std::string& name() const { return name_; }

 protected:
  explicit PlatformServerChannel(const std::string& name);

 private:
  const std::string name_;

  DISALLOW_COPY_AND_ASSIGN(PlatformServerChannel);
};

class MOJO_SYSTEM_EXPORT PlatformClientChannel : public PlatformChannel {
 public:
  virtual ~PlatformClientChannel() {}

  // Creates a client channel if you already have the underlying handle for it.
  // Note: This takes ownership of |handle|.
  static scoped_ptr<PlatformClientChannel> CreateFromHandle(
      const PlatformChannelHandle& handle);

  // To be called to get a client channel passed from the parent process, using
  // |PlatformServerChannel::GetDataNeededToPassClientChannelToChildProcess()|,
  // etc. Returns null on failure.
  static scoped_ptr<PlatformClientChannel> CreateFromParentProcess(
      const CommandLine& command_line);

 private:
  PlatformClientChannel() {}

  DISALLOW_COPY_AND_ASSIGN(PlatformClientChannel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_CHANNEL_H_
