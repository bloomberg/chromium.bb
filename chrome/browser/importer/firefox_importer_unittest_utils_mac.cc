// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_unittest_utils.h"

#include "base/bind.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_multiprocess_test.h"
#include "ipc/ipc_switches.h"
#include "testing/multiprocess_func_list.h"

#define IPC_MESSAGE_IMPL
#include "chrome/browser/importer/firefox_importer_unittest_messages_internal.h"

namespace {

// Name of IPC Channel to use for Server<-> Child Communications.
const char kTestChannelID[] = "T1";

// Launch the child process:
// |nss_path| - path to the NSS directory holding the decryption libraries.
// |channel| - IPC Channel to use for communication.
// |handle| - On return, the process handle to use to communicate with the
// child.
bool LaunchNSSDecrypterChildProcess(const base::FilePath& nss_path,
    IPC::Channel* channel, base::ProcessHandle* handle) {
  CommandLine cl(*CommandLine::ForCurrentProcess());
  cl.AppendSwitchASCII(switches::kTestChildProcess, "NSSDecrypterChildProcess");

  // Set env variable needed for FF encryption libs to load.
  // See "chrome/browser/importer/nss_decryptor_mac.mm" for an explanation of
  // why we need this.
  base::EnvironmentVector env;
  std::pair<std::string, std::string> dyld_override;
  dyld_override.first = "DYLD_FALLBACK_LIBRARY_PATH";
  dyld_override.second = nss_path.value();
  env.push_back(dyld_override);

  int ipcfd = channel->TakeClientFileDescriptor();
  if (ipcfd == -1)
    return false;

  file_util::ScopedFD client_file_descriptor_closer(&ipcfd);
  base::FileHandleMappingVector fds_to_map;
  fds_to_map.push_back(std::pair<int,int>(ipcfd, kPrimaryIPCChannel + 3));

  bool debug_on_start = CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kDebugChildren);
  base::LaunchOptions options;
  options.environ = &env;
  options.fds_to_remap = &fds_to_map;
  options.wait = debug_on_start;
  return base::LaunchProcess(cl.argv(), options, handle);
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
    MessageLoop::current()->Quit();
  }

  void OnDecryptedTextResonse(const string16& decrypted_text) {
    DCHECK(!got_result);
    result_string = decrypted_text;
    got_result = true;
    MessageLoop::current()->Quit();
  }

  void QuitClient() {
    if (sender_)
      sender_->Send(new Msg_Decryptor_Quit());
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FFDecryptorServerChannelListener, msg)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_InitReturnCode, OnInitDecryptorResponse)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Response, OnDecryptedTextResonse)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // If an error occured, just kill the message Loop.
  virtual void OnChannelError() OVERRIDE {
    got_result = false;
    MessageLoop::current()->Quit();
  }

  // Results of IPC calls.
  string16 result_string;
  bool result_bool;
  // True if IPC call succeeded and data in above variables is valid.
  bool got_result;

 private:
  IPC::Sender* sender_;  // weak
};

FFUnitTestDecryptorProxy::FFUnitTestDecryptorProxy()
    : child_process_(0) {
}

bool FFUnitTestDecryptorProxy::Setup(const base::FilePath& nss_path) {
  // Create a new message loop and spawn the child process.
  message_loop_.reset(new MessageLoopForIO());

  listener_.reset(new FFDecryptorServerChannelListener());
  channel_.reset(new IPC::Channel(kTestChannelID,
                                  IPC::Channel::MODE_SERVER,
                                  listener_.get()));
  CHECK(channel_->Connect());
  listener_->SetSender(channel_.get());

  // Spawn child and set up sync IPC connection.
  bool ret = LaunchNSSDecrypterChildProcess(nss_path,
                                            channel_.get(),
                                            &child_process_);
  return ret && (child_process_ != 0);
}

FFUnitTestDecryptorProxy::~FFUnitTestDecryptorProxy() {
  listener_->QuitClient();
  channel_->Close();

  if (child_process_) {
    base::WaitForSingleProcess(child_process_, base::TimeDelta::FromSeconds(5));
    base::CloseProcessHandle(child_process_);
  }
}

// A message_loop task that quits the message loop when invoked, setting cancel
// causes the task to do nothing when invoked.
class CancellableQuitMsgLoop : public base::RefCounted<CancellableQuitMsgLoop> {
 public:
  CancellableQuitMsgLoop() : cancelled_(false) {}
  void QuitNow() {
    if (!cancelled_)
      MessageLoop::current()->Quit();
  }
  bool cancelled_;

 private:
  friend class base::RefCounted<CancellableQuitMsgLoop>;
  ~CancellableQuitMsgLoop() {}
};

// Spin until either a client response arrives or a timeout occurs.
bool FFUnitTestDecryptorProxy::WaitForClientResponse() {
  // What we're trying to do here is to wait for an RPC message to go over the
  // wire and the client to reply.  If the client does not reply by a given
  // timeout we kill the message loop.
  // The way we do this is to post a CancellableQuitMsgLoop for 3 seconds in
  // the future and cancel it if an RPC message comes back earlier.
  // This relies on the IPC listener class to quit the message loop itself when
  // a message comes in.
  scoped_refptr<CancellableQuitMsgLoop> quit_task(
      new CancellableQuitMsgLoop());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CancellableQuitMsgLoop::QuitNow, quit_task.get()),
      TestTimeouts::action_max_timeout());

  message_loop_->Run();
  bool ret = !quit_task->cancelled_;
  quit_task->cancelled_ = false;
  return ret;
}

bool FFUnitTestDecryptorProxy::DecryptorInit(const base::FilePath& dll_path,
                                             const base::FilePath& db_path) {
  channel_->Send(new Msg_Decryptor_Init(dll_path, db_path));
  bool ok = WaitForClientResponse();
  if (ok && listener_->got_result) {
    listener_->got_result = false;
    return listener_->result_bool;
  }
  return false;
}

string16 FFUnitTestDecryptorProxy::Decrypt(const std::string& crypt) {
  channel_->Send(new Msg_Decrypt(crypt));
  bool ok = WaitForClientResponse();
  if (ok && listener_->got_result) {
    listener_->got_result = false;
    return listener_->result_string;
  }
  return string16();
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

  void OnDecrypt(std::string crypt) {
    string16 unencrypted_str = decryptor_.Decrypt(crypt);
    sender_->Send(new Msg_Decryptor_Response(unencrypted_str));
  }

  void OnQuitRequest() {
    MessageLoop::current()->Quit();
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FFDecryptorClientChannelListener, msg)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Init, OnDecryptor_Init)
      IPC_MESSAGE_HANDLER(Msg_Decrypt, OnDecrypt)
      IPC_MESSAGE_HANDLER(Msg_Decryptor_Quit, OnQuitRequest)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  virtual void OnChannelError() OVERRIDE {
    MessageLoop::current()->Quit();
  }

 private:
  NSSDecryptor decryptor_;
  IPC::Sender* sender_;
};

// Entry function in child process.
MULTIPROCESS_IPC_TEST_MAIN(NSSDecrypterChildProcess) {
  MessageLoopForIO main_message_loop;
  FFDecryptorClientChannelListener listener;

  IPC::Channel channel(kTestChannelID, IPC::Channel::MODE_CLIENT, &listener);
  CHECK(channel.Connect());
  listener.SetSender(&channel);

  // run message loop
  MessageLoop::current()->Run();

  return 0;
}
