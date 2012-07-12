// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/devtools_server.h"

#include <pwd.h>

#include <cstring>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stringprintf.h"
#include "content/public/browser/devtools_http_handler.h"
#include "net/base/unix_domain_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/static/%s/devtools.html";
const char kSocketName[] = "chrome_devtools_remote";

bool UserCanConnect(uid_t uid, gid_t gid) {
  struct passwd* creds = getpwuid(uid);
  if (!creds || !creds->pw_name) {
    LOG(WARNING) << "DevToolsServer: can't obtain creds for uid " << uid;
    return false;
  }
  if (gid == uid &&
      (strcmp("root", creds->pw_name) == 0 ||
       strcmp("shell", creds->pw_name) == 0)) {
    return true;
  }
  LOG(WARNING) << "DevToolsServer: connection attempt from " << creds->pw_name;
  return false;
}

class DevToolsServerImpl : public DevToolsServer {
 public:
  static DevToolsServerImpl* GetInstance() {
    return Singleton<DevToolsServerImpl>::get();
  }

  // DevToolsServer:
  virtual void Init(const std::string& frontend_version,
                    net::URLRequestContextGetter* url_request_context,
                    const DelegateCreator& delegate_creator) OVERRIDE {
    frontend_url_ = StringPrintf(kFrontEndURL, frontend_version.c_str());
    url_request_context_ = url_request_context;
    delegate_creator_ = delegate_creator;
  }

  virtual void Start() OVERRIDE {
    Stop();
    DCHECK(IsInitialized());
    protocol_handler_ = DevToolsHttpHandler::Start(
        new net::UnixDomainSocketWithAbstractNamespaceFactory(
            kSocketName, base::Bind(&UserCanConnect)),
        frontend_url_,
        url_request_context_,
        delegate_creator_.Run());
  }

  virtual void Stop() OVERRIDE {
    if (protocol_handler_) {
      // Note that the call to Stop() below takes care of |protocol_handler_|
      // deletion.
      protocol_handler_->Stop();
      protocol_handler_ = NULL;
    }
  }

  virtual bool IsInitialized() const OVERRIDE {
    return !frontend_url_.empty() && url_request_context_;
  }

  virtual bool IsStarted() const OVERRIDE {
    return protocol_handler_;
  }

 private:
  friend struct DefaultSingletonTraits<DevToolsServerImpl>;

  DevToolsServerImpl() : protocol_handler_(NULL), url_request_context_(NULL) {}

  virtual ~DevToolsServerImpl() {
    Stop();
  }

  DevToolsHttpHandler* protocol_handler_;
  std::string frontend_url_;
  net::URLRequestContextGetter* url_request_context_;
  DelegateCreator delegate_creator_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerImpl);
};

}  // namespace

// static
DevToolsServer* DevToolsServer::GetInstance() {
  return DevToolsServerImpl::GetInstance();
}

}  // namespace content
