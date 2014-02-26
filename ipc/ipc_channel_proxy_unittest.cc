// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_test_base.h"

namespace {

#if defined(IPC_MESSAGE_START)
#undef IPC_MESSAGE_START
#endif

enum Command {
  SEND,
  QUIT
};

static void Send(IPC::Sender* sender,
                 int message_class,
                 Command command) {
  const int IPC_MESSAGE_START = message_class;
  IPC::Message* message = new IPC::Message(0,
                                           IPC_MESSAGE_ID(),
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(command);
  sender->Send(message);
}

class QuitListener : public IPC::Listener {
 public:
  QuitListener() {}
  virtual ~QuitListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);

    int command = SEND;
    EXPECT_TRUE(iter.ReadInt(&command));
    if (command == QUIT)
      base::MessageLoop::current()->QuitWhenIdle();

    return true;
  }
};

class ChannelReflectorListener : public IPC::Listener {
 public:
  ChannelReflectorListener() : channel_(NULL) {}
  virtual ~ChannelReflectorListener() {}

  void Init(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    CHECK(channel_);

    PickleIterator iter(message);

    int command = SEND;
    EXPECT_TRUE(iter.ReadInt(&command));
    if (command == QUIT) {
      channel_->Send(new IPC::Message(message));
      base::MessageLoop::current()->QuitWhenIdle();
      return true;
    }

    channel_->Send(new IPC::Message(message));
    return true;
  }

 private:
  IPC::Channel* channel_;
};

class MessageCountFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  MessageCountFilter()
      : messages_received_(0),
        supported_message_class_(0),
        is_global_filter_(true),
        filter_removed_(false),
        message_filtering_enabled_(false) {}

  MessageCountFilter(uint32 supported_message_class)
      : messages_received_(0),
        supported_message_class_(supported_message_class),
        is_global_filter_(false),
        filter_removed_(false),
        message_filtering_enabled_(false) {}

  virtual void OnFilterRemoved() OVERRIDE {
    EXPECT_FALSE(filter_removed_);
    filter_removed_ = true;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    if (!is_global_filter_) {
      EXPECT_EQ(supported_message_class_, IPC_MESSAGE_CLASS(message));
    }
    ++messages_received_;
    return message_filtering_enabled_;
  }

  virtual bool GetSupportedMessageClasses(
      std::vector<uint32>* supported_message_classes) const OVERRIDE {
    if (is_global_filter_)
      return false;
    supported_message_classes->push_back(supported_message_class_);
    return true;
  }

  void set_message_filtering_enabled(bool enabled) {
    message_filtering_enabled_ = enabled;
  }

  size_t messages_received() const { return messages_received_; }
  bool filter_removed() const { return filter_removed_; }

 private:
  virtual ~MessageCountFilter() {}

  size_t messages_received_;
  uint32 supported_message_class_;
  bool is_global_filter_;
  bool filter_removed_;
  bool message_filtering_enabled_;
};

class IPCChannelProxyTest : public IPCTestBase {
 public:
  IPCChannelProxyTest() {}
  virtual ~IPCChannelProxyTest() {}

  virtual void SetUp() OVERRIDE {
    IPCTestBase::SetUp();

    Init("ChannelProxyClient");

    thread_.reset(new base::Thread("ChannelProxyTestServerThread"));
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    thread_->StartWithOptions(options);

    listener_.reset(new QuitListener());
    CreateChannelProxy(listener_.get(), thread_->message_loop_proxy().get());

    ASSERT_TRUE(StartClient());
  }

  virtual void TearDown() {
    DestroyChannelProxy();
    thread_.reset();
    listener_.reset();
    IPCTestBase::TearDown();
  }

  void SendQuitMessageAndWaitForIdle() {
    Send(sender(), -1, QUIT);
    base::MessageLoop::current()->Run();
    EXPECT_TRUE(WaitForClientShutdown());
  }

 private:
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<QuitListener> listener_;
};

TEST_F(IPCChannelProxyTest, MessageClassFilters) {
  // Construct a filter per message class.
  std::vector<scoped_refptr<MessageCountFilter> > class_filters;
  for (uint32 i = 0; i < LastIPCMsgStart; ++i) {
    class_filters.push_back(make_scoped_refptr(
        new MessageCountFilter(i)));
    channel_proxy()->AddFilter(class_filters.back().get());
  }

  // Send a message for each class; each filter should receive just one message.
  for (uint32 i = 0; i < LastIPCMsgStart; ++i)
    Send(sender(), i, SEND);

  // Send some messages not assigned to a specific or valid message class.
  Send(sender(), -1, SEND);
  Send(sender(), LastIPCMsgStart, SEND);
  Send(sender(), LastIPCMsgStart + 1, SEND);

  // Each filter should have received just the one sent message of the
  // corresponding class.
  SendQuitMessageAndWaitForIdle();
  for (size_t i = 0; i < class_filters.size(); ++i)
    EXPECT_EQ(1U, class_filters[i]->messages_received());
}

TEST_F(IPCChannelProxyTest, GlobalAndMessageClassFilters) {
  // Add a class and global filter.
  const int kMessageClass = 7;
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(kMessageClass));
  class_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(class_filter.get());

  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());
  global_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(global_filter.get());

  // A message  of class |kMessageClass| should be seen by both the global
  // filter and |kMessageClass|-specific filter.
  Send(sender(), kMessageClass, SEND);

  // A message of a different class should be seen only by the global filter.
  Send(sender(), kMessageClass + 1, SEND);

  // Flush all messages.
  SendQuitMessageAndWaitForIdle();

  // The class filter should have received only the class-specific message.
  EXPECT_EQ(1U, class_filter->messages_received());

  // The global filter should have received both SEND messages, as well as the
  // final QUIT message.
  EXPECT_EQ(3U, global_filter->messages_received());
}

TEST_F(IPCChannelProxyTest, FilterRemoval) {
  // Add a class and global filter.
  const int kMessageClass = 7;
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(kMessageClass));
  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());

  // Add and remove both types of filters.
  channel_proxy()->AddFilter(class_filter.get());
  channel_proxy()->AddFilter(global_filter.get());
  channel_proxy()->RemoveFilter(global_filter.get());
  channel_proxy()->RemoveFilter(class_filter.get());

  // Send some messages; they should not be seen by either filter.
  Send(sender(), 0, SEND);
  Send(sender(), kMessageClass, SEND);

  // Ensure that the filters were removed and did not receive any messages.
  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(global_filter->filter_removed());
  EXPECT_TRUE(class_filter->filter_removed());
  EXPECT_EQ(0U, class_filter->messages_received());
  EXPECT_EQ(0U, global_filter->messages_received());
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ChannelProxyClient) {
  base::MessageLoopForIO main_message_loop;
  ChannelReflectorListener listener;
  IPC::Channel channel(IPCTestBase::GetChannelName("ChannelProxyClient"),
                       IPC::Channel::MODE_CLIENT,
                       &listener);
  CHECK(channel.Connect());
  listener.Init(&channel);

  base::MessageLoop::current()->Run();
  return 0;
}

}  // namespace
