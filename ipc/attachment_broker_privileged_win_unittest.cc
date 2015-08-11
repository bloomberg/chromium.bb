// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/attachment_broker_privileged_win.h"
#include "ipc/attachment_broker_unprivileged_win.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/handle_win.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"

namespace {

const char kDataBuffer[] = "This is some test data to write to the file.";

// Returns the contents of the file represented by |h| as a std::string.
std::string ReadFromFile(HANDLE h) {
  SetFilePointer(h, 0, nullptr, FILE_BEGIN);
  char buffer[100];
  DWORD bytes_read;
  BOOL success = ::ReadFile(h, buffer, static_cast<DWORD>(strlen(kDataBuffer)),
                            &bytes_read, nullptr);
  return success ? std::string(buffer, bytes_read) : std::string();
}

HANDLE GetHandleFromBrokeredAttachment(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment) {
  if (attachment->GetType() !=
      IPC::BrokerableAttachment::TYPE_BROKERABLE_ATTACHMENT)
    return nullptr;
  if (attachment->GetBrokerableType() != IPC::BrokerableAttachment::WIN_HANDLE)
    return nullptr;
  IPC::internal::HandleAttachmentWin* received_handle_attachment =
      static_cast<IPC::internal::HandleAttachmentWin*>(attachment.get());
  return received_handle_attachment->get_handle();
}

// Returns true if |attachment| is a file HANDLE whose contents is
// |kDataBuffer|.
bool CheckContentsOfBrokeredAttachment(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment) {
  HANDLE h = GetHandleFromBrokeredAttachment(attachment);
  if (h == nullptr)
    return false;

  std::string contents = ReadFromFile(h);
  return contents == std::string(kDataBuffer);
}

enum TestResult {
  RESULT_UNKNOWN,
  RESULT_SUCCESS,
  RESULT_FAILURE,
};

// Once the test is finished, send a control message to the parent process with
// the result. The message may require the runloop to be run before its
// dispatched.
void SendControlMessage(IPC::Sender* sender, bool success) {
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  TestResult result = success ? RESULT_SUCCESS : RESULT_FAILURE;
  message->WriteInt(result);
  sender->Send(message);
}

class MockObserver : public IPC::AttachmentBroker::Observer {
 public:
  void ReceivedBrokerableAttachmentWithId(
      const IPC::BrokerableAttachment::AttachmentId& id) override {
    id_ = id;
  }
  IPC::BrokerableAttachment::AttachmentId* get_id() { return &id_; }

 private:
  IPC::BrokerableAttachment::AttachmentId id_;
};

// Forwards all messages to |listener_|.  Quits the message loop after a
// message is received, or the channel has an error.
class ProxyListener : public IPC::Listener {
 public:
  ProxyListener() : reason_(MESSAGE_RECEIVED) {}
  ~ProxyListener() override {}

  // The reason for exiting the message loop.
  enum Reason { MESSAGE_RECEIVED, CHANNEL_ERROR };

  bool OnMessageReceived(const IPC::Message& message) override {
    bool result = listener_->OnMessageReceived(message);
    reason_ = MESSAGE_RECEIVED;
    base::MessageLoop::current()->Quit();
    return result;
  }

  void OnChannelError() override {
    reason_ = CHANNEL_ERROR;
    base::MessageLoop::current()->Quit();
  }

  void set_listener(IPC::Listener* listener) { listener_ = listener; }
  Reason get_reason() { return reason_; }

 private:
  IPC::Listener* listener_;
  Reason reason_;
};

// Waits for a result to be sent over the channel.  Quits the message loop
// after a message is received, or the channel has an error.
class ResultListener : public IPC::Listener {
 public:
  ResultListener() : result_(RESULT_UNKNOWN) {}
  ~ResultListener() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);

    int result;
    EXPECT_TRUE(iter.ReadInt(&result));
    result_ = static_cast<TestResult>(result);
    return true;
  }

  TestResult get_result() { return result_; }

 private:
  TestResult result_;
};

// The parent process acts as an unprivileged process. The forked process acts
// as the privileged process.
class IPCAttachmentBrokerPrivilegedWinTest : public IPCTestBase {
 public:
  IPCAttachmentBrokerPrivilegedWinTest() : message_index_(0) {}
  ~IPCAttachmentBrokerPrivilegedWinTest() override {}

  void SetUp() override {
    IPCTestBase::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &temp_path_));
  }

  void TearDown() override { IPCTestBase::TearDown(); }

  // Takes ownership of |broker|. Has no effect if called after CommonSetUp().
  void set_broker(IPC::AttachmentBrokerUnprivilegedWin* broker) {
    broker_.reset(broker);
  }

  void CommonSetUp() {
    if (!broker_.get())
      set_broker(new IPC::AttachmentBrokerUnprivilegedWin);
    broker_->AddObserver(&observer_);
    set_attachment_broker(broker_.get());
    CreateChannel(&proxy_listener_);
    broker_->DesignateBrokerCommunicationChannel(channel());
    ASSERT_TRUE(ConnectChannel());
    ASSERT_TRUE(StartClient());
  }

  void CommonTearDown() {
    // Close the channel so the client's OnChannelError() gets fired.
    channel()->Close();

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
    broker_.reset();
  }

  HANDLE CreateTempFile() {
    EXPECT_NE(-1, WriteFile(temp_path_, kDataBuffer,
                            static_cast<int>(strlen(kDataBuffer))));

    HANDLE h =
        CreateFile(temp_path_.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT_NE(h, INVALID_HANDLE_VALUE);
    return h;
  }

  void SendMessageWithAttachment(HANDLE h) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    message->WriteInt(message_index_++);
    scoped_refptr<IPC::internal::HandleAttachmentWin> attachment(
        new IPC::internal::HandleAttachmentWin(h, IPC::HandleWin::DUPLICATE));
    ASSERT_TRUE(message->WriteAttachment(attachment));
    sender()->Send(message);
  }

  ProxyListener* get_proxy_listener() { return &proxy_listener_; }
  IPC::AttachmentBrokerUnprivilegedWin* get_broker() { return broker_.get(); }
  MockObserver* get_observer() { return &observer_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_path_;
  int message_index_;
  ProxyListener proxy_listener_;
  scoped_ptr<IPC::AttachmentBrokerUnprivilegedWin> broker_;
  MockObserver observer_;
};

// A broker which always sets the current process as the destination process
// for attachments.
class MockBroker : public IPC::AttachmentBrokerUnprivilegedWin {
 public:
  MockBroker() {}
  ~MockBroker() override {}
  bool SendAttachmentToProcess(const IPC::BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override {
    return IPC::AttachmentBrokerUnprivilegedWin::SendAttachmentToProcess(
        attachment, base::Process::Current().Pid());
  }
};

// An unprivileged process makes a file HANDLE, and writes a string to it. The
// file HANDLE is sent to the privileged process using the attachment broker.
// The privileged process dups the HANDLE into its own HANDLE table. This test
// checks that the file has the same contents in the privileged process.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandle) {
  Init("SendHandle");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  SendMessageWithAttachment(h);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// Similar to SendHandle, except the file HANDLE attached to the message has
// neither read nor write permissions.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandleWithoutPermissions) {
  Init("SendHandleWithoutPermissions");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  HANDLE h2;
  BOOL result = ::DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(),
                                  &h2, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
  ASSERT_TRUE(result);
  SendMessageWithAttachment(h2);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// Similar to SendHandle, except the attachment's destination process is this
// process. This is an unrealistic scenario, but simulates an unprivileged
// process sending an attachment to another unprivileged process.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandleToSelf) {
  Init("SendHandleToSelf");

  set_broker(new MockBroker);
  CommonSetUp();
  // Technically, the channel is an endpoint, but we need the proxy listener to
  // receive the messages so that it can quit the message loop.
  channel()->SetAttachmentBrokerEndpoint(false);
  get_proxy_listener()->set_listener(get_broker());

  HANDLE h = CreateTempFile();
  SendMessageWithAttachment(h);
  base::MessageLoop::current()->Run();

  // Get the received attachment.
  IPC::BrokerableAttachment::AttachmentId* id = get_observer()->get_id();
  scoped_refptr<IPC::BrokerableAttachment> received_attachment;
  get_broker()->GetAttachmentWithId(*id, &received_attachment);
  ASSERT_NE(received_attachment.get(), nullptr);

  // Check that it's a new entry in the HANDLE table.
  HANDLE h2 = GetHandleFromBrokeredAttachment(received_attachment);
  EXPECT_NE(h2, h);
  EXPECT_NE(h2, nullptr);

  // But it still points to the same file.
  std::string contents = ReadFromFile(h);
  EXPECT_EQ(contents, std::string(kDataBuffer));

  CommonTearDown();
}

// Similar to SendHandle, except this test uses the HandleWin class.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandleWin) {
  Init("SendHandleWin");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  IPC::HandleWin handle_win(h, IPC::HandleWin::FILE_READ_WRITE);
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(0);
  IPC::ParamTraits<IPC::HandleWin>::Write(message, handle_win);
  sender()->Send(message);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

using OnMessageReceivedCallback =
    void (*)(MockObserver* observer,
             IPC::AttachmentBrokerPrivilegedWin* broker,
             IPC::Sender* sender);

int CommonPrivilegedProcessMain(OnMessageReceivedCallback callback,
                                const char* channel_name) {
  base::MessageLoopForIO main_message_loop;
  ProxyListener listener;

  // Set up IPC channel.
  IPC::AttachmentBrokerPrivilegedWin broker;
  listener.set_listener(&broker);
  scoped_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName(channel_name), &listener, &broker));
  broker.RegisterCommunicationChannel(channel.get());
  CHECK(channel->Connect());

  MockObserver observer;
  broker.AddObserver(&observer);

  while (true) {
    base::MessageLoop::current()->Run();
    ProxyListener::Reason reason = listener.get_reason();
    if (reason == ProxyListener::CHANNEL_ERROR)
      break;

    callback(&observer, &broker, channel.get());
  }

  return 0;
}

void SendHandleCallback(MockObserver* observer,
                        IPC::AttachmentBrokerPrivilegedWin* broker,
                        IPC::Sender* sender) {
  IPC::BrokerableAttachment::AttachmentId* id = observer->get_id();
  scoped_refptr<IPC::BrokerableAttachment> received_attachment;
  broker->GetAttachmentWithId(*id, &received_attachment);

  // Check that it's the expected handle.
  bool success = CheckContentsOfBrokeredAttachment(received_attachment);

  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandle) {
  return CommonPrivilegedProcessMain(&SendHandleCallback, "SendHandle");
}

void SendHandleWithoutPermissionsCallback(
    MockObserver* observer,
    IPC::AttachmentBrokerPrivilegedWin* broker,
    IPC::Sender* sender) {
  IPC::BrokerableAttachment::AttachmentId* id = observer->get_id();
  scoped_refptr<IPC::BrokerableAttachment> received_attachment;
  broker->GetAttachmentWithId(*id, &received_attachment);

  // Check that it's the expected handle.
  HANDLE h = GetHandleFromBrokeredAttachment(received_attachment);
  if (h != nullptr) {
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);

    char buffer[100];
    DWORD bytes_read;
    BOOL success =
        ::ReadFile(h, buffer, static_cast<DWORD>(strlen(kDataBuffer)),
                   &bytes_read, nullptr);
    if (!success && GetLastError() == ERROR_ACCESS_DENIED) {
      SendControlMessage(sender, true);
      return;
    }
  }

  SendControlMessage(sender, false);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleWithoutPermissions) {
  return CommonPrivilegedProcessMain(&SendHandleWithoutPermissionsCallback,
                                     "SendHandleWithoutPermissions");
}

void SendHandleToSelfCallback(MockObserver* observer,
                              IPC::AttachmentBrokerPrivilegedWin* broker,
                              IPC::Sender* sender) {
  // Do nothing special. The default behavior already runs the
  // AttachmentBrokerPrivilegedWin.
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleToSelf) {
  return CommonPrivilegedProcessMain(&SendHandleToSelfCallback,
                                     "SendHandleToSelf");
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleWin) {
  return CommonPrivilegedProcessMain(&SendHandleCallback, "SendHandleWin");
}

}  // namespace
