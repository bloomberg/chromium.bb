// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/webstore_bindings.h"

#include "base/strings/string_util.h"
#include "chrome/common/extensions/api/webstore/webstore_api_constants.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/crx_file/id_util.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebUserGestureIndicator;

namespace extensions {

namespace {

const char kWebstoreLinkRelation[] = "chrome-webstore-item";

const char kNotInTopFrameError[] =
    "Chrome Web Store installations can only be started by the top frame.";
const char kNotUserGestureError[] =
    "Chrome Web Store installations can only be initated by a user gesture.";
const char kNoWebstoreItemLinkFoundError[] =
    "No Chrome Web Store item link found.";
const char kInvalidWebstoreItemUrlError[] =
    "Invalid Chrome Web Store item URL.";

// chrome.webstore.install() calls generate an install ID so that the install's
// callbacks may be fired when the browser notifies us of install completion
// (successful or not) via OnInlineWebstoreInstallResponse.
int g_next_install_id = 0;

} // anonymous namespace

WebstoreBindings::WebstoreBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context), ChromeV8ExtensionHandler(context) {
  RouteFunction("Install",
                base::Bind(&WebstoreBindings::Install, base::Unretained(this)));
}

void WebstoreBindings::Install(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = context()->GetRenderView();
  if (!render_view)
    return;

  // The first two arguments indicate whether or not there are install stage
  // or download progress listeners.
  int listener_mask = 0;
  CHECK(args[0]->IsBoolean());
  if (args[0]->BooleanValue())
    listener_mask |= api::webstore::INSTALL_STAGE_LISTENER;
  CHECK(args[1]->IsBoolean());
  if (args[1]->BooleanValue())
    listener_mask |= api::webstore::DOWNLOAD_PROGRESS_LISTENER;

  std::string preferred_store_link_url;
  if (!args[2]->IsUndefined()) {
    CHECK(args[2]->IsString());
    preferred_store_link_url = std::string(*v8::String::Utf8Value(args[2]));
  }

  std::string webstore_item_id;
  std::string error;
  WebFrame* frame = context()->web_frame();

  if (!GetWebstoreItemIdFromFrame(
      frame, preferred_store_link_url, &webstore_item_id, &error)) {
    args.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(args.GetIsolate(), error.c_str()));
    return;
  }

  int install_id = g_next_install_id++;

  Send(new ExtensionHostMsg_InlineWebstoreInstall(render_view->GetRoutingID(),
                                                  install_id,
                                                  GetRoutingID(),
                                                  webstore_item_id,
                                                  frame->document().url(),
                                                  listener_mask));

  args.GetReturnValue().Set(static_cast<int32_t>(install_id));
}

// static
bool WebstoreBindings::GetWebstoreItemIdFromFrame(
      WebFrame* frame, const std::string& preferred_store_link_url,
      std::string* webstore_item_id, std::string* error) {
  if (frame != frame->top()) {
    *error = kNotInTopFrameError;
    return false;
  }

  if (!WebUserGestureIndicator::isProcessingUserGesture()) {
    *error = kNotUserGestureError;
    return false;
  }

  WebDocument document = frame->document();
  if (document.isNull()) {
    *error = kNoWebstoreItemLinkFoundError;
    return false;
  }

  WebElement head = document.head();
  if (head.isNull()) {
    *error = kNoWebstoreItemLinkFoundError;
    return false;
  }

  GURL webstore_base_url =
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix());
  WebNodeList children = head.childNodes();
  for (unsigned i = 0; i < children.length(); ++i) {
    WebNode child = children.item(i);
    if (!child.isElementNode())
      continue;
    WebElement elem = child.to<WebElement>();

    if (!elem.hasHTMLTagName("link") || !elem.hasAttribute("rel") ||
        !elem.hasAttribute("href"))
      continue;

    std::string rel = elem.getAttribute("rel").utf8();
    if (!LowerCaseEqualsASCII(rel, kWebstoreLinkRelation))
      continue;

    std::string webstore_url_string(elem.getAttribute("href").utf8());

    if (!preferred_store_link_url.empty() &&
        preferred_store_link_url != webstore_url_string) {
      continue;
    }

    GURL webstore_url = GURL(webstore_url_string);
    if (!webstore_url.is_valid()) {
      *error = kInvalidWebstoreItemUrlError;
      return false;
    }

    if (webstore_url.scheme() != webstore_base_url.scheme() ||
        webstore_url.host() != webstore_base_url.host() ||
        !StartsWithASCII(
            webstore_url.path(), webstore_base_url.path(), true)) {
      *error = kInvalidWebstoreItemUrlError;
      return false;
    }

    std::string candidate_webstore_item_id = webstore_url.path().substr(
        webstore_base_url.path().length());
    if (!crx_file::id_util::IdIsValid(candidate_webstore_item_id)) {
      *error = kInvalidWebstoreItemUrlError;
      return false;
    }

    std::string reconstructed_webstore_item_url_string =
        extension_urls::GetWebstoreItemDetailURLPrefix() +
            candidate_webstore_item_id;
    if (reconstructed_webstore_item_url_string != webstore_url_string) {
      *error = kInvalidWebstoreItemUrlError;
      return false;
    }

    *webstore_item_id = candidate_webstore_item_id;
    return true;
  }

  *error = kNoWebstoreItemLinkFoundError;
  return false;
}

bool WebstoreBindings::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebstoreBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_InlineWebstoreInstallResponse,
                        OnInlineWebstoreInstallResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_InlineInstallStageChanged,
                        OnInlineInstallStageChanged)
    IPC_MESSAGE_HANDLER(ExtensionMsg_InlineInstallDownloadProgress,
                        OnInlineInstallDownloadProgress)
    IPC_MESSAGE_UNHANDLED(CHECK(false) << "Unhandled IPC message")
  IPC_END_MESSAGE_MAP()
  return true;
}

void WebstoreBindings::OnInlineWebstoreInstallResponse(
    int install_id,
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Value> argv[] = {
    v8::Integer::New(isolate, install_id),
    v8::Boolean::New(isolate, success),
    v8::String::NewFromUtf8(isolate, error.c_str()),
    v8::String::NewFromUtf8(
        isolate, api::webstore::kInstallResultCodes[static_cast<int>(result)])
  };
  context()->module_system()->CallModuleMethod(
      "webstore", "onInstallResponse", arraysize(argv), argv);
}

void WebstoreBindings::OnInlineInstallStageChanged(int stage) {
  const char* stage_string = NULL;
  api::webstore::InstallStage install_stage =
      static_cast<api::webstore::InstallStage>(stage);
  switch (install_stage) {
    case api::webstore::INSTALL_STAGE_DOWNLOADING:
      stage_string = api::webstore::kInstallStageDownloading;
      break;
    case api::webstore::INSTALL_STAGE_INSTALLING:
      stage_string = api::webstore::kInstallStageInstalling;
      break;
  }
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, stage_string)};
  context()->module_system()->CallModuleMethod(
      "webstore", "onInstallStageChanged", arraysize(argv), argv);
}

void WebstoreBindings::OnInlineInstallDownloadProgress(int percent_downloaded) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Value> argv[] = {
      v8::Number::New(isolate, percent_downloaded / 100.0)};
  context()->module_system()->CallModuleMethod(
      "webstore", "onDownloadProgress", arraysize(argv), argv);
}

}  // namespace extensions
