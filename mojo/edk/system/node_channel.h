// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_NODE_CHANNEL_H_
#define MOJO_EDK_SYSTEM_NODE_CHANNEL_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {

// Wraps a Channel to send and receive Node control messages.
class NodeChannel : public base::RefCountedThreadSafe<NodeChannel>,
                    public Channel::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAcceptChild(const ports::NodeName& from_node,
                               const ports::NodeName& parent_name,
                               const ports::NodeName& token) = 0;
    virtual void OnAcceptParent(const ports::NodeName& from_node,
                                const ports::NodeName& token,
                                const ports::NodeName& child_name) = 0;
    virtual void OnAddBrokerClient(const ports::NodeName& from_node,
                                   const ports::NodeName& client_name,
                                   ScopedPlatformHandle process_handle) = 0;
    virtual void OnBrokerClientAdded(const ports::NodeName& from_node,
                                     const ports::NodeName& client_name,
                                     ScopedPlatformHandle broker_channel) = 0;
    virtual void OnAcceptBrokerClient(const ports::NodeName& from_node,
                                      const ports::NodeName& broker_name,
                                      ScopedPlatformHandle broker_channel) = 0;
    virtual void OnPortsMessage(Channel::MessagePtr message) = 0;
    virtual void OnRequestPortMerge(const ports::NodeName& from_node,
                                    const ports::PortName& connector_port_name,
                                    const std::string& token) = 0;
    virtual void OnRequestIntroduction(const ports::NodeName& from_node,
                                       const ports::NodeName& name) = 0;
    virtual void OnIntroduce(const ports::NodeName& from_node,
                             const ports::NodeName& name,
                             ScopedPlatformHandle channel_handle) = 0;
#if defined(OS_WIN)
    virtual void OnRelayPortsMessage(const ports::NodeName& from_node,
                                     base::ProcessHandle from_process,
                                     const ports::NodeName& destination,
                                     Channel::MessagePtr message) = 0;
#endif

    virtual void OnChannelError(const ports::NodeName& node) = 0;
  };

  static scoped_refptr<NodeChannel> Create(
      Delegate* delegate,
      ScopedPlatformHandle platform_handle,
      scoped_refptr<base::TaskRunner> io_task_runner);

  static Channel::MessagePtr CreatePortsMessage(size_t payload_size,
                                                void** payload,
                                                size_t num_handles);

  static void GetPortsMessageData(Channel::Message* message, void** data,
                                  size_t* num_data_bytes);

  // Start receiving messages.
  void Start();

  // Permanently stop the channel from sending or receiving messages.
  void ShutDown();

  void SetRemoteProcessHandle(base::ProcessHandle process_handle);
  bool HasRemoteProcessHandle();
  ScopedPlatformHandle CopyRemoteProcessHandle();

  // Used for context in Delegate calls (via |from_node| arguments.)
  void SetRemoteNodeName(const ports::NodeName& name);

  void AcceptChild(const ports::NodeName& parent_name,
                   const ports::NodeName& token);
  void AcceptParent(const ports::NodeName& token,
                    const ports::NodeName& child_name);
  void AddBrokerClient(const ports::NodeName& client_name,
                       ScopedPlatformHandle process_handle);
  void BrokerClientAdded(const ports::NodeName& client_name,
                         ScopedPlatformHandle broker_channel);
  void AcceptBrokerClient(const ports::NodeName& broker_name,
                          ScopedPlatformHandle broker_channel);
  void PortsMessage(Channel::MessagePtr message);
  void RequestPortMerge(const ports::PortName& connector_port_name,
                        const std::string& token);
  void RequestIntroduction(const ports::NodeName& name);
  void Introduce(const ports::NodeName& name,
                 ScopedPlatformHandle channel_handle);

#if defined(OS_WIN)
  // Relay the message to the specified node via this channel.  This is used to
  // pass windows handles between two processes that do not have permission to
  // duplicate handles into the other's address space. The relay process is
  // assumed to have that permission.
  void RelayPortsMessage(const ports::NodeName& destination,
                         Channel::MessagePtr message);
#endif

 private:
  friend class base::RefCountedThreadSafe<NodeChannel>;

  NodeChannel(Delegate* delegate,
              ScopedPlatformHandle platform_handle,
              scoped_refptr<base::TaskRunner> io_task_runner);
  ~NodeChannel() override;

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override;
  void OnChannelError() override;

  void WriteChannelMessage(Channel::MessagePtr message);

  Delegate* const delegate_;
  const scoped_refptr<base::TaskRunner> io_task_runner_;

  base::Lock channel_lock_;
  scoped_refptr<Channel> channel_;

  // Must only be accessed from |io_task_runner_|'s thread.
  ports::NodeName remote_node_name_;

#if defined(OS_WIN)
  base::Lock remote_process_handle_lock_;
  base::ProcessHandle remote_process_handle_ = base::kNullProcessHandle;
#endif

  DISALLOW_COPY_AND_ASSIGN(NodeChannel);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_NODE_CHANNEL_H_
