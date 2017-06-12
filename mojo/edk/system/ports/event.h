// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_EVENT_H_
#define MOJO_EDK_SYSTEM_PORTS_EVENT_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/ports/user_message.h"

namespace mojo {
namespace edk {
namespace ports {

class Event;

using ScopedEvent = std::unique_ptr<Event>;

class Event {
 public:
  enum Type : uint32_t {
    kUserMessage,
    kPortAccepted,
    kObserveProxy,
    kObserveProxyAck,
    kObserveClosure,
    kMergePort,
  };

#pragma pack(push, 1)
  struct PortDescriptor {
    PortDescriptor();

    NodeName peer_node_name;
    PortName peer_port_name;
    NodeName referring_node_name;
    PortName referring_port_name;
    uint64_t next_sequence_num_to_send;
    uint64_t next_sequence_num_to_receive;
    uint64_t last_sequence_num_to_receive;
    bool peer_closed;
    char padding[7];
  };
#pragma pack(pop)
  virtual ~Event();

  static ScopedEvent Deserialize(const void* buffer, size_t num_bytes);

  template <typename T>
  static std::unique_ptr<T> Cast(ScopedEvent* event) {
    return base::WrapUnique(static_cast<T*>(event->release()));
  }

  Type type() const { return type_; }
  const PortName& port_name() const { return port_name_; }
  void set_port_name(const PortName& port_name) { port_name_ = port_name; }

  size_t GetSerializedSize() const;
  void Serialize(void* buffer) const;
  virtual ScopedEvent Clone() const;

 protected:
  Event(Type type, const PortName& port_name);

  virtual size_t GetSerializedDataSize() const = 0;
  virtual void SerializeData(void* buffer) const = 0;

 private:
  const Type type_;
  PortName port_name_;

  DISALLOW_COPY_AND_ASSIGN(Event);
};

class UserMessageEvent : public Event {
 public:
  explicit UserMessageEvent(size_t num_ports);
  ~UserMessageEvent() override;

  bool HasMessage() const { return !!message_; }
  void AttachMessage(std::unique_ptr<UserMessage> message);

  template <typename T>
  T* GetMessage() {
    DCHECK(HasMessage());
    DCHECK_EQ(&T::kUserMessageTypeInfo, message_->type_info());
    return static_cast<T*>(message_.get());
  }

  template <typename T>
  const T* GetMessage() const {
    DCHECK(HasMessage());
    DCHECK_EQ(&T::kUserMessageTypeInfo, message_->type_info());
    return static_cast<const T*>(message_.get());
  }

  void ReservePorts(size_t num_ports);
  bool NotifyWillBeRoutedExternally();

  uint32_t sequence_num() const { return sequence_num_; }
  void set_sequence_num(uint32_t sequence_num) { sequence_num_ = sequence_num; }

  size_t num_ports() const { return ports_.size(); }
  PortDescriptor* port_descriptors() { return port_descriptors_.data(); }
  PortName* ports() { return ports_.data(); }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  UserMessageEvent(const PortName& port_name, uint64_t sequence_num);

  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  uint64_t sequence_num_ = 0;
  std::vector<PortDescriptor> port_descriptors_;
  std::vector<PortName> ports_;
  std::unique_ptr<UserMessage> message_;

  DISALLOW_COPY_AND_ASSIGN(UserMessageEvent);
};

class PortAcceptedEvent : public Event {
 public:
  explicit PortAcceptedEvent(const PortName& port_name);
  ~PortAcceptedEvent() override;

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  DISALLOW_COPY_AND_ASSIGN(PortAcceptedEvent);
};

class ObserveProxyEvent : public Event {
 public:
  ObserveProxyEvent(const PortName& port_name,
                    const NodeName& proxy_node_name,
                    const PortName& proxy_port_name,
                    const NodeName& proxy_target_node_name,
                    const PortName& proxy_target_port_name);
  ~ObserveProxyEvent() override;

  const NodeName& proxy_node_name() const { return proxy_node_name_; }
  const PortName& proxy_port_name() const { return proxy_port_name_; }
  const NodeName& proxy_target_node_name() const {
    return proxy_target_node_name_;
  }
  const PortName& proxy_target_port_name() const {
    return proxy_target_port_name_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;
  ScopedEvent Clone() const override;

  const NodeName proxy_node_name_;
  const PortName proxy_port_name_;
  const NodeName proxy_target_node_name_;
  const PortName proxy_target_port_name_;

  DISALLOW_COPY_AND_ASSIGN(ObserveProxyEvent);
};

class ObserveProxyAckEvent : public Event {
 public:
  ObserveProxyAckEvent(const PortName& port_name, uint64_t last_sequence_num);
  ~ObserveProxyAckEvent() override;

  uint64_t last_sequence_num() const { return last_sequence_num_; }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;
  ScopedEvent Clone() const override;

  const uint64_t last_sequence_num_;

  DISALLOW_COPY_AND_ASSIGN(ObserveProxyAckEvent);
};

class ObserveClosureEvent : public Event {
 public:
  ObserveClosureEvent(const PortName& port_name, uint64_t last_sequence_num);
  ~ObserveClosureEvent() override;

  uint64_t last_sequence_num() const { return last_sequence_num_; }
  void set_last_sequence_num(uint64_t last_sequence_num) {
    last_sequence_num_ = last_sequence_num;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;
  ScopedEvent Clone() const override;

  uint64_t last_sequence_num_;

  DISALLOW_COPY_AND_ASSIGN(ObserveClosureEvent);
};

class MergePortEvent : public Event {
 public:
  MergePortEvent(const PortName& port_name,
                 const PortName& new_port_name,
                 const PortDescriptor& new_port_descriptor);
  ~MergePortEvent() override;

  const PortName& new_port_name() const { return new_port_name_; }
  const PortDescriptor& new_port_descriptor() const {
    return new_port_descriptor_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  const PortName new_port_name_;
  const PortDescriptor new_port_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(MergePortEvent);
};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_EVENT_H_
