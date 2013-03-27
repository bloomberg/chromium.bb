// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

static const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";
static const char kDevToolsChannelName[] = "chrome_devtools_remote";
static const char kHostDevicesCommand[] = "host:devices";
static const char kDeviceModelCommand[] =
    "host:transport:%s|shell:getprop ro.product.model";

static const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";
const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;

typedef DevToolsAdbBridge::Callback Callback;
typedef DevToolsAdbBridge::PagesCallback PagesCallback;

class AdbQueryCommand : public base::RefCounted<AdbQueryCommand> {
 public:
  AdbQueryCommand(const std::string& query,
                  const Callback& callback)
      : query_(query),
        callback_(callback) {
  }

  void Run() {
    AdbClientSocket::AdbQuery(kAdbPort, query_,
                              base::Bind(&AdbQueryCommand::Handle, this));
  }

 private:
  friend class base::RefCounted<AdbQueryCommand>;
  virtual ~AdbQueryCommand() {}

  void Handle(int result, const std::string& response) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbQueryCommand::Respond, this, result, response));
  }

  void Respond(int result, const std::string& response) {
    callback_.Run(result, response);
  }

  std::string query_;
  Callback callback_;
};

class AdbPagesCommand : public base::RefCounted<AdbPagesCommand> {
 public:
  explicit AdbPagesCommand(const PagesCallback& callback)
     : callback_(callback) {
    pages_.reset(new DevToolsAdbBridge::RemotePages());
  }

  void Run() {
    AdbClientSocket::AdbQuery(
        kAdbPort, kHostDevicesCommand,
        base::Bind(&AdbPagesCommand::ReceivedDevices, this));
  }

 private:
  friend class base::RefCounted<AdbPagesCommand>;
  virtual ~AdbPagesCommand() {}

  void ReceivedDevices(int result, const std::string& response) {
    if (result != net::OK) {
      ProcessSerials();
      return;
    }

    std::vector<std::string> devices;
    Tokenize(response, "\n", &devices);
    for (size_t i = 0; i < devices.size(); ++i) {
      std::vector<std::string> tokens;
      Tokenize(devices[i], "\t ", &tokens);
      std::string serial = tokens[0];
      serials_.push_back(serial);
    }

    ProcessSerials();
  }

  void ProcessSerials() {
    if (serials_.size() == 0) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&AdbPagesCommand::Respond, this));
      return;
    }

    AdbClientSocket::AdbQuery(
      kAdbPort,
      base::StringPrintf(kDeviceModelCommand, serials_.back().c_str()),
      base::Bind(&AdbPagesCommand::ReceivedModel, this));
  }

  void ReceivedModel(int result, const std::string& response) {
    if (result != net::OK) {
      serials_.pop_back();
      ProcessSerials();
      return;
    }

    AdbClientSocket::HttpQuery(
      kAdbPort, serials_.back(), kDevToolsChannelName, kPageListRequest,
      base::Bind(&AdbPagesCommand::ReceivedPages, this, response));
  }

  void ReceivedPages(const std::string& model,
                     int result,
                     const std::string& response) {
    std::string serial = serials_.back();
    serials_.pop_back();
    if (result < 0) {
      ProcessSerials();
      return;
    }

    std::string body = response.substr(result);
    scoped_ptr<base::Value> value(base::JSONReader::Read(body));
    base::ListValue* list_value;
    if (!value || !value->GetAsList(&list_value)) {
      ProcessSerials();
      return;
    }

    base::Value* item;
    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      list_value->Get(i, &item);
      base::DictionaryValue* dict;
      if (!item || !item->GetAsDictionary(&dict))
        continue;
      pages_->push_back(
          new DevToolsAdbBridge::RemotePage(serial, model, *dict));
    }
    ProcessSerials();
  }

  void Respond() {
    callback_.Run(net::OK, pages_.release());
  }

  PagesCallback callback_;
  std::vector<std::string> serials_;
  scoped_ptr<DevToolsAdbBridge::RemotePages> pages_;
};

}  // namespace

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

class AgentHostDelegate : public base::RefCountedThreadSafe<AgentHostDelegate>,
                          public content::DevToolsExternalAgentProxyDelegate {
 public:
  AgentHostDelegate(
      const std::string& id,
      scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
      net::TCPClientSocket* socket)
      : id_(id),
        adb_thread_(adb_thread),
        socket_(socket) {
    AddRef();  // Balanced in SelfDestruct.
    proxy_.reset(content::DevToolsExternalAgentProxy::Create(this));
    g_host_delegates.Get()[id] = this;
  }

  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() {
    return proxy_->GetAgentHost();
  }

 private:
  friend class base::RefCountedThreadSafe<AgentHostDelegate>;

  virtual ~AgentHostDelegate() {
    g_host_delegates.Get().erase(id_);
  }

  virtual void Attach() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::StartListeningOnHandlerThread, this));
  }

  virtual void Detach() OVERRIDE {
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::CloseConnection, this, net::OK, false));
  }

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::SendFrameOnHandlerThread, this,
                   message));
  }

  void StartListeningOnHandlerThread() {
    scoped_refptr<net::IOBuffer> response_buffer =
        new net::IOBuffer(kBufferSize);
    int result = socket_->Read(response_buffer, kBufferSize,
        base::Bind(&AgentHostDelegate::OnBytesRead, this, response_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(response_buffer, result);
  }

  void OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer, int result) {
    if (!socket_)
      return;

    if (result <= 0) {
      CloseIfNecessary(net::ERR_CONNECTION_CLOSED);
      return;
    }

    std::string data = std::string(response_buffer->data(), result);
    response_buffer_ += data;

    int bytes_consumed;
    std::string output;
    WebSocket::ParseResult parse_result = WebSocket::DecodeFrameHybi17(
        response_buffer_, false, &bytes_consumed, &output);

    while (parse_result == WebSocket::FRAME_OK) {
      response_buffer_ = response_buffer_.substr(bytes_consumed);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&AgentHostDelegate::OnFrameRead, this, output));
      parse_result = WebSocket::DecodeFrameHybi17(
          response_buffer_, false, &bytes_consumed, &output);
    }

    if (parse_result == WebSocket::FRAME_ERROR ||
        parse_result == WebSocket::FRAME_CLOSE) {
      CloseIfNecessary(net::ERR_CONNECTION_CLOSED);
      return;
    }

    result = socket_->Read(response_buffer, kBufferSize,
        base::Bind(&AgentHostDelegate::OnBytesRead, this, response_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(response_buffer, result);
  }

  void SendFrameOnHandlerThread(const std::string& data) {
    int mask = base::RandInt(0, 0x7FFFFFFF);
    std::string encoded_frame = WebSocket::EncodeFrameHybi17(data, mask);
    scoped_refptr<net::StringIOBuffer> request_buffer =
        new net::StringIOBuffer(encoded_frame);
    if (!socket_)
      return;
    int result = socket_->Write(request_buffer, request_buffer->size(),
        base::Bind(&AgentHostDelegate::CloseIfNecessary, this));
    if (result != net::ERR_IO_PENDING)
      CloseIfNecessary(result);
  }

  void CloseConnection(int result, bool initiated_by_me) {
    if (!socket_)
      return;
    socket_->Disconnect();
    socket_.reset();
    if (initiated_by_me) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&AgentHostDelegate::OnSocketClosed, this, result));
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AgentHostDelegate::SelfDestruct, this));
  }

  void SelfDestruct() {
    Release();  // Balanced in constructor.
  }

  void CloseIfNecessary(int result) {
    if (result >= 0)
      return;
    CloseConnection(result, true);
  }

  void OnFrameRead(const std::string& message) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(message));
    DictionaryValue* dvalue;
    if (!value || !value->GetAsDictionary(&dvalue))
      return;

    content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
    content::DevToolsClientHost* client_host =
        manager->GetDevToolsClientHostFor(proxy_->GetAgentHost());
    if (client_host)
      client_host->DispatchOnInspectorFrontend(message);
  }

  void OnSocketClosed(int result) {
    proxy_->ConnectionClosed();
  }

  std::string id_;
  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  scoped_ptr<net::TCPClientSocket> socket_;
  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  std::string response_buffer_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

class AdbAttachCommand : public base::RefCounted<AdbAttachCommand> {
 public:
  explicit AdbAttachCommand(const base::WeakPtr<DevToolsAdbBridge>& bridge,
                            const std::string& serial,
                            const std::string& debug_url,
                            const std::string& frontend_url)
      : bridge_(bridge),
        serial_(serial),
        debug_url_(debug_url),
        frontend_url_(frontend_url) {
  }

  void Run() {
    AdbClientSocket::HttpQuery(
        kAdbPort, serial_, kDevToolsChannelName,
        base::StringPrintf(kWebSocketUpgradeRequest, debug_url_.c_str()),
        base::Bind(&AdbAttachCommand::Handle, this));
  }

 private:
  friend class base::RefCounted<AdbAttachCommand>;
  virtual ~AdbAttachCommand() {}

  void Handle(int result, net::TCPClientSocket* socket) {
    if (result != net::OK || socket == NULL)
      return;

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbAttachCommand::OpenDevToolsWindow, this, socket));
  }

  void OpenDevToolsWindow(net::TCPClientSocket* socket) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    DevToolsAdbBridge* bridge = bridge_.get();
    if (!bridge)
      return;

    std::string id = base::StringPrintf("%s:%s", serial_.c_str(),
                                        debug_url_.c_str());
    AgentHostDelegates::iterator it = g_host_delegates.Get().find(id);
    AgentHostDelegate* delegate;
    if (it != g_host_delegates.Get().end())
      delegate = it->second;
    else
      delegate = new AgentHostDelegate(id, bridge->adb_thread_, socket);
    // TODO(pfeldman): uncomment once DevToolsWindow is ready for external
    // hosts.
    // DevToolsWindow::OpenExternalFrontend(bridge->profile_,
    //                                      frontend_url_,
    //                                      delegate->GetAgentHost());
    LOG(INFO) << delegate;
  }

  base::WeakPtr<DevToolsAdbBridge> bridge_;
  std::string serial_;
  std::string debug_url_;
  std::string frontend_url_;
};

DevToolsAdbBridge::RemotePage::RemotePage(const std::string& serial,
                                          const std::string& model,
                                          const base::DictionaryValue& value)
    : serial_(serial),
      model_(model) {
  value.GetString("id", &id_);
  value.GetString("url", &url_);
  value.GetString("title", &title_);
  value.GetString("descirption", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (debug_url_.find("ws://") == 0)
    debug_url_ = debug_url_.substr(5);
  else
    debug_url_ = "";

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);
}

DevToolsAdbBridge::RemotePage::~RemotePage() {
}

DevToolsAdbBridge::RefCountedAdbThread*
DevToolsAdbBridge::RefCountedAdbThread::instance_ = NULL;

// static
scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread>
DevToolsAdbBridge::RefCountedAdbThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new RefCountedAdbThread();
  return instance_;
}

DevToolsAdbBridge::RefCountedAdbThread::RefCountedAdbThread() {
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

MessageLoop* DevToolsAdbBridge::RefCountedAdbThread::message_loop() {
  return thread_ ? thread_->message_loop() : NULL;
}

// static
void DevToolsAdbBridge::RefCountedAdbThread::StopThread(base::Thread* thread) {
  thread->Stop();
}

DevToolsAdbBridge::RefCountedAdbThread::~RefCountedAdbThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RefCountedAdbThread::StopThread, thread_));
}

DevToolsAdbBridge::DevToolsAdbBridge(Profile* profile)
    : profile_(profile),
      adb_thread_(RefCountedAdbThread::GetInstance()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      has_message_loop_(adb_thread_->message_loop() != NULL) {
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DevToolsAdbBridge::Query(
    const std::string query,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_) {
    callback.Run(net::ERR_FAILED, "Could not start ADB thread");
    return;
  }
  scoped_refptr<AdbQueryCommand> command(new AdbQueryCommand(query, callback));
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AdbQueryCommand::Run, command));
}

void DevToolsAdbBridge::Pages(const PagesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  scoped_refptr<AdbPagesCommand> command(new AdbPagesCommand(callback));
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AdbPagesCommand::Run, command));
}

void DevToolsAdbBridge::Attach(const std::string& serial,
                               const std::string& debug_url,
                               const std::string& frontend_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  scoped_refptr<AdbAttachCommand> command(
      new AdbAttachCommand(weak_factory_.GetWeakPtr(), serial, debug_url,
                           frontend_url));
  adb_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AdbAttachCommand::Run, command));
}
