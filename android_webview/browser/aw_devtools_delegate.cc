// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_devtools_delegate.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/unix_domain_socket_posix.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
const char kSocketNameFormat[] = "webview_devtools_remote_%d";
}

namespace android_webview {

AwDevToolsDelegate::AwDevToolsDelegate(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  devtools_http_handler_ = content::DevToolsHttpHandler::Start(
      new net::UnixDomainSocketWithAbstractNamespaceFactory(
          StringPrintf(kSocketNameFormat, getpid()),
          base::Bind(&content::CanUserConnectToDevTools)),
      "",
      this);
}

AwDevToolsDelegate::~AwDevToolsDelegate() {
}

void AwDevToolsDelegate::Stop() {
  // The HTTP handler will delete our instance.
  devtools_http_handler_->Stop();
}

std::string AwDevToolsDelegate::GetDiscoveryPageHTML() {
  // This is a temporary way of providing the list of inspectable WebViews.
  // Since WebView doesn't have its own resources now, it doesn't seem
  // reasonable to create a dedicated .pak file just for this temporary page.
  const char html[] =
      "<html>"
      "<head>"
      "<title>WebView remote debugging</title>"
      "<style>"
      "</style>"
      "<script>"
      "function onLoad() {"
      "  var tabs_list_request = new XMLHttpRequest();"
      "  tabs_list_request.open("
      "\"GET\", \"/json/list?t=\" + new Date().getTime(), true);"
      "  tabs_list_request.onreadystatechange = onReady;"
      "  tabs_list_request.send();"
      "}"
      "function onReady() {"
      "  if(this.readyState == 4 && this.status == 200) {"
      "    if(this.response != null)"
      "      var responseJSON = JSON.parse(this.response);"
      "      for (var i = 0; i < responseJSON.length; ++i)"
      "        appendItem(responseJSON[i]);"
      "  }"
      "}"
      "function appendItem(item_object) {"
      "  var frontend_ref;"
      "  if (item_object.devtoolsFrontendUrl) {"
      "    frontend_ref = document.createElement(\"a\");"
      "    frontend_ref.href = item_object.devtoolsFrontendUrl;"
      "    frontend_ref.title = item_object.title;"
      "  } else {"
      "    frontend_ref = document.createElement(\"div\");"
      "    frontend_ref.title = "
      "\"The view already has active debugging session\";"
      "  }"
      "  var text = document.createElement(\"div\");"
      "  if (item_object.title)"
      "    text.innerText = item_object.title;"
      "  else"
      "    text.innerText = \"(untitled tab)\";"
      "  text.style.cssText = "
      "\"background-image:url(\" + item_object.faviconUrl + \")\";"
      "  frontend_ref.appendChild(text);"
      "  var item = document.createElement(\"p\");"
      "  item.appendChild(frontend_ref);"
      "  document.getElementById(\"items\").appendChild(item);"
      "}"
      "</script>"
      "</head>"
      "<body onload='onLoad()'>"
      "  <div id='caption'>Inspectable WebViews</div>"
      "  <div id='items'></div>"
      "</body>"
      "</html>";
  return html;
}

bool AwDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

base::FilePath AwDevToolsDelegate::GetDebugFrontendDir() {
  return base::FilePath();
}

std::string AwDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return "";
}

content::RenderViewHost* AwDevToolsDelegate::CreateNewTarget() {
  return NULL;
}

content::DevToolsHttpHandlerDelegate::TargetType
AwDevToolsDelegate::GetTargetType(content::RenderViewHost*) {
  return kTargetTypeTab;
}

}  // namespace android_webview
