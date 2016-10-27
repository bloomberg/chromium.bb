// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_BASE_H_
#define IPC_IPC_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_multiprocess_test.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"

namespace base {
class MessageLoop;
}

namespace IPC {
class AttachmentBroker;
}

// A test fixture for multiprocess IPC tests. Such tests include a "client" side
// (running in a separate process). The same client may be shared between
// several different tests.
class IPCTestBase : public base::MultiProcessTest {
 public:
  // The channel name is based on the client's name. This is a public static
  // helper to be used by the client-side code; server-side test code should
  // usually not use this (directly).
  static std::string GetChannelName(const std::string& test_client_name);

 protected:
  IPCTestBase();
  ~IPCTestBase() override;

  void TearDown() override;

  // Initializes the test to use the given client and creates an IO message loop
  // on the current thread.
  void Init(const std::string& test_client_name);
  // Some tests create separate thread for IO message loop and run non-IO
  // message loop on the main thread. As IPCTestBase creates IO message loop by
  // default, such tests need to provide a custom message loop for the main
  // thread.
  virtual void InitWithCustomMessageLoop(
      const std::string& test_client_name,
      std::unique_ptr<base::MessageLoop> message_loop);

  // Creates a channel with the given listener and connects to the channel
  // (returning true if successful), respectively. Use these to use a channel
  // directly. Since the listener must outlive the channel, you must destroy the
  // channel before the listener gets destroyed.
  void CreateChannel(IPC::Listener* listener);
  bool ConnectChannel();
  void DestroyChannel();

  // Releases or replaces existing channel.
  // These are useful for testing specific types of channel subclasses.
  std::unique_ptr<IPC::Channel> ReleaseChannel();
  void SetChannel(std::unique_ptr<IPC::Channel> channel);

  // Use this instead of CreateChannel() if you want to use some different
  // channel specification (then use ConnectChannel() as usual).
  void CreateChannelFromChannelHandle(const IPC::ChannelHandle& channel_handle,
                                      IPC::Listener* listener);

  // Creates a channel proxy with the given listener and task runner. (The
  // channel proxy will automatically create and connect a channel.) You must
  // (manually) destroy the channel proxy before the task runner's thread is
  // destroyed.
  void CreateChannelProxy(
      IPC::Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);
  void DestroyChannelProxy();

  // Starts the client process, returning true if successful; this should be
  // done after connecting to the channel.
  virtual bool StartClient();

#if defined(OS_POSIX)
  // A StartClient() variant that allows caller to pass the FD of IPC pipe
  bool StartClientWithFD(int ipcfd);
#endif

  // Waits for the client to shut down, returning true if successful. Note that
  // this does not initiate client shutdown; that must be done by the test
  // (somehow). This must be called before the end of the test whenever
  // StartClient() was called successfully.
  virtual bool WaitForClientShutdown();

  IPC::ChannelHandle GetTestChannelHandle();

  // Use this to send IPC messages (when you don't care if you're using a
  // channel or a proxy).
  IPC::Sender* sender() {
    return channel_.get() ? static_cast<IPC::Sender*>(channel_.get()) :
                            static_cast<IPC::Sender*>(channel_proxy_.get());
  }

  IPC::Channel* channel() { return channel_.get(); }
  IPC::ChannelProxy* channel_proxy() { return channel_proxy_.get(); }
  void set_channel_proxy(std::unique_ptr<IPC::ChannelProxy> proxy) {
    DCHECK(!channel_proxy_);
    channel_proxy_.swap(proxy);
  }

  const base::Process& client_process() const { return client_process_; }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner();

  virtual std::unique_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SingleThreadTaskRunner* runner);

  virtual bool DidStartClient();

 private:
  std::string GetTestMainName() const;

  std::string test_client_name_;
  std::unique_ptr<base::MessageLoop> message_loop_;

  std::unique_ptr<IPC::Channel> channel_;
  std::unique_ptr<IPC::ChannelProxy> channel_proxy_;

  base::Process client_process_;

  DISALLOW_COPY_AND_ASSIGN(IPCTestBase);
};

class IPCChannelMojoTestBase : public testing::Test {
 public:
  IPCChannelMojoTestBase();
  ~IPCChannelMojoTestBase() override;

  void Init(const std::string& test_client_name);

  bool WaitForClientShutdown();

  void TearDown() override;

  void CreateChannel(IPC::Listener* listener);

  bool ConnectChannel();

  void DestroyChannel();

  IPC::Sender* sender() { return channel(); }
  IPC::Channel* channel() { return channel_.get(); }
  const base::Process& client_process() const { return helper_.test_child(); }

 protected:
  mojo::ScopedMessagePipeHandle TakeHandle();

 private:
  base::MessageLoop message_loop_;

  mojo::ScopedMessagePipeHandle handle_;
  mojo::edk::test::MultiprocessTestHelper helper_;

  std::unique_ptr<IPC::Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(IPCChannelMojoTestBase);
};

class IpcChannelMojoTestClient {
 public:
  IpcChannelMojoTestClient();
  ~IpcChannelMojoTestClient();

  void Init(mojo::ScopedMessagePipeHandle handle);

  void Connect(IPC::Listener* listener);

  void Close();

  IPC::Channel* channel() const { return channel_.get(); }

 private:
  base::MessageLoopForIO main_message_loop_;
  mojo::ScopedMessagePipeHandle handle_;
  std::unique_ptr<IPC::Channel> channel_;
};

// Use this to declare the client side for tests using IPCTestBase.
#define MULTIPROCESS_IPC_TEST_CLIENT_MAIN(test_client_name) \
    MULTIPROCESS_IPC_TEST_MAIN(test_client_name ## TestClientMain)

// Use this to declare the client side for tests using IPCChannelMojoTestBase
// when a custom test fixture class is required in the client. |test_base| must
// be derived from IpcChannelMojoTestClient.
#define DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(client_name,  \
                                                                test_base)    \
  class client_name##_MainFixture : public test_base {                        \
   public:                                                                    \
    void Main();                                                              \
  };                                                                          \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                          \
      client_name##TestChildMain,                                             \
      ::mojo::edk::test::MultiprocessTestHelper::ChildSetup) {                \
    client_name##_MainFixture test;                                           \
    test.Init(                                                                \
        std::move(mojo::edk::test::MultiprocessTestHelper::primordial_pipe)); \
    test.Main();                                                              \
    return (::testing::Test::HasFatalFailure() ||                             \
            ::testing::Test::HasNonfatalFailure())                            \
               ? 1                                                            \
               : 0;                                                           \
  }                                                                           \
  void client_name##_MainFixture::Main()

// Use this to declare the client side for tests using IPCChannelMojoTestBase.
#define DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(client_name)   \
  DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE( \
      client_name, IpcChannelMojoTestClient)

#endif  // IPC_IPC_TEST_BASE_H_
