// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_message_filter.h"

#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if !defined(OS_MACOSX)
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

namespace content {

namespace {

void CreateChildFrameOnUI(int process_id,
                          int parent_routing_id,
                          blink::WebTreeScopeType scope,
                          const std::string& frame_name,
                          blink::WebSandboxFlags sandbox_flags,
                          int new_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, parent_routing_id);
  // Handles the RenderFrameHost being deleted on the UI thread while
  // processing a subframe creation message.
  if (render_frame_host) {
    render_frame_host->OnCreateChildFrame(new_routing_id, scope, frame_name,
                                          sandbox_flags);
  }
}

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderFrameMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
    if (reply_msg_) {
      // If the owner of this class failed to call SendReplyAndDeleteThis(),
      // send an error reply to prevent the renderer from being hung.
      reply_msg_->set_reply_error();
      filter_->Send(reply_msg_);
    }
  }

  RenderFrameMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    reply_msg_ = nullptr;
    delete this;
  }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

}  // namespace

#if defined(ENABLE_PLUGINS)

class RenderFrameMessageFilter::OpenChannelToNpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PluginProcessHost::Client {
 public:
  OpenChannelToNpapiPluginCallback(RenderFrameMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context),
        host_(nullptr),
        sent_plugin_channel_request_(false) {
  }

  int ID() override { return filter()->render_process_id_; }

  ResourceContext* GetResourceContext() override { return context_; }

  bool OffTheRecord() override {
    if (filter()->incognito_)
      return true;
    if (GetContentClient()->browser()->AllowSaveLocalState(context_))
      return false;

    // For now, only disallow storing data for Flash <http://crbug.com/97319>.
    for (const auto& type : info_.mime_types) {
      if (type.mime_type == kFlashPluginSwfMimeType)
        return true;
    }
    return false;
  }

  void SetPluginInfo(const WebPluginInfo& info) override { info_ = info; }

  void OnFoundPluginProcessHost(PluginProcessHost* host) override {
    DCHECK(host);
    host_ = host;
  }

  void OnSentPluginChannelRequest() override {
    sent_plugin_channel_request_ = true;
  }

  void OnChannelOpened(const IPC::ChannelHandle& handle) override {
    WriteReplyAndDeleteThis(handle);
  }

  void OnError() override { WriteReplyAndDeleteThis(IPC::ChannelHandle()); }

  PluginProcessHost* host() const {
    return host_;
  }

  bool sent_plugin_channel_request() const {
    return sent_plugin_channel_request_;
  }

  void Cancel() {
    delete this;
  }

 private:
  void WriteReplyAndDeleteThis(const IPC::ChannelHandle& handle) {
    FrameHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg(),
                                                       handle, info_);
    filter()->OnCompletedOpenChannelToNpapiPlugin(this);
    SendReplyAndDeleteThis();
  }

  ResourceContext* context_;
  WebPluginInfo info_;
  PluginProcessHost* host_;
  bool sent_plugin_channel_request_;
};

#endif  // ENABLE_PLUGINS

RenderFrameMessageFilter::RenderFrameMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(FrameMsgStart),
      plugin_service_(plugin_service),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      render_process_id_(render_process_id) {
}

RenderFrameMessageFilter::~RenderFrameMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(ENABLE_PLUGINS)
  DCHECK(plugin_host_clients_.empty());
#endif  // ENABLE_PLUGINS
}

void RenderFrameMessageFilter::OnChannelClosing() {
#if defined(ENABLE_PLUGINS)
  for (OpenChannelToNpapiPluginCallback* client : plugin_host_clients_) {
    if (client->host()) {
      if (client->sent_plugin_channel_request()) {
        client->host()->CancelSentRequest(client);
      } else {
        client->host()->CancelPendingRequest(client);
      }
    } else {
      plugin_service_->CancelOpenChannelToNpapiPlugin(client);
    }
    client->Cancel();
  }
  plugin_host_clients_.clear();
#endif  // ENABLE_PLUGINS
}

bool RenderFrameMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameMessageFilter, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CookiesEnabled, OnCookiesEnabled)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Are3DAPIsBlocked, OnAre3DAPIsBlocked)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidLose3DContext, OnDidLose3DContext)
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
#endif  // ENABLE_PLUGINS
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameMessageFilter::OnCreateChildFrame(
    int parent_routing_id,
    blink::WebTreeScopeType scope,
    const std::string& frame_name,
    blink::WebSandboxFlags sandbox_flags,
    int* new_routing_id) {
  *new_routing_id = render_widget_helper_->GetNextRoutingID();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateChildFrameOnUI, render_process_id_, parent_routing_id,
                 scope, frame_name, sandbox_flags, *new_routing_id));
}

void RenderFrameMessageFilter::OnSetCookie(int render_frame_id,
                                           const GURL& url,
                                           const GURL& first_party_for_cookies,
                                           const std::string& cookie) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RFMF_SET_COOKIE_BAD_ORIGIN);
    return;
  }

  net::CookieOptions options;
  if (GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie, resource_context_,
          render_process_id_, render_frame_id, &options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    // Pass a null callback since we don't care about when the 'set' completes.
    context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, options, net::CookieStore::SetCookiesCallback());
  }
}

void RenderFrameMessageFilter::OnGetCookies(int render_frame_id,
                                            const GURL& url,
                                            const GURL& first_party_for_cookies,
                                            IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RFMF_GET_COOKIES_BAD_ORIGIN);
    delete reply_msg;
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/99242
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderFrameMessageFilter::CheckPolicyForCookies, this,
                      render_frame_id, url, first_party_for_cookies,
                      reply_msg));
}

void RenderFrameMessageFilter::OnCookiesEnabled(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* cookies_enabled) {
  // TODO(ananta): If this render frame is associated with an automation
  // channel, aka ChromeFrame then we need to retrieve cookie settings from the
  // external host.
  *cookies_enabled = GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, render_frame_id);
}

void RenderFrameMessageFilter::CheckPolicyForCookies(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, render_frame_id)) {
    // Gets the cookies from cookie store if allowed.
    context->cookie_store()->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&RenderFrameMessageFilter::SendGetCookiesResponse,
                   this, reply_msg));
  } else {
    SendGetCookiesResponse(reply_msg, std::string());
  }
}

void RenderFrameMessageFilter::SendGetCookiesResponse(
    IPC::Message* reply_msg,
    const std::string& cookies) {
  FrameHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderFrameMessageFilter::OnAre3DAPIsBlocked(int render_frame_id,
                                                  const GURL& top_origin_url,
                                                  ThreeDAPIType requester,
                                                  bool* blocked) {
  *blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      top_origin_url, render_process_id_, render_frame_id, requester);
}

void RenderFrameMessageFilter::OnDidLose3DContext(
    const GURL& top_origin_url,
    ThreeDAPIType /* unused */,
    int arb_robustness_status_code) {
  GpuDataManagerImpl::DomainGuilt guilt;
  switch (arb_robustness_status_code) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
      break;
    default:
      // Ignore lost contexts known to be innocent.
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
      top_origin_url, guilt);
}

#if defined(ENABLE_PLUGINS)

void RenderFrameMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, resource_context_,
      url, page_url, mime_type, allow_wildcard,
      nullptr, info, actual_mime_type);
}

void RenderFrameMessageFilter::OnOpenChannelToPlugin(
    int render_frame_id,
    const GURL& url,
    const GURL& policy_url,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  OpenChannelToNpapiPluginCallback* client =
      new OpenChannelToNpapiPluginCallback(this, resource_context_, reply_msg);
  DCHECK(!ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.insert(client);
  plugin_service_->OpenChannelToNpapiPlugin(
      render_process_id_, render_frame_id,
      url, policy_url, mime_type, client);
}

void RenderFrameMessageFilter::OnCompletedOpenChannelToNpapiPlugin(
    OpenChannelToNpapiPluginCallback* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.erase(client);
}

#endif  // ENABLE_PLUGINS

net::URLRequestContext* RenderFrameMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context_);
  if (!context)
    context = request_context_->GetURLRequestContext();

  return context;
}

}  // namespace content
