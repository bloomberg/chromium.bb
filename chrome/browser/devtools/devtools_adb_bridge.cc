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
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

static const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";
static const char kDevToolsChannelName[] = "chrome_devtools_remote";
static const char kHostDevicesCommand[] = "host:devices";
static const char kDeviceModelCommand[] =
    "host:transport:%s|shell:getprop ro.product.model";
static const char kPageListQuery[] = "/json";
const int kAdbPort = 5037;

}  // namespace

DevToolsAdbBridge::AgentHost::AgentHost(const std::string& serial,
                                        const std::string& model,
                                        const base::DictionaryValue& value)
    : serial_(serial),
      model_(model) {
  value.GetString("id", &id_);
  value.GetString("title", &title_);
  value.GetString("descirption", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
}

DevToolsAdbBridge::AgentHost::~AgentHost() {
}

// static
DevToolsAdbBridge* DevToolsAdbBridge::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new DevToolsAdbBridge();
}

void DevToolsAdbBridge::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!thread_.get()) {
    ResetHandlerAndReleaseOnUIThread();
    return;
  }
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsAdbBridge::StopHandlerOnFileThread,
                 base::Unretained(this)),
      base::Bind(&DevToolsAdbBridge::ResetHandlerAndReleaseOnUIThread,
                 base::Unretained(this)));
}

void DevToolsAdbBridge::Query(
    const std::string query,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // There is a race condition in case Query immediately follows start. We
  // consider it Ok since query is polling anyways.
  if (!thread_.get()) {
    callback.Run(net::ERR_FAILED, "ADB is not yet connected");
    return;
  }
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::QueryOnHandlerThread,
                 base::Unretained(this), query, callback));
}

void DevToolsAdbBridge::Devices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!thread_.get())
    return;

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::DevicesOnHandlerThread,
                 base::Unretained(this),
                 base::Bind(&DevToolsAdbBridge::PrintHosts,
                            base::Unretained(this))));
}

DevToolsAdbBridge::DevToolsAdbBridge() {
  thread_.reset(new base::Thread(kDevToolsAdbBridgeThreadName));

  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options))
    thread_.reset();
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop() must be called prior to destruction.
  DCHECK(thread_.get() == NULL);
}

// Runs on FILE thread to make sure that it is serialized against
// {Start|Stop}HandlerThread and to allow calling pthread_join.
void DevToolsAdbBridge::StopHandlerOnFileThread() {
  if (!thread_->message_loop())
    return;
  // Thread::Stop joins the thread.
  thread_->Stop();
}

void DevToolsAdbBridge::ResetHandlerAndReleaseOnUIThread() {
  ResetHandlerOnUIThread();
  delete this;
}

void DevToolsAdbBridge::ResetHandlerOnUIThread() {
  thread_.reset();
}

void DevToolsAdbBridge::QueryOnHandlerThread(
    const std::string query,
    const Callback& callback) {
  AdbClientSocket::AdbQuery(kAdbPort, query,
      base::Bind(&DevToolsAdbBridge::QueryResponseOnHandlerThread,
                 base::Unretained(this), callback));
}

void DevToolsAdbBridge::QueryResponseOnHandlerThread(
    const Callback& callback,
    int result,
    const std::string& response) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DevToolsAdbBridge::RespondOnUIThread, base::Unretained(this),
          callback, result, response));
}

void DevToolsAdbBridge::DevicesOnHandlerThread(
    const HostsCallback& callback) {
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&DevToolsAdbBridge::ReceivedDevices,
                 base::Unretained(this), callback));
}

void DevToolsAdbBridge::ReceivedDevices(
    const HostsCallback& callback,
    int result,
    const std::string& response) {
  AgentHosts* hosts = new AgentHosts();
  if (result != net::OK) {
    callback.Run(result, hosts);
    return;
  }

  std::vector<std::string> devices;
  Tokenize(response, "\n", &devices);
  std::vector<std::string>* serials = new std::vector<std::string>();
  for (size_t i = 0; i < devices.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(devices[i], "\t ", &tokens);
    std::string serial = tokens[0];
    serials->push_back(serial);
  }

  ProcessSerials(callback, hosts, serials);
}

void DevToolsAdbBridge::ProcessSerials(
    const HostsCallback& callback,
    AgentHosts* hosts,
    std::vector<std::string>* serials) {
  if (serials->size() == 0) {
    delete serials;
    callback.Run(net::OK, hosts);
    return;
  }

  AdbClientSocket::AdbQuery(
    kAdbPort,
    base::StringPrintf(kDeviceModelCommand, serials->back().c_str()),
    base::Bind(&DevToolsAdbBridge::ReceivedModel, base::Unretained(this),
               callback, hosts, serials));
}

void DevToolsAdbBridge::ReceivedModel(const HostsCallback& callback,
                                      AgentHosts* hosts,
                                      std::vector<std::string>* serials,
                                      int result,
                                      const std::string& response) {
  if (result != net::OK) {
    serials->pop_back();
    ProcessSerials(callback, hosts, serials);
    return;
  }

  AdbClientSocket::HttpQuery(
    kAdbPort, serials->back(), kDevToolsChannelName, kPageListQuery,
    base::Bind(&DevToolsAdbBridge::ReceivedPages, base::Unretained(this),
               callback, hosts, serials, response));

}

void DevToolsAdbBridge::ReceivedPages(const HostsCallback& callback,
                                      AgentHosts* hosts,
                                      std::vector<std::string>* serials,
                                      const std::string& model,
                                      int result,
                                      const std::string& response) {
  std::string serial = serials->back();
  serials->pop_back();
  if (result != net::OK) {
    ProcessSerials(callback, hosts, serials);
    return;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::ListValue* list_value;
  if (!value || !value->GetAsList(&list_value)) {
    ProcessSerials(callback, hosts, serials);
    return;
  }

  base::Value* item;
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    list_value->Get(i, &item);
    base::DictionaryValue* dict;
    if (!item || !item->GetAsDictionary(&dict))
      continue;
    scoped_refptr<AgentHost> host = new AgentHost(serial, model, *dict);
    hosts->push_back(host);
  }
  ProcessSerials(callback, hosts, serials);
}

void DevToolsAdbBridge::RespondOnUIThread(const Callback& callback,
                                          int result,
                                          const std::string& response) {
  callback.Run(result, response);
}

void DevToolsAdbBridge::PrintHosts(int result, AgentHosts* hosts) {
  for (AgentHosts::iterator it = hosts->begin(); it != hosts->end(); ++it) {
    AgentHost* host = it->get();
    fprintf(stderr, "HOST %s %s %s %s %s %s %s\n", host->serial().c_str(),
        host->model().c_str(), host->id().c_str(), host->title().c_str(),
        host->description().c_str(), host->favicon_url().c_str(),
        host->debug_url().c_str());
  }
}
