// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
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

class AdbAttachCommand : public base::RefCounted<AdbAttachCommand> {
 public:
  explicit AdbAttachCommand(scoped_refptr<DevToolsAdbBridge::RemotePage> page)
      : page_(page) {
  }

  void Run() {
    AdbClientSocket::HttpQuery(
        kAdbPort, page_->serial(), kDevToolsChannelName,
        base::StringPrintf(kWebSocketUpgradeRequest,
                           page_->debug_url().c_str()),
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
    scoped_ptr<net::TCPClientSocket> tcp_socket(socket);
    // TODO(pfeldman): Show DevToolsWindow here.
  }

  scoped_refptr<DevToolsAdbBridge::RemotePage> page_;
};

}  // namespace

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

void DevToolsAdbBridge::Attach(scoped_refptr<RemotePage> page) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  scoped_refptr<AdbAttachCommand> command(new AdbAttachCommand(page));
  adb_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AdbAttachCommand::Run, command));
}
