// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/firefox_importer_unittest_utils.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/global_descriptors.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/mojo_channel_switches.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "testing/multiprocess_func_list.h"

#define IPC_MESSAGE_IMPL
#include "chrome/utility/importer/firefox_importer_unittest_messages_internal.h"

namespace {

// Launch the child process:
// |nss_path| - path to the NSS directory holding the decryption libraries.
// |mojo_handle| - platform handle for Mojo transport.
// |mojo_channel_token| - token for creating the IPC channel over Mojo.
base::Process LaunchNSSDecrypterChildProcess(
    const base::FilePath& nss_path,
    mojo::edk::ScopedPlatformHandle mojo_handle,
    const std::string& mojo_channel_token) {
  base::CommandLine cl(*base::CommandLine::ForCurrentProcess());
  cl.AppendSwitchASCII(switches::kTestChildProcess, "NSSDecrypterChildProcess");
  cl.AppendSwitchASCII(switches::kMojoChannelToken, mojo_channel_token);

  // Set env variable needed for FF encryption libs to load.
  // See "chrome/utility/importer/nss_decryptor_mac.mm" for an explanation of
  // why we need this.
  base::LaunchOptions options;
  options.environ["DYLD_FALLBACK_LIBRARY_PATH"] = nss_path.value();

  base::FileHandleMappingVector fds_to_map;
  fds_to_map.push_back(std::pair<int, int>(
      mojo_handle.get().handle,
      kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor));

  options.fds_to_remap = &fds_to_map;
  return base::LaunchProcess(cl.argv(), options);
}

}  // namespace

//----------------------- Server --------------------

// Class to communicate on the server side of the IPC Channel.
// Method calls are sent over IPC and replies are read back into class
// variables.
// This class needs to be called on a single thread.
class FFDecryptorServerChannelListener : public IPC::Listener {
 public:
  FFDecryptorServerChannelListener()
      : got_result(false), sender_(NULL) {}

  void SetSender(IPC::Sender* sender) {
    sender_ = sender;
  }

  void OnInitDecryptorResponse(bool result) {
    DCHECK(!got_result);
    result_bool = result;
    got_result = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnDecryptedTextResponse(const base::string16& decrypted_text) {
    DCHECK(!got_result);
    result_string = decrypted_text;
    got_result = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnParseSignonsResponse(
      const std::vector<autofill::PasswordForm>& parsed_vector) {
    DCHECK(!got_result);
    result_vector = parsed_vector;
    got_result = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void QuitClient() {
    if (sender_)
      sender_->Send(new Msg_Decryptor_Quit());
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FFDecryptorServerChannelListener, msg)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_InitReturnCode, OnInitDecryptorResponse)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Response, OnDecryptedTextResponse)
      IPC_MESSAGE_HANDLER(Msg_ParseSignons_Response, OnParseSignonsResponse)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // If an error occured, just kill the message Loop.
  void OnChannelError() override {
    got_result = false;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  // Results of IPC calls.
  base::string16 result_string;
  std::vector<autofill::PasswordForm> result_vector;
  bool result_bool;
  // True if IPC call succeeded and data in above variables is valid.
  bool got_result;

 private:
  IPC::Sender* sender_;  // weak
};

FFUnitTestDecryptorProxy::FFUnitTestDecryptorProxy() {
}

bool FFUnitTestDecryptorProxy::Setup(const base::FilePath& nss_path) {
  // Create a new message loop and spawn the child process.
  message_loop_.reset(new base::MessageLoopForIO());

  listener_.reset(new FFDecryptorServerChannelListener());

  // Set up IPC channel using ChannelMojo.
  mojo::edk::PendingProcessConnection process;
  std::string token;
  mojo::ScopedMessagePipeHandle parent_pipe = process.CreateMessagePipe(&token);
  channel_ = IPC::Channel::CreateServer(parent_pipe.release(), listener_.get());
  CHECK(channel_->Connect());
  listener_->SetSender(channel_.get());

  // Spawn child and set up sync IPC connection.
  mojo::edk::PlatformChannelPair channel_pair;
  child_process_ = LaunchNSSDecrypterChildProcess(
      nss_path, channel_pair.PassClientHandle(), token);
  if (child_process_.IsValid())
    process.Connect(child_process_.Handle(), channel_pair.PassServerHandle());
  return child_process_.IsValid();
}

FFUnitTestDecryptorProxy::~FFUnitTestDecryptorProxy() {
  listener_->QuitClient();
  channel_->Close();

  if (child_process_.IsValid()) {
    int exit_code;
    child_process_.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(5),
                                          &exit_code);
  }
}

// Spin until either a client response arrives or a timeout occurs.
void FFUnitTestDecryptorProxy::WaitForClientResponse() {
  // What we're trying to do here is to wait for an RPC message to go over the
  // wire and the client to reply.  If the client does not reply by a given
  // timeout we kill the message loop.
  // This relies on the IPC listener class to quit the message loop itself when
  // a message comes in.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(),
      TestTimeouts::action_max_timeout());
  run_loop.Run();
}

bool FFUnitTestDecryptorProxy::DecryptorInit(const base::FilePath& dll_path,
                                             const base::FilePath& db_path) {
  channel_->Send(new Msg_Decryptor_Init(dll_path, db_path));
  WaitForClientResponse();
  if (listener_->got_result) {
    listener_->got_result = false;
    return listener_->result_bool;
  }
  return false;
}

base::string16 FFUnitTestDecryptorProxy::Decrypt(const std::string& crypt) {
  channel_->Send(new Msg_Decrypt(crypt));
  WaitForClientResponse();
  if (listener_->got_result) {
    listener_->got_result = false;
    return listener_->result_string;
  }
  return base::string16();
}

std::vector<autofill::PasswordForm> FFUnitTestDecryptorProxy::ParseSignons(
    const base::FilePath& signons_path) {
  channel_->Send(new Msg_ParseSignons(signons_path));
  WaitForClientResponse();
  if (listener_->got_result) {
    listener_->got_result = false;
    return listener_->result_vector;
  }
  return std::vector<autofill::PasswordForm>();
}

//---------------------------- Child Process -----------------------

// Class to listen on the client side of the ipc channel, it calls through
// to the NSSDecryptor and sends back a reply.
class FFDecryptorClientChannelListener : public IPC::Listener {
 public:
  FFDecryptorClientChannelListener()
      : sender_(NULL) {}

  void SetSender(IPC::Sender* sender) {
    sender_ = sender;
  }

  void OnDecryptor_Init(base::FilePath dll_path, base::FilePath db_path) {
    bool ret = decryptor_.Init(dll_path, db_path);
    sender_->Send(new Msg_Decryptor_InitReturnCode(ret));
  }

  void OnDecrypt(const std::string& crypt) {
    base::string16 unencrypted_str = decryptor_.Decrypt(crypt);
    sender_->Send(new Msg_Decryptor_Response(unencrypted_str));
  }

  void OnParseSignons(base::FilePath signons_path) {
    std::vector<autofill::PasswordForm> forms;
    decryptor_.ReadAndParseSignons(signons_path, &forms);
    sender_->Send(new Msg_ParseSignons_Response(forms));
  }

  void OnQuitRequest() { base::MessageLoop::current()->QuitWhenIdle(); }

  bool OnMessageReceived(const IPC::Message& msg) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FFDecryptorClientChannelListener, msg)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Init, OnDecryptor_Init)
      IPC_MESSAGE_HANDLER(Msg_Decrypt, OnDecrypt)
      IPC_MESSAGE_HANDLER(Msg_ParseSignons, OnParseSignons)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Quit, OnQuitRequest)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnChannelError() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  NSSDecryptor decryptor_;
  IPC::Sender* sender_;
};

// Entry function in child process.
MULTIPROCESS_TEST_MAIN(NSSDecrypterChildProcess) {
  base::MessageLoopForIO main_message_loop;
  FFDecryptorClientChannelListener listener;

  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(
          kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor)));
  mojo::ScopedMessagePipeHandle mojo_handle =
      mojo::edk::CreateChildMessagePipe(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kMojoChannelToken));

  std::unique_ptr<IPC::Channel> channel =
      IPC::Channel::CreateClient(mojo_handle.release(), &listener);
  CHECK(channel->Connect());
  listener.SetSender(channel.get());

  // run message loop
  base::RunLoop().Run();

  return 0;
}
