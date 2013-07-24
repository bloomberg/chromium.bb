// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/child/appcache_dispatcher.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_callback_adapters.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/request_extra_data.h"
#include "content/common/socket_stream_handle_data.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/password_form_conversion_utils.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/webplugin_impl.h"
#include "content/renderer/websharedworker_proxy.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFileSystemCallbacks.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "webkit/child/weburlresponse_extradata_impl.h"

using WebKit::WebDataSource;
using WebKit::WebDocument;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFrame;
using WebKit::WebNavigationPolicy;
using WebKit::WebPluginParams;
using WebKit::WebReferrerPolicy;
using WebKit::WebSearchableFormData;
using WebKit::WebSecurityOrigin;
using WebKit::WebStorageQuotaCallbacks;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebUserGestureIndicator;
using WebKit::WebVector;
using WebKit::WebView;

using appcache::WebApplicationCacheHostImpl;
using base::Time;
using base::TimeDelta;

using webkit_glue::WebURLResponseExtraDataImpl;

namespace content {

static RenderFrameImpl* (*g_create_render_frame_impl)(RenderViewImpl*, int32) =
    NULL;

// static
RenderFrameImpl* RenderFrameImpl::Create(
    RenderViewImpl* render_view,
    int32 routing_id) {
  DCHECK(routing_id != MSG_ROUTING_NONE);

  RenderFrameImpl* render_frame = NULL;
  if (g_create_render_frame_impl)
    render_frame = g_create_render_frame_impl(render_view, routing_id);
  else
    render_frame = new RenderFrameImpl(render_view, routing_id);

  return render_frame;
}

// static
void RenderFrameImpl::InstallCreateHook(
    RenderFrameImpl* (*create_render_frame_impl)(RenderViewImpl*, int32)) {
  CHECK(!g_create_render_frame_impl);
  g_create_render_frame_impl = create_render_frame_impl;
}

// RenderFrameImpl ----------------------------------------------------------
RenderFrameImpl::RenderFrameImpl(RenderViewImpl* render_view, int routing_id)
    : render_view_(render_view),
      routing_id_(routing_id) {
}

RenderFrameImpl::~RenderFrameImpl() {
}

int RenderFrameImpl::GetRoutingID() const {
  // TODO(nasko): Until we register RenderFrameHost in the browser process as
  // a listener, we must route all messages to the RenderViewHost, so use the
  // routing id of the RenderView for now.
  return render_view_->GetRoutingID();
}

bool RenderFrameImpl::Send(IPC::Message* message) {
  // TODO(nasko): Move away from using the RenderView's Send method once we
  // have enough infrastructure and state to make the right checks here.
  return render_view_->Send(message);
}

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& msg) {
  // Pass the message up to the RenderView, until we have enough
  // infrastructure to start processing messages in this object.
  return render_view_->OnMessageReceived(msg);
}

// WebKit::WebFrameClient implementation -------------------------------------

WebKit::WebPlugin* RenderFrameImpl::createPlugin(
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  WebKit::WebPlugin* plugin = NULL;
  if (GetContentClient()->renderer()->OverrideCreatePlugin(
          render_view_, frame, params, &plugin)) {
    return plugin;
  }

#if defined(ENABLE_PLUGINS)
  if (UTF16ToASCII(params.mimeType) == kBrowserPluginMimeType) {
    return render_view_->GetBrowserPluginManager()->CreateBrowserPlugin(
        render_view_, frame, params);
  }

  WebPluginInfo info;
  std::string mime_type;
  bool found = render_view_->GetPluginInfo(
      params.url, frame->top()->document().url(), params.mimeType.utf8(),
      &info, &mime_type);
  if (!found)
    return NULL;

  WebPluginParams params_to_use = params;
  params_to_use.mimeType = WebString::fromUTF8(mime_type);
  return render_view_->CreatePlugin(frame, info, params_to_use);
#else
  return NULL;
#endif  // defined(ENABLE_PLUGINS)
}

WebKit::WebSharedWorker* RenderFrameImpl::createSharedWorker(
    WebKit::WebFrame* frame,
    const WebKit::WebURL& url,
    const WebKit::WebString& name,
    unsigned long long document_id) {
  int route_id = MSG_ROUTING_NONE;
  bool exists = false;
  bool url_mismatch = false;
  ViewHostMsg_CreateWorker_Params params;
  params.url = url;
  params.name = name;
  params.document_id = document_id;
  params.render_view_route_id = GetRoutingID();
  params.route_id = MSG_ROUTING_NONE;
  params.script_resource_appcache_id = 0;
  Send(new ViewHostMsg_LookupSharedWorker(
      params, &exists, &route_id, &url_mismatch));
  if (url_mismatch) {
    return NULL;
  } else {
    return new WebSharedWorkerProxy(RenderThreadImpl::current(),
                                    document_id,
                                    exists,
                                    route_id,
                                    GetRoutingID());
  }
}

WebKit::WebMediaPlayer* RenderFrameImpl::createMediaPlayer(
    WebKit::WebFrame* frame,
    const WebKit::WebURL& url,
    WebKit::WebMediaPlayerClient* client) {
  // TODO(nasko): Moving the implementation here involves moving a few media
  // related client objects here or referencing them in the RenderView. Needs
  // more work to understand where the proper place for those objects is.
  return render_view_->createMediaPlayer(frame, url, client);
}

WebKit::WebApplicationCacheHost* RenderFrameImpl::createApplicationCacheHost(
    WebKit::WebFrame* frame,
    WebKit::WebApplicationCacheHostClient* client) {
  if (!frame || !frame->view())
    return NULL;
  return new RendererWebApplicationCacheHostImpl(
      RenderViewImpl::FromWebView(frame->view()), client,
      RenderThreadImpl::current()->appcache_dispatcher()->backend_proxy());
}

WebKit::WebCookieJar* RenderFrameImpl::cookieJar(WebKit::WebFrame* frame) {
  return render_view_->cookieJar(frame);
}

void RenderFrameImpl::didAccessInitialDocument(WebKit::WebFrame* frame) {
  render_view_->didAccessInitialDocument(frame);
}

void RenderFrameImpl::didCreateFrame(WebKit::WebFrame* parent,
                                     WebKit::WebFrame* child) {
  Send(new ViewHostMsg_FrameAttached(GetRoutingID(), parent->identifier(),
      child->identifier(), UTF16ToUTF8(child->assignedName())));
}

void RenderFrameImpl::didDisownOpener(WebKit::WebFrame* frame) {
  render_view_->didDisownOpener(frame);
}

void RenderFrameImpl::frameDetached(WebKit::WebFrame* frame) {
  int64 parent_frame_id = -1;
  if (frame->parent())
    parent_frame_id = frame->parent()->identifier();

  Send(new ViewHostMsg_FrameDetached(GetRoutingID(), parent_frame_id,
      frame->identifier()));

  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->frameDetached(frame);
}

void RenderFrameImpl::willClose(WebKit::WebFrame* frame) {
  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->willClose(frame);
}

void RenderFrameImpl::didChangeName(WebKit::WebFrame* frame,
                                    const WebKit::WebString& name) {
  if (!render_view_->renderer_preferences_.report_frame_name_changes)
    return;

  Send(new ViewHostMsg_UpdateFrameName(GetRoutingID(),
                                       frame->identifier(),
                                       !frame->parent(),
                                       UTF16ToUTF8(name)));
}

void RenderFrameImpl::loadURLExternally(WebKit::WebFrame* frame,
                                        const WebKit::WebURLRequest& request,
                                        WebKit::WebNavigationPolicy policy) {
  loadURLExternally(frame, request, policy, WebString());
}

void RenderFrameImpl::loadURLExternally(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationPolicy policy,
    const WebKit::WebString& suggested_name) {
  Referrer referrer(RenderViewImpl::GetReferrerFromRequest(frame, request));
  if (policy == WebKit::WebNavigationPolicyDownload) {
    Send(new ViewHostMsg_DownloadUrl(GetRoutingID(), request.url(), referrer,
                                     suggested_name));
  } else {
    render_view_->OpenURL(frame, request.url(), referrer, policy);
  }
}

WebKit::WebNavigationPolicy RenderFrameImpl::decidePolicyForNavigation(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebKit::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return render_view_->decidePolicyForNavigation(
      frame, request, type, default_policy, is_redirect);
}

WebKit::WebURLError RenderFrameImpl::cannotHandleRequestError(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request) {
  NOTREACHED();  // Since we said we can handle all requests.
  return WebURLError();
}

WebKit::WebURLError RenderFrameImpl::cancelledError(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

void RenderFrameImpl::unableToImplementPolicyWithError(
    WebKit::WebFrame* frame,
    const WebKit::WebURLError& error) {
  NOTREACHED();  // Since we said we can handle all requests.
}

void RenderFrameImpl::willSendSubmitEvent(WebKit::WebFrame* frame,
                                          const WebKit::WebFormElement& form) {
  // Some login forms have onSubmit handlers that put a hash of the password
  // into a hidden field and then clear the password. (Issue 28910.)
  // This method gets called before any of those handlers run, so save away
  // a copy of the password in case it gets lost.
  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  document_state->set_password_form_data(CreatePasswordForm(form));
}

void RenderFrameImpl::willSubmitForm(WebKit::WebFrame* frame,
                                     const WebKit::WebFormElement& form) {
  DocumentState* document_state =
      DocumentState::FromDataSource(frame->provisionalDataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (PageTransitionCoreTypeIs(navigation_state->transition_type(),
                               PAGE_TRANSITION_LINK)) {
    navigation_state->set_transition_type(PAGE_TRANSITION_FORM_SUBMIT);
  }

  // Save these to be processed when the ensuing navigation is committed.
  WebSearchableFormData web_searchable_form_data(form);
  internal_data->set_searchable_form_url(web_searchable_form_data.url());
  internal_data->set_searchable_form_encoding(
      web_searchable_form_data.encoding().utf8());
  scoped_ptr<PasswordForm> password_form_data =
      CreatePasswordForm(form);

  // In order to save the password that the user actually typed and not one
  // that may have gotten transformed by the site prior to submit, recover it
  // from the form contents already stored by |willSendSubmitEvent| into the
  // dataSource's NavigationState (as opposed to the provisionalDataSource's,
  // which is what we're storing into now.)
  if (password_form_data) {
    DocumentState* old_document_state =
        DocumentState::FromDataSource(frame->dataSource());
    if (old_document_state) {
      PasswordForm* old_form_data = old_document_state->password_form_data();
      if (old_form_data && old_form_data->action == password_form_data->action)
        password_form_data->password_value = old_form_data->password_value;
    }
  }

  document_state->set_password_form_data(password_form_data.Pass());

  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->willSubmitForm(frame, form);
}

void RenderFrameImpl::willPerformClientRedirect(WebKit::WebFrame* frame,
                                                const WebKit::WebURL& from,
                                                const WebKit::WebURL& to,
                                                double interval,
                                                double fire_time) {
  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->willPerformClientRedirect(frame, from, to, interval, fire_time);
}

void RenderFrameImpl::didCancelClientRedirect(WebKit::WebFrame* frame) {
  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->didCancelClientRedirect(frame);
}

void RenderFrameImpl::didCompleteClientRedirect(WebKit::WebFrame* frame,
                                                const WebKit::WebURL& from) {
  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->didCompleteClientRedirect(frame, from);
}

void RenderFrameImpl::didCreateDataSource(WebKit::WebFrame* frame,
                                          WebKit::WebDataSource* datasource) {
  // TODO(nasko): Move implementation here. Needed state:
  // * pending_navigation_params_
  // * webview
  // Needed methods:
  // * PopulateDocumentStateFromPending
  // * CreateNavigationStateFromPending
  render_view_->didCreateDataSource(frame, datasource);
}

void RenderFrameImpl::didStartProvisionalLoad(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed state:
  // * is_swapped_out_
  // * navigation_gesture_
  // * completed_client_redirect_src_
  render_view_->didStartProvisionalLoad(frame);
}

void RenderFrameImpl::didReceiveServerRedirectForProvisionalLoad(
    WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed state:
  // * page_id_
  render_view_->didReceiveServerRedirectForProvisionalLoad(frame);
}

void RenderFrameImpl::didFailProvisionalLoad(
    WebKit::WebFrame* frame,
    const WebKit::WebURLError& error) {
  // TODO(nasko): Move implementation here. Needed state:
  // * page_id_
  // * pending_navigation_params_
  // Needed methods
  // * MaybeLoadAlternateErrorPage
  // * LoadNavigationErrorPage
  render_view_->didFailProvisionalLoad(frame, error);
}

void RenderFrameImpl::didCommitProvisionalLoad(WebKit::WebFrame* frame,
                                               bool is_new_navigation) {
  // TODO(nasko): Move implementation here. Needed state:
  // * page_id_
  // * next_page_id_
  // * history_list_offset_
  // * history_list_length_
  // * history_page_ids_
  // Needed methods
  // * webview
  // * UpdateSessionHistory
  // * GetLoadingUrl
  render_view_->didCommitProvisionalLoad(frame, is_new_navigation);
}

void RenderFrameImpl::didClearWindowObject(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed state:
  // * enabled_bindings_
  // * dom_automation_controller_
  // * stats_collection_controller_
  render_view_->didClearWindowObject(frame);
}

void RenderFrameImpl::didCreateDocumentElement(WebKit::WebFrame* frame) {
  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame->document().url();
  if (url.is_valid() && url.spec() != kAboutBlankURL) {
    // TODO(nasko): Check if webview()->mainFrame() is the same as the
    // frame->tree()->top().
    if (frame == render_view_->webview()->mainFrame())
      Send(new ViewHostMsg_DocumentAvailableInMainFrame(GetRoutingID()));
  }

  // Call back to RenderViewImpl for observers to be notified.
  // TODO(nasko): Remove once we have RenderFrameObserver.
  render_view_->didCreateDocumentElement(frame);
}

void RenderFrameImpl::didReceiveTitle(WebKit::WebFrame* frame,
                                      const WebKit::WebString& title,
                                      WebKit::WebTextDirection direction) {
  // TODO(nasko): Investigate wheather implementation should move here.
  render_view_->didReceiveTitle(frame, title, direction);
}

void RenderFrameImpl::didChangeIcon(WebKit::WebFrame* frame,
                                    WebKit::WebIconURL::Type icon_type) {
  // TODO(nasko): Investigate wheather implementation should move here.
  render_view_->didChangeIcon(frame, icon_type);
}

void RenderFrameImpl::didFinishDocumentLoad(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. No state needed, just observers
  // notification in before updating encoding.
  render_view_->didFinishDocumentLoad(frame);
}

void RenderFrameImpl::didHandleOnloadEvents(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed state:
  // * page_id_
  render_view_->didHandleOnloadEvents(frame);
}

void RenderFrameImpl::didFailLoad(WebKit::WebFrame* frame,
                                  const WebKit::WebURLError& error) {
  // TODO(nasko): Move implementation here. No state needed.
  render_view_->didFailLoad(frame, error);
}

void RenderFrameImpl::didFinishLoad(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. No state needed, just observers
  // notification before sending message to the browser process.
  render_view_->didFinishLoad(frame);
}

void RenderFrameImpl::didNavigateWithinPage(WebKit::WebFrame* frame,
                                            bool is_new_navigation) {
  // TODO(nasko): Move implementation here. No state needed, just observers
  // notification before sending message to the browser process.
  render_view_->didNavigateWithinPage(frame, is_new_navigation);
}

void RenderFrameImpl::didUpdateCurrentHistoryItem(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed methods:
  // * StartNavStateSyncTimerIfNecessary
  render_view_->didUpdateCurrentHistoryItem(frame);
}

void RenderFrameImpl::willSendRequest(
    WebKit::WebFrame* frame,
    unsigned identifier,
    WebKit::WebURLRequest& request,
    const WebKit::WebURLResponse& redirect_response) {
  // The request my be empty during tests.
  if (request.url().isEmpty())
    return;

  WebFrame* top_frame = frame->top();
  if (!top_frame)
    top_frame = frame;
  WebDataSource* provisional_data_source = top_frame->provisionalDataSource();
  WebDataSource* top_data_source = top_frame->dataSource();
  WebDataSource* data_source =
      provisional_data_source ? provisional_data_source : top_data_source;

  PageTransition transition_type = PAGE_TRANSITION_LINK;
  DocumentState* document_state = DocumentState::FromDataSource(data_source);
  DCHECK(document_state);
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);
  NavigationState* navigation_state = document_state->navigation_state();
  transition_type = navigation_state->transition_type();

  GURL request_url(request.url());
  GURL new_url;
  if (GetContentClient()->renderer()->WillSendRequest(
          frame,
          transition_type,
          request_url,
          request.firstPartyForCookies(),
          &new_url)) {
    request.setURL(WebURL(new_url));
  }

  if (internal_data->is_cache_policy_override_set())
    request.setCachePolicy(internal_data->cache_policy_override());

  WebKit::WebReferrerPolicy referrer_policy;
  if (internal_data->is_referrer_policy_set()) {
    referrer_policy = internal_data->referrer_policy();
    internal_data->clear_referrer_policy();
  } else {
    referrer_policy = frame->document().referrerPolicy();
  }

  // The request's extra data may indicate that we should set a custom user
  // agent. This needs to be done here, after WebKit is through with setting the
  // user agent on its own.
  WebString custom_user_agent;
  if (request.extraData()) {
    webkit_glue::WebURLRequestExtraDataImpl* old_extra_data =
        static_cast<webkit_glue::WebURLRequestExtraDataImpl*>(
            request.extraData());
    custom_user_agent = old_extra_data->custom_user_agent();

    if (!custom_user_agent.isNull()) {
      if (custom_user_agent.isEmpty())
        request.clearHTTPHeaderField("User-Agent");
      else
        request.setHTTPHeaderField("User-Agent", custom_user_agent);
    }
  }

  request.setExtraData(
      new RequestExtraData(referrer_policy,
                           custom_user_agent,
                           (frame == top_frame),
                           frame->identifier(),
                           frame->parent() == top_frame,
                           frame->parent() ? frame->parent()->identifier() : -1,
                           navigation_state->allow_download(),
                           transition_type,
                           navigation_state->transferred_request_child_id(),
                           navigation_state->transferred_request_request_id()));

  DocumentState* top_document_state =
      DocumentState::FromDataSource(top_data_source);
  // TODO(gavinp): separate out prefetching and prerender field trials
  // if the rel=prerender rel type is sticking around.
  if (top_document_state &&
      request.targetType() == WebURLRequest::TargetIsPrefetch)
    top_document_state->set_was_prefetcher(true);

  // This is an instance where we embed a copy of the routing id
  // into the data portion of the message. This can cause problems if we
  // don't register this id on the browser side, since the download manager
  // expects to find a RenderViewHost based off the id.
  request.setRequestorID(GetRoutingID());
  request.setHasUserGesture(WebUserGestureIndicator::isProcessingUserGesture());

  if (!navigation_state->extra_headers().empty()) {
    for (net::HttpUtil::HeadersIterator i(
        navigation_state->extra_headers().begin(),
        navigation_state->extra_headers().end(), "\n");
        i.GetNext(); ) {
      request.setHTTPHeaderField(WebString::fromUTF8(i.name()),
                                 WebString::fromUTF8(i.values()));
    }
  }

  if (!render_view_->renderer_preferences_.enable_referrers)
    request.clearHTTPHeaderField("Referer");
}

void RenderFrameImpl::didReceiveResponse(
    WebKit::WebFrame* frame,
    unsigned identifier,
    const WebKit::WebURLResponse& response) {
  // Only do this for responses that correspond to a provisional data source
  // of the top-most frame.  If we have a provisional data source, then we
  // can't have any sub-resources yet, so we know that this response must
  // correspond to a frame load.
  if (!frame->provisionalDataSource() || frame->parent())
    return;

  // If we are in view source mode, then just let the user see the source of
  // the server's error page.
  if (frame->isViewSourceModeEnabled())
    return;

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->provisionalDataSource());
  int http_status_code = response.httpStatusCode();

  // Record page load flags.
  WebURLResponseExtraDataImpl* extra_data =
      RenderViewImpl::GetExtraDataFromResponse(response);
  if (extra_data) {
    document_state->set_was_fetched_via_spdy(
        extra_data->was_fetched_via_spdy());
    document_state->set_was_npn_negotiated(
        extra_data->was_npn_negotiated());
    document_state->set_npn_negotiated_protocol(
        extra_data->npn_negotiated_protocol());
    document_state->set_was_alternate_protocol_available(
        extra_data->was_alternate_protocol_available());
    document_state->set_connection_info(
        extra_data->connection_info());
    document_state->set_was_fetched_via_proxy(
        extra_data->was_fetched_via_proxy());
  }
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);
  internal_data->set_http_status_code(http_status_code);
  // Whether or not the http status code actually corresponds to an error is
  // only checked when the page is done loading, if |use_error_page| is
  // still true.
  internal_data->set_use_error_page(true);
}

void RenderFrameImpl::didFinishResourceLoad(WebKit::WebFrame* frame,
                                            unsigned identifier) {
  // TODO(nasko): Move implementation here. Needed state:
  // * devtools_agent_
  // Needed methods:
  // * LoadNavigationErrorPage
  render_view_->didFinishResourceLoad(frame, identifier);
}

void RenderFrameImpl::didFailResourceLoad(WebKit::WebFrame* frame,
                                          unsigned identifier,
                                          const WebKit::WebURLError& error) {
  // Ignore
}

void RenderFrameImpl::didLoadResourceFromMemoryCache(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    const WebKit::WebURLResponse& response) {
  // The recipients of this message have no use for data: URLs: they don't
  // affect the page's insecure content list and are not in the disk cache. To
  // prevent large (1M+) data: URLs from crashing in the IPC system, we simply
  // filter them out here.
  GURL url(request.url());
  if (url.SchemeIs("data"))
    return;

  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new ViewHostMsg_DidLoadResourceFromMemoryCache(
      GetRoutingID(),
      url,
      response.securityInfo(),
      request.httpMethod().utf8(),
      response.mimeType().utf8(),
      ResourceType::FromTargetType(request.targetType())));
}

void RenderFrameImpl::didDisplayInsecureContent(WebKit::WebFrame* frame) {
  Send(new ViewHostMsg_DidDisplayInsecureContent(GetRoutingID()));
}

void RenderFrameImpl::didRunInsecureContent(
    WebKit::WebFrame* frame,
    const WebKit::WebSecurityOrigin& origin,
    const WebKit::WebURL& target) {
  Send(new ViewHostMsg_DidRunInsecureContent(
      GetRoutingID(),
      origin.toString().utf8(),
      target));
}

void RenderFrameImpl::didExhaustMemoryAvailableForScript(
    WebKit::WebFrame* frame) {
  Send(new ViewHostMsg_JSOutOfMemory(GetRoutingID()));
}

void RenderFrameImpl::didCreateScriptContext(WebKit::WebFrame* frame,
                                             v8::Handle<v8::Context> context,
                                             int extension_group,
                                             int world_id) {
  GetContentClient()->renderer()->DidCreateScriptContext(
      frame, context, extension_group, world_id);
}

void RenderFrameImpl::willReleaseScriptContext(WebKit::WebFrame* frame,
                                               v8::Handle<v8::Context> context,
                                               int world_id) {
  GetContentClient()->renderer()->WillReleaseScriptContext(
      frame, context, world_id);
}

void RenderFrameImpl::didFirstVisuallyNonEmptyLayout(WebKit::WebFrame* frame) {
  render_view_->didFirstVisuallyNonEmptyLayout(frame);
}

void RenderFrameImpl::didChangeContentsSize(WebKit::WebFrame* frame,
                                            const WebKit::WebSize& size) {
  // TODO(nasko): Move implementation here. Needed state:
  // * cached_has_main_frame_horizontal_scrollbar_
  // * cached_has_main_frame_vertical_scrollbar_
  render_view_->didChangeContentsSize(frame, size);
}

void RenderFrameImpl::didChangeScrollOffset(WebKit::WebFrame* frame) {
  // TODO(nasko): Move implementation here. Needed methods:
  // * StartNavStateSyncTimerIfNecessary
  render_view_->didChangeScrollOffset(frame);
}

void RenderFrameImpl::willInsertBody(WebKit::WebFrame* frame) {
  if (!frame->parent())
    Send(new ViewHostMsg_WillInsertBody(GetRoutingID()));
}

void RenderFrameImpl::reportFindInPageMatchCount(int request_id,
                                                 int count,
                                                 bool final_update) {
  int active_match_ordinal = -1;  // -1 = don't update active match ordinal
  if (!count)
    active_match_ordinal = 0;

  Send(new ViewHostMsg_Find_Reply(GetRoutingID(),
                                  request_id,
                                  count,
                                  gfx::Rect(),
                                  active_match_ordinal,
                                  final_update));
}

void RenderFrameImpl::reportFindInPageSelection(
    int request_id,
    int active_match_ordinal,
    const WebKit::WebRect& selection_rect) {
  Send(new ViewHostMsg_Find_Reply(GetRoutingID(),
                                  request_id,
                                  -1,
                                  selection_rect,
                                  active_match_ordinal,
                                  false));
}

void RenderFrameImpl::openFileSystem(
    WebKit::WebFrame* frame,
    WebKit::WebFileSystemType type,
    long long size,
    bool create,
    WebKit::WebFileSystemCallbacks* callbacks) {
  DCHECK(callbacks);

  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique()) {
    // Unique origins cannot store persistent state.
    callbacks->didFail(WebKit::WebFileErrorAbort);
    return;
  }

  ChildThread::current()->file_system_dispatcher()->OpenFileSystem(
      GURL(origin.toString()), static_cast<fileapi::FileSystemType>(type),
      size, create,
      base::Bind(&OpenFileSystemCallbackAdapter, callbacks),
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void RenderFrameImpl::deleteFileSystem(
    WebKit::WebFrame* frame,
    WebKit::WebFileSystemType type,
    WebKit::WebFileSystemCallbacks* callbacks) {
  DCHECK(callbacks);

  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique()) {
    // Unique origins cannot store persistent state.
    callbacks->didSucceed();
    return;
  }

  ChildThread::current()->file_system_dispatcher()->DeleteFileSystem(
      GURL(origin.toString()),
      static_cast<fileapi::FileSystemType>(type),
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void RenderFrameImpl::queryStorageUsageAndQuota(
    WebKit::WebFrame* frame,
    WebKit::WebStorageQuotaType type,
    WebKit::WebStorageQuotaCallbacks* callbacks) {
  DCHECK(frame);
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique()) {
    // Unique origins cannot store persistent state.
    callbacks->didFail(WebKit::WebStorageQuotaErrorAbort);
    return;
  }
  ChildThread::current()->quota_dispatcher()->QueryStorageUsageAndQuota(
      GURL(origin.toString()),
      static_cast<quota::StorageType>(type),
      QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

void RenderFrameImpl::requestStorageQuota(
    WebKit::WebFrame* frame,
    WebKit::WebStorageQuotaType type,
    unsigned long long requested_size,
    WebKit::WebStorageQuotaCallbacks* callbacks) {
  DCHECK(frame);
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique()) {
    // Unique origins cannot store persistent state.
    callbacks->didFail(WebKit::WebStorageQuotaErrorAbort);
    return;
  }
  ChildThread::current()->quota_dispatcher()->RequestStorageQuota(
      GetRoutingID(), GURL(origin.toString()),
      static_cast<quota::StorageType>(type), requested_size,
      QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

void RenderFrameImpl::willOpenSocketStream(
    WebKit::WebSocketStreamHandle* handle) {
  SocketStreamHandleData::AddToHandle(handle, GetRoutingID());
}

void RenderFrameImpl::willStartUsingPeerConnectionHandler(
    WebKit::WebFrame* frame,
    WebKit::WebRTCPeerConnectionHandler* handler) {
#if defined(ENABLE_WEBRTC)
  static_cast<RTCPeerConnectionHandler*>(handler)->associateWithFrame(frame);
#endif
}

bool RenderFrameImpl::willCheckAndDispatchMessageEvent(
    WebKit::WebFrame* sourceFrame,
    WebKit::WebFrame* targetFrame,
    WebKit::WebSecurityOrigin targetOrigin,
    WebKit::WebDOMMessageEvent event) {
  // TODO(nasko): Move implementation here. Needed state:
  // * is_swapped_out_
  return render_view_->willCheckAndDispatchMessageEvent(
      sourceFrame, targetFrame, targetOrigin, event);
}

WebKit::WebString RenderFrameImpl::userAgentOverride(
    WebKit::WebFrame* frame,
    const WebKit::WebURL& url) {
  if (!render_view_->webview() || !render_view_->webview()->mainFrame() ||
      render_view_->renderer_preferences_.user_agent_override.empty()) {
    return WebKit::WebString();
  }

  // If we're in the middle of committing a load, the data source we need
  // will still be provisional.
  WebFrame* main_frame = render_view_->webview()->mainFrame();
  WebDataSource* data_source = NULL;
  if (main_frame->provisionalDataSource())
    data_source = main_frame->provisionalDataSource();
  else
    data_source = main_frame->dataSource();

  InternalDocumentStateData* internal_data = data_source ?
      InternalDocumentStateData::FromDataSource(data_source) : NULL;
  if (internal_data && internal_data->is_overriding_user_agent())
    return WebString::fromUTF8(
        render_view_->renderer_preferences_.user_agent_override);
  return WebKit::WebString();
}

WebKit::WebString RenderFrameImpl::doNotTrackValue(WebKit::WebFrame* frame) {
  if (render_view_->renderer_preferences_.enable_do_not_track)
    return WebString::fromUTF8("1");
  return WebString();
}

bool RenderFrameImpl::allowWebGL(WebKit::WebFrame* frame, bool default_value) {
  if (!default_value)
    return false;

  bool blocked = true;
  Send(new ViewHostMsg_Are3DAPIsBlocked(
      GetRoutingID(),
      GURL(frame->top()->document().securityOrigin().toString()),
      THREE_D_API_TYPE_WEBGL,
      &blocked));
  return !blocked;
}

void RenderFrameImpl::didLoseWebGLContext(WebKit::WebFrame* frame,
                                          int arb_robustness_status_code) {
  Send(new ViewHostMsg_DidLose3DContext(
      GURL(frame->top()->document().securityOrigin().toString()),
      THREE_D_API_TYPE_WEBGL,
      arb_robustness_status_code));
}

}  // namespace content
