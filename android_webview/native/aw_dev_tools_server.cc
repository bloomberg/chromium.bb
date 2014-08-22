// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_dev_tools_server.h"

#include "android_webview/native/aw_contents.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "jni/AwDevToolsServer_jni.h"
#include "net/socket/unix_domain_listen_socket_posix.h"

using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/devtools.html";
const char kSocketNameFormat[] = "webview_devtools_remote_%d";

const char kTargetTypePage[] = "page";

std::string GetViewDescription(WebContents* web_contents);

class Target : public content::DevToolsTarget {
 public:
  explicit Target(WebContents* web_contents);

  virtual std::string GetId() const OVERRIDE { return id_; }
  virtual std::string GetParentId() const OVERRIDE { return std::string(); }
  virtual std::string GetType() const OVERRIDE { return kTargetTypePage; }
  virtual std::string GetTitle() const OVERRIDE { return title_; }
  virtual std::string GetDescription() const OVERRIDE { return description_; }
  virtual GURL GetURL() const OVERRIDE { return url_; }
  virtual GURL GetFaviconURL() const OVERRIDE { return GURL(); }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE { return false; }
  virtual bool Close() const OVERRIDE { return false; }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string title_;
  std::string description_;
  GURL url_;
  base::TimeTicks last_activity_time_;
};

Target::Target(WebContents* web_contents) {
  agent_host_ =
      DevToolsAgentHost::GetOrCreateFor(web_contents);
  id_ = agent_host_->GetId();
  description_ = GetViewDescription(web_contents);
  title_ = base::UTF16ToUTF8(web_contents->GetTitle());
  url_ = web_contents->GetURL();
  last_activity_time_ = web_contents->GetLastActiveTime();
}

// Delegate implementation for the devtools http handler for WebView. A new
// instance of this gets created each time web debugging is enabled.
class AwDevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  AwDevToolsServerDelegate() {}
  virtual ~AwDevToolsServerDelegate() {}

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;

  virtual bool BundlesFrontendResources() OVERRIDE {
    return false;
  }

  virtual base::FilePath GetDebugFrontendDir() OVERRIDE {
    return base::FilePath();
  }

  virtual std::string GetPageThumbnailData(const GURL&) OVERRIDE {
    return "";
  }

  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL&) OVERRIDE {
    return scoped_ptr<content::DevToolsTarget>();
  }

  virtual void EnumerateTargets(TargetCallback callback) OVERRIDE {
    TargetList targets;
    std::vector<WebContents*> wc_list =
        DevToolsAgentHost::GetInspectableWebContents();
    for (std::vector<WebContents*>::iterator it = wc_list.begin();
         it != wc_list.end(); ++it) {
      targets.push_back(new Target(*it));
    }
    callback.Run(targets);
  }

  virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(
      net::StreamListenSocket::Delegate* delegate,
      std::string* name) OVERRIDE {
    return scoped_ptr<net::StreamListenSocket>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AwDevToolsServerDelegate);
};


std::string AwDevToolsServerDelegate::GetDiscoveryPageHTML() {
  const char html[] =
      "<html>"
      "<head><title>WebView remote debugging</title></head>"
      "<body>Please use <a href=\'chrome://inspect\'>chrome://inspect</a>"
      "</body>"
      "</html>";
  return html;
}

std::string GetViewDescription(WebContents* web_contents) {
  const android_webview::BrowserViewRenderer* bvr =
      android_webview::AwContents::FromWebContents(web_contents)
          ->GetBrowserViewRenderer();
  if (!bvr) return "";
  base::DictionaryValue description;
  description.SetBoolean("attached", bvr->attached_to_window());
  description.SetBoolean("visible", bvr->IsVisible());
  gfx::Rect screen_rect = bvr->GetScreenRect();
  description.SetInteger("screenX", screen_rect.x());
  description.SetInteger("screenY", screen_rect.y());
  description.SetBoolean("empty", screen_rect.size().IsEmpty());
  if (!screen_rect.size().IsEmpty()) {
    description.SetInteger("width", screen_rect.width());
    description.SetInteger("height", screen_rect.height());
  }
  std::string json;
  base::JSONWriter::Write(&description, &json);
  return json;
}

}  // namespace

namespace android_webview {

AwDevToolsServer::AwDevToolsServer()
    : protocol_handler_(NULL) {
}

AwDevToolsServer::~AwDevToolsServer() {
  Stop();
}

void AwDevToolsServer::Start() {
  if (protocol_handler_)
    return;

  protocol_handler_ = content::DevToolsHttpHandler::Start(
      new net::deprecated::UnixDomainListenSocketWithAbstractNamespaceFactory(
          base::StringPrintf(kSocketNameFormat, getpid()),
          "",
          base::Bind(&content::CanUserConnectToDevTools)),
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      new AwDevToolsServerDelegate(),
      base::FilePath());
}

void AwDevToolsServer::Stop() {
  if (!protocol_handler_)
    return;
  // Note that the call to Stop() below takes care of |protocol_handler_|
  // deletion.
  protocol_handler_->Stop();
  protocol_handler_ = NULL;
}

bool AwDevToolsServer::IsStarted() const {
  return protocol_handler_;
}

bool RegisterAwDevToolsServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong InitRemoteDebugging(JNIEnv* env,
                                jobject obj) {
  AwDevToolsServer* server = new AwDevToolsServer();
  return reinterpret_cast<intptr_t>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env, jobject obj, jlong server) {
  delete reinterpret_cast<AwDevToolsServer*>(server);
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      jobject obj,
                                      jlong server,
                                      jboolean enabled) {
  AwDevToolsServer* devtools_server =
      reinterpret_cast<AwDevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start();
  } else {
    devtools_server->Stop();
  }
}

}  // namespace android_webview
