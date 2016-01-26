// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <queue>
#include <sstream>

#include "base/logging.h"
#include "mojo/edk/system/ports/node.h"
#include "mojo/edk/system/ports/node_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace ports {
namespace test {

namespace {

void LogMessage(const Message* message) {
  std::stringstream ports;
  for (size_t i = 0; i < message->num_ports(); ++i) {
    if (i > 0)
      ports << ",";
    ports << message->ports()[i];
  }
  DVLOG(1) << "message: \""
           << static_cast<const char*>(message->payload_bytes())
           << "\" ports=[" << ports.str() << "]";
}

void ClosePortsInMessage(Node* node, Message* message) {
  for (size_t i = 0; i < message->num_ports(); ++i) {
    PortRef port;
    ASSERT_EQ(OK, node->GetPort(message->ports()[i], &port));
    EXPECT_EQ(OK, node->ClosePort(port));
  }
}

class TestMessage : public Message {
 public:
  static ScopedMessage NewUserMessage(size_t num_payload_bytes,
                                      size_t num_ports) {
    return ScopedMessage(new TestMessage(num_payload_bytes, num_ports));
  }

  TestMessage(size_t num_payload_bytes, size_t num_ports)
      : Message(num_payload_bytes, num_ports) {
    start_ = new char[num_header_bytes_ + num_ports_bytes_ + num_payload_bytes];
    InitializeUserMessageHeader(start_);
  }

  TestMessage(size_t num_header_bytes,
              size_t num_payload_bytes,
              size_t num_ports_bytes)
      : Message(num_header_bytes,
                num_payload_bytes,
                num_ports_bytes) {
    start_ = new char[num_header_bytes + num_payload_bytes + num_ports_bytes];
  }

  ~TestMessage() override {
    delete[] start_;
  }
};

struct Task {
  Task(NodeName node_name, ScopedMessage message)
      : node_name(node_name),
        message(std::move(message)),
        priority(rand()) {
  }

  NodeName node_name;
  ScopedMessage message;
  int32_t priority;
};

struct TaskComparator {
  bool operator()(const Task* a, const Task* b) {
    return a->priority < b->priority;
  }
};

std::priority_queue<Task*, std::vector<Task*>, TaskComparator> task_queue;
Node* node_map[2];

Node* GetNode(const NodeName& name) {
  return node_map[name.v1];
}

void SetNode(const NodeName& name, Node* node) {
  node_map[name.v1] = node;
}

void PumpTasks() {
  while (!task_queue.empty()) {
    Task* task = task_queue.top();
    task_queue.pop();

    Node* node = GetNode(task->node_name);
    node->AcceptMessage(std::move(task->message));

    delete task;
  }
}

void DiscardPendingTasks() {
  while (!task_queue.empty()) {
    Task* task = task_queue.top();
    task_queue.pop();
    delete task;
  }
}

int SendStringMessage(Node* node, const PortRef& port, const std::string& s) {
  size_t size = s.size() + 1;
  ScopedMessage message = TestMessage::NewUserMessage(size, 0);
  memcpy(message->mutable_payload_bytes(), s.data(), size);
  return node->SendMessage(port, &message);
}

int SendStringMessageWithPort(Node* node,
                              const PortRef& port,
                              const std::string& s,
                              const PortName& sent_port_name) {
  size_t size = s.size() + 1;
  ScopedMessage message = TestMessage::NewUserMessage(size, 1);
  memcpy(message->mutable_payload_bytes(), s.data(), size);
  message->mutable_ports()[0] = sent_port_name;
  return node->SendMessage(port ,&message);
}

int SendStringMessageWithPort(Node* node,
                              const PortRef& port,
                              const std::string& s,
                              const PortRef& sent_port) {
  return SendStringMessageWithPort(node, port, s, sent_port.name());
}

const char* ToString(const ScopedMessage& message) {
  return static_cast<const char*>(message->payload_bytes());
}

class TestNodeDelegate : public NodeDelegate {
 public:
  explicit TestNodeDelegate(const NodeName& node_name)
      : node_name_(node_name),
        drop_messages_(false),
        read_messages_(true),
        save_messages_(false) {
  }

  void set_drop_messages(bool value) { drop_messages_ = value; }
  void set_read_messages(bool value) { read_messages_ = value; }
  void set_save_messages(bool value) { save_messages_ = value; }

  bool GetSavedMessage(ScopedMessage* message) {
    if (saved_messages_.empty()) {
      message->reset();
      return false;
    }
    *message = std::move(saved_messages_.front());
    saved_messages_.pop();
    return true;
  }

  void GenerateRandomPortName(PortName* port_name) override {
    static uint64_t next_port_name = 1;
    port_name->v1 = next_port_name++;
    port_name->v2 = 0;
  }

  void AllocMessage(size_t num_header_bytes, ScopedMessage* message) override {
    message->reset(new TestMessage(num_header_bytes, 0, 0));
  }

  void ForwardMessage(const NodeName& node_name,
                      ScopedMessage message) override {
    if (drop_messages_) {
      DVLOG(1) << "Dropping ForwardMessage from node "
               << node_name_ << " to " << node_name;
      ClosePortsInMessage(GetNode(node_name), message.get());
      return;
    }
    DVLOG(1) << "ForwardMessage from node "
             << node_name_ << " to " << node_name;
    task_queue.push(new Task(node_name, std::move(message)));
  }

  void PortStatusChanged(const PortRef& port) override {
    DVLOG(1) << "PortStatusChanged for " << port.name() << "@" << node_name_;
    if (!read_messages_)
      return;
    Node* node = GetNode(node_name_);
    for (;;) {
      ScopedMessage message;
      int rv = node->GetMessage(port, &message);
      EXPECT_TRUE(rv == OK || rv == ERROR_PORT_PEER_CLOSED);
      if (rv == ERROR_PORT_PEER_CLOSED || !message)
        break;
      if (save_messages_) {
        SaveMessage(std::move(message));
      } else {
        LogMessage(message.get());
        for (size_t i = 0; i < message->num_ports(); ++i) {
          std::stringstream buf;
          buf << "got port: " << message->ports()[i];

          PortRef received_port;
          node->GetPort(message->ports()[i], &received_port);

          SendStringMessage(node, received_port, buf.str());

          // Avoid leaking these ports.
          node->ClosePort(received_port);
        }
      }
    }
  }

 private:
  void SaveMessage(ScopedMessage message) {
    saved_messages_.emplace(std::move(message));
  }

  std::queue<ScopedMessage> saved_messages_;
  NodeName node_name_;
  bool drop_messages_;
  bool read_messages_;
  bool save_messages_;
};

class PortsTest : public testing::Test {
 public:
  PortsTest() {
    SetNode(NodeName(0, 1), nullptr);
    SetNode(NodeName(1, 1), nullptr);
  }

  ~PortsTest() override {
    DiscardPendingTasks();
    SetNode(NodeName(0, 1), nullptr);
    SetNode(NodeName(1, 1), nullptr);
  }
};

}  // namespace

TEST_F(PortsTest, Basic1) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  SetNode(node1_name, &node1);

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  // Transfer a port from node0 to node1.
  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "hello", a1));

  EXPECT_EQ(OK, node0.ClosePort(a0));

  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, Basic2) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  SetNode(node1_name, &node1);

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  PortRef b0, b1;
  EXPECT_EQ(OK, node0.CreatePortPair(&b0, &b1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "hello", b1));
  EXPECT_EQ(OK, SendStringMessage(&node0, b0, "hello again"));

  // This may cause a SendMessage(b1) failure.
  EXPECT_EQ(OK, node0.ClosePort(b0));

  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, Basic3) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  SetNode(node1_name, &node1);

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  // Transfer a port from node0 to node1.
  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "hello", a1));
  EXPECT_EQ(OK, SendStringMessage(&node0, a0, "hello again"));

  // Transfer a0 as well.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "foo", a0));

  PortRef b0, b1;
  EXPECT_EQ(OK, node0.CreatePortPair(&b0, &b1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "bar", b1));
  EXPECT_EQ(OK, SendStringMessage(&node0, b0, "baz"));

  // This may cause a SendMessage(b1) failure.
  EXPECT_EQ(OK, node0.ClosePort(b0));

  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, LostConnectionToNode1) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  SetNode(node1_name, &node1);

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  // Transfer port to node1 and simulate a lost connection to node1. Dropping
  // events from node1 is how we simulate the lost connection.

  node1_delegate.set_drop_messages(true);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "foo", a1));

  PumpTasks();

  EXPECT_EQ(OK, node0.LostConnectionToNode(node1_name));

  PumpTasks();

  EXPECT_EQ(OK, node0.ClosePort(a0));
  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, LostConnectionToNode2) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  node_map[1] = &node1;

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  node1_delegate.set_read_messages(false);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "take a1", a1));

  PumpTasks();

  node1_delegate.set_drop_messages(true);

  EXPECT_EQ(OK, node0.LostConnectionToNode(node1_name));

  PumpTasks();

  ScopedMessage message;
  EXPECT_EQ(ERROR_PORT_PEER_CLOSED, node0.GetMessage(a0, &message));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node0.ClosePort(a0));

  EXPECT_EQ(OK, node0.ClosePort(x0));

  EXPECT_EQ(OK, node1.GetMessage(x1, &message));
  EXPECT_TRUE(message);
  ClosePortsInMessage(&node1, message.get());

  EXPECT_EQ(OK, node1.ClosePort(x1));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, GetMessage1) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));

  ScopedMessage message;
  EXPECT_EQ(OK, node0.GetMessage(a0, &message));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node0.ClosePort(a1));

  EXPECT_EQ(OK, node0.GetMessage(a0, &message));
  EXPECT_FALSE(message);

  PumpTasks();

  EXPECT_EQ(ERROR_PORT_PEER_CLOSED, node0.GetMessage(a0, &message));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node0.ClosePort(a0));

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, GetMessage2) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_read_messages(false);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));

  EXPECT_EQ(OK, SendStringMessage(&node0, a1, "1"));

  ScopedMessage message;
  EXPECT_EQ(OK, node0.GetMessage(a0, &message));

  ASSERT_TRUE(message);
  EXPECT_EQ(0, strcmp("1", ToString(message)));

  EXPECT_EQ(OK, node0.ClosePort(a0));
  EXPECT_EQ(OK, node0.ClosePort(a1));

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, GetMessage3) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_read_messages(false);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));

  const char* kStrings[] = {
    "1",
    "2",
    "3"
  };

  for (size_t i = 0; i < sizeof(kStrings)/sizeof(kStrings[0]); ++i)
    EXPECT_EQ(OK, SendStringMessage(&node0, a1, kStrings[i]));

  ScopedMessage message;
  for (size_t i = 0; i < sizeof(kStrings)/sizeof(kStrings[0]); ++i) {
    EXPECT_EQ(OK, node0.GetMessage(a0, &message));
    ASSERT_TRUE(message);
    EXPECT_EQ(0, strcmp(kStrings[i], ToString(message)));
    DVLOG(1) << "got " << kStrings[i];
  }

  EXPECT_EQ(OK, node0.ClosePort(a0));
  EXPECT_EQ(OK, node0.ClosePort(a1));

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, Delegation1) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  node_map[1] = &node1;

  node0_delegate.set_save_messages(true);
  node1_delegate.set_save_messages(true);

  // Setup pipe between node0 and node1.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));
  EXPECT_EQ(OK, node0.InitializePort(x0, node1_name, x1.name()));
  EXPECT_EQ(OK, node1.InitializePort(x1, node0_name, x0.name()));

  // In this test, we send a message to a port that has been moved.

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.CreatePortPair(&a0, &a1));

  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, x0, "a1", a1));

  PumpTasks();

  ScopedMessage message;
  ASSERT_TRUE(node1_delegate.GetSavedMessage(&message));

  ASSERT_EQ(1u, message->num_ports());

  // This is "a1" from the point of view of node1.
  PortName a2_name = message->ports()[0];

  EXPECT_EQ(OK, SendStringMessageWithPort(&node1, x1, "a2", a2_name));

  PumpTasks();

  EXPECT_EQ(OK, SendStringMessage(&node0, a0, "hello"));

  PumpTasks();

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));

  ASSERT_EQ(1u, message->num_ports());

  // This is "a2" from the point of view of node1.
  PortName a3_name = message->ports()[0];

  PortRef a3;
  EXPECT_EQ(OK, node0.GetPort(a3_name, &a3));

  EXPECT_EQ(0, strcmp("a2", ToString(message)));

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));

  EXPECT_EQ(0u, message->num_ports());
  EXPECT_EQ(0, strcmp("hello", ToString(message)));

  EXPECT_EQ(OK, node0.ClosePort(a0));
  EXPECT_EQ(OK, node0.ClosePort(a3));

  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, Delegation2) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  SetNode(node0_name, &node0);

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  node_map[1] = &node1;

  node0_delegate.set_save_messages(true);
  node1_delegate.set_save_messages(true);

  for (int i = 0; i < 10; ++i) {
    // Setup pipe a<->b between node0 and node1.
    PortRef A, B;
    EXPECT_EQ(OK, node0.CreateUninitializedPort(&A));
    EXPECT_EQ(OK, node1.CreateUninitializedPort(&B));
    EXPECT_EQ(OK, node0.InitializePort(A, node1_name, B.name()));
    EXPECT_EQ(OK, node1.InitializePort(B, node0_name, A.name()));

    PortRef C, D;
    EXPECT_EQ(OK, node0.CreatePortPair(&C, &D));

    PortRef E, F;
    EXPECT_EQ(OK, node0.CreatePortPair(&E, &F));

    // Pass D over A to B.
    EXPECT_EQ(OK, SendStringMessageWithPort(&node0, A, "1", D));

    // Pass F over C to D.
    EXPECT_EQ(OK, SendStringMessageWithPort(&node0, C, "1", F));

    // This message should find its way to node1.
    EXPECT_EQ(OK, SendStringMessage(&node0, E, "hello"));

    PumpTasks();

    EXPECT_EQ(OK, node0.ClosePort(C));
    EXPECT_EQ(OK, node0.ClosePort(E));

    EXPECT_EQ(OK, node0.ClosePort(A));
    EXPECT_EQ(OK, node1.ClosePort(B));

    for (;;) {
      ScopedMessage message;
      if (node1_delegate.GetSavedMessage(&message)) {
        ClosePortsInMessage(&node1, message.get());
        if (strcmp("hello", ToString(message)) == 0)
          break;
      } else {
        ASSERT_TRUE(false);  // "hello" message not delivered!
        break;
      }
    }

    PumpTasks();  // Because ClosePort may have generated tasks.
  }

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, SendUninitialized) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  NodeName node1_name(1, 1);
  TestNodeDelegate node1_delegate(node1_name);
  Node node1(node1_name, &node1_delegate);
  node_map[1] = &node1;

  // Begin to setup a pipe between node0 and node1, but don't initialize either
  // endpoint.
  PortRef x0, x1;
  EXPECT_EQ(OK, node0.CreateUninitializedPort(&x0));
  EXPECT_EQ(OK, node1.CreateUninitializedPort(&x1));

  node0_delegate.set_save_messages(true);
  node1_delegate.set_save_messages(true);

  // Send a message on each port and expect neither to arrive yet.

  EXPECT_EQ(ERROR_PORT_STATE_UNEXPECTED,
            SendStringMessage(&node0, x0, "oops"));
  EXPECT_EQ(ERROR_PORT_STATE_UNEXPECTED,
            SendStringMessage(&node1, x1, "oh well"));

  EXPECT_EQ(OK, node0.ClosePort(x0));
  EXPECT_EQ(OK, node1.ClosePort(x1));

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
  EXPECT_TRUE(node1.CanShutdownCleanly(false));
}

TEST_F(PortsTest, SendFailure) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_save_messages(true);

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  // Try to send A over itself.

  EXPECT_EQ(ERROR_PORT_CANNOT_SEND_SELF,
            SendStringMessageWithPort(&node0, A, "oops", A));

  // Try to send B over A.

  EXPECT_EQ(ERROR_PORT_CANNOT_SEND_PEER,
            SendStringMessageWithPort(&node0, A, "nope", B));

  PumpTasks();

  // There should have been no messages accepted.
  ScopedMessage message;
  EXPECT_FALSE(node0_delegate.GetSavedMessage(&message));

  // Both A and B should still work.

  EXPECT_EQ(OK, SendStringMessage(&node0, A, "hi"));
  EXPECT_EQ(OK, SendStringMessage(&node0, B, "hey"));

  PumpTasks();

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  EXPECT_EQ(0, strcmp("hi", ToString(message)));
  ClosePortsInMessage(&node0, message.get());

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  EXPECT_EQ(0, strcmp("hey", ToString(message)));
  ClosePortsInMessage(&node0, message.get());

  PumpTasks();

  EXPECT_EQ(OK, node0.ClosePort(A));
  EXPECT_EQ(OK, node0.ClosePort(B));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, DontLeakUnreceivedPorts) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_read_messages(false);

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  PortRef C, D;
  EXPECT_EQ(OK, node0.CreatePortPair(&C, &D));

  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, A, "foo", D));

  PumpTasks();

  EXPECT_EQ(OK, node0.ClosePort(C));

  EXPECT_EQ(OK, node0.ClosePort(A));
  EXPECT_EQ(OK, node0.ClosePort(B));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, AllowShutdownWithLocalPortsOpen) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_save_messages(true);

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  PortRef C, D;
  EXPECT_EQ(OK, node0.CreatePortPair(&C, &D));

  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, A, "foo", D));

  ScopedMessage message;
  EXPECT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());

  PortRef E;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &E));

  EXPECT_TRUE(node0.CanShutdownCleanly(true));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(true));
  EXPECT_FALSE(node0.CanShutdownCleanly(false));

  EXPECT_EQ(OK, node0.ClosePort(A));
  EXPECT_EQ(OK, node0.ClosePort(B));
  EXPECT_EQ(OK, node0.ClosePort(C));
  EXPECT_EQ(OK, node0.ClosePort(E));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, ProxyCollapse1) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_save_messages(true);

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  PortRef X, Y;
  EXPECT_EQ(OK, node0.CreatePortPair(&X, &Y));

  ScopedMessage message;

  // Send B and receive it as C.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", B));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &C));

  // Send C and receive it as D.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", C));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef D;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &D));

  // Send D and receive it as E.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", D));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &E));

  EXPECT_EQ(OK, node0.ClosePort(X));
  EXPECT_EQ(OK, node0.ClosePort(Y));

  EXPECT_EQ(OK, node0.ClosePort(A));
  EXPECT_EQ(OK, node0.ClosePort(E));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, ProxyCollapse2) {
  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_save_messages(true);

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  PortRef X, Y;
  EXPECT_EQ(OK, node0.CreatePortPair(&X, &Y));

  ScopedMessage message;

  // Send B and receive it as C.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", B));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &C));

  // Send A and receive it as D.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", A));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef D;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &D));

  // At this point we have a scenario with:
  //
  // D -> [B] -> C -> [A]
  //
  // Ensure that the proxies can collapse.

  EXPECT_EQ(OK, node0.ClosePort(X));
  EXPECT_EQ(OK, node0.ClosePort(Y));

  EXPECT_EQ(OK, node0.ClosePort(C));
  EXPECT_EQ(OK, node0.ClosePort(D));

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, SendWithClosedPeer) {
  // This tests that if a port is sent when its peer is already known to be
  // closed, the newly created port will be aware of that peer closure, and the
  // proxy will eventually collapse.

  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_read_messages(false);

  // Send a message from A to B, then close A.
  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));
  EXPECT_EQ(OK, SendStringMessage(&node0, A, "hey"));
  EXPECT_EQ(OK, node0.ClosePort(A));

  PumpTasks();

  // Now send B over X-Y as new port C.
  PortRef X, Y;
  EXPECT_EQ(OK, node0.CreatePortPair(&X, &Y));

  node0_delegate.set_read_messages(true);
  node0_delegate.set_save_messages(true);
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", B));

  EXPECT_EQ(OK, node0.ClosePort(X));
  EXPECT_EQ(OK, node0.ClosePort(Y));

  ScopedMessage message;
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());

  PortRef C;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &C));

  PumpTasks();

  // C should receive the message originally sent to B, and it should also be
  // aware of A's closure.

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  EXPECT_EQ(0, strcmp("hey", ToString(message)));

  PortStatus status;
  EXPECT_EQ(OK, node0.GetStatus(C, &status));
  EXPECT_FALSE(status.receiving_messages);
  EXPECT_FALSE(status.has_messages);
  EXPECT_TRUE(status.peer_closed);

  node0.ClosePort(C);

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

TEST_F(PortsTest, SendWithClosedPeerSent) {
  // This tests that if a port is closed while some number of proxies are still
  // routing messages (directly or indirectly) to it, that the peer port is
  // eventually notified of the closure, and the dead-end proxies will
  // eventually be removed.

  NodeName node0_name(0, 1);
  TestNodeDelegate node0_delegate(node0_name);
  Node node0(node0_name, &node0_delegate);
  node_map[0] = &node0;

  node0_delegate.set_save_messages(true);

  PortRef X, Y;
  EXPECT_EQ(OK, node0.CreatePortPair(&X, &Y));

  PortRef A, B;
  EXPECT_EQ(OK, node0.CreatePortPair(&A, &B));

  ScopedMessage message;

  // Send A as new port C.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", A));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &C));

  // Send C as new port D.
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", C));
  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef D;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &D));

  node0_delegate.set_read_messages(false);

  // Send a message to B through D, then close D.
  EXPECT_EQ(OK, SendStringMessage(&node0, D, "hey"));
  EXPECT_EQ(OK, node0.ClosePort(D));

  PumpTasks();

  // Now send B as new port E.

  node0_delegate.set_read_messages(true);
  EXPECT_EQ(OK, SendStringMessageWithPort(&node0, X, "foo", B));

  EXPECT_EQ(OK, node0.ClosePort(X));
  EXPECT_EQ(OK, node0.ClosePort(Y));

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  ASSERT_EQ(1u, message->num_ports());

  PortRef E;
  ASSERT_EQ(OK, node0.GetPort(message->ports()[0], &E));

  PumpTasks();

  // E should receive the message originally sent to B, and it should also be
  // aware of D's closure.

  ASSERT_TRUE(node0_delegate.GetSavedMessage(&message));
  EXPECT_EQ(0, strcmp("hey", ToString(message)));

  PortStatus status;
  EXPECT_EQ(OK, node0.GetStatus(E, &status));
  EXPECT_FALSE(status.receiving_messages);
  EXPECT_FALSE(status.has_messages);
  EXPECT_TRUE(status.peer_closed);

  node0.ClosePort(E);

  PumpTasks();

  EXPECT_TRUE(node0.CanShutdownCleanly(false));
}

}  // namespace test
}  // namespace ports
}  // namespace edk
}  // namespace mojo
