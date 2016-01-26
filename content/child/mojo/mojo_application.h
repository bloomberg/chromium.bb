// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
#define CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/mojo/channel_init.h"
#include "content/common/mojo/service_registry_impl.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class SequencedTaskRunner;
}

namespace IPC {
class Message;
}

namespace content {

// MojoApplication represents the code needed to setup a child process as a
// Mojo application via Chrome IPC. Instantiate MojoApplication and call its
// OnMessageReceived method to give it a shot at handling Chrome IPC messages.
// It makes the ServiceRegistry interface available.
class MojoApplication {
 public:
  explicit MojoApplication(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  virtual ~MojoApplication();

  bool OnMessageReceived(const IPC::Message& msg);

  ServiceRegistry* service_registry() { return &service_registry_; }

 private:
  void OnActivate(const IPC::PlatformFileForTransit& file);

  void OnMessagePipeCreated(mojo::ScopedMessagePipeHandle pipe);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  ChannelInit channel_init_;
  ServiceRegistryImpl service_registry_;
  base::WeakPtrFactory<MojoApplication> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoApplication);
};

}  // namespace content

#endif  // CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
