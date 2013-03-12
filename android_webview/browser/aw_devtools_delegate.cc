// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_devtools_delegate.h"

#include "android_webview/browser/browser_view_renderer_impl.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/values.h"
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
  devtools_http_handler_->Stop();
  // WARNING: |this| has now been deleted by the method above.
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
      "      'GET', '/json/list?t=' + new Date().getTime(), true);"
      "  tabs_list_request.onreadystatechange = onReady;"
      "  tabs_list_request.send();"
      "}"
      "function viewsComparator(v1, v2) {"
      "  if (v1.attached != v2.attached) {"
      "    return v1.attached ? -1 : 1;"
      "  } else if (v1.visible != v2.visible) {"
      "    return v1.visible ? -1 : 1;"
      "  } else if (v1.empty != v2.empty) {"
      "    return v1.empty ? 1 : -1;"
      "  } else if (v1.screenX != v2.screenX) {"
      "    return v1.screenX - v2.screenX;"
      "  } else if (v1.screenY != v2.screenY) {"
      "    return v1.screenY - v2.screenY;"
      "  }"
      "  return 0;"
      "}"
      "function processItem(item) {"
      "  var result = JSON.parse(item.description);"
      "  result.debuggable = !!item.devtoolsFrontendUrl;"
      "  result.debugUrl = item.devtoolsFrontendUrl;"
      "  result.title = item.title;"
      "  return result;"
      "}"
      "function onReady() {"
      "  if(this.readyState == 4 && this.status == 200) {"
      "    if(this.response != null)"
      "      var responseJSON = JSON.parse(this.response);"
      "      var items = [];"
      "      for (var i = 0; i < responseJSON.length; ++i)"
      "        items.push(processItem(responseJSON[i]));"
      "      items.sort(viewsComparator);"
      "      for (var i = 0; i < items.length; ++i)"
      "        displayView(items[i]);"
      "  }"
      "}"
      "function addColumn(row, text) {"
      "  var column = document.createElement('td');"
      "  column.innerText = text;"
      "  row.appendChild(column);"
      "}"
      "function cutTextIfNeeded(text, maxLen) {"
      "  return text.length <= maxLen ?"
      "      text : text.substr(0, maxLen) + '\u2026';"
      "}"
      "function displayView(item) {"
      "  var row = document.createElement('tr');"
      "  var column = document.createElement('td');"
      "  var frontend_ref;"
      "  if (item.debuggable) {"
      "    frontend_ref = document.createElement('a');"
      "    frontend_ref.href = item.debugUrl;"
      "    frontend_ref.title = item.title;"
      "    column.appendChild(frontend_ref);"
      "  } else {"
      "    frontend_ref = column;"
      "  }"
      "  var text = document.createElement('span');"
      "  if (item.title) {"
      "    text.innerText = cutTextIfNeeded(item.title, 64);"
      "  } else {"
      "    text.innerText = '(untitled)';"
      "  }"
      "  frontend_ref.appendChild(text);"
      "  row.appendChild(column);"
      "  addColumn(row, item.attached ? 'Y' : 'N');"
      "  addColumn(row, item.visible ? 'Y' : 'N');"
      "  addColumn(row, item.empty ? 'Y' : 'N');"
      "  addColumn(row, item.screenX + ', ' + item.screenY);"
      "  addColumn(row,"
      "            !item.empty ? (item.width + '\u00d7' + item.height) : '');"
      "  document.getElementById('items').appendChild(row);"
      "}"
      "</script>"
      "</head>"
      "<body onload='onLoad()'>"
      "  <div id='caption'>Inspectable WebViews</div>"
      "  <table>"
      "    <tr><th>Title</th><th>Attached</th><th>Visible</th><th>Empty</th>"
      "<th>Position</th><th>Size</th></tr>"
      "  <tbody id='items'></tbody>"
      "  </table>"
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

std::string AwDevToolsDelegate::GetViewDescription(
    content::RenderViewHost* rvh) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents) return "";
  BrowserViewRenderer* bvr =
      BrowserViewRendererImpl::FromWebContents(web_contents);
  if (!bvr) return "";
  base::DictionaryValue description;
  description.SetBoolean("attached", bvr->IsAttachedToWindow());
  description.SetBoolean("visible", bvr->IsViewVisible());
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

}  // namespace android_webview
