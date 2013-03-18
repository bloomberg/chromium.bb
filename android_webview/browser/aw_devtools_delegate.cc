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
          base::StringPrintf(kSocketNameFormat, getpid()),
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
      "      clear();"
      "      for (var i = 0; i < items.length; ++i)"
      "        displayView(items[i]);"
      "      filter();"
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
      "    frontend_ref.target = '_blank';"
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
      "  var bits = 0;"
      "  var attached = item.attached ? (bits |= 1, 'Y') : 'N';"
      "  var visible = item.visible ? (bits |= 2, 'Y') : 'N';"
      "  var empty = item.empty ? 'Y' : (bits |= 4, 'N');"
      "  row.setAttribute('class', bits);"
      "  row.appendChild(column);"
      "  addColumn(row, attached);"
      "  addColumn(row, visible);"
      "  addColumn(row, empty);"
      "  addColumn(row, item.screenX + ', ' + item.screenY);"
      "  addColumn(row,"
      "            !item.empty ? (item.width + '\u00d7' + item.height) : '');"
      "  document.getElementById('items').appendChild(row);"
      "}"
      "function filter() {"
      "  var show_attached = document.getElementById('show_attached').checked;"
      "  var show_visible = document.getElementById('show_visible').checked;"
      "  var show_nonempty = document.getElementById('show_nonempty').checked;"
      "  var items = document.getElementById('items').childNodes;"
      "  for (var i = 0; i < items.length; i++) {"
      "    if (items[i].nodeName == 'TR') {"
      "      var mask = parseInt(items[i].getAttribute('class'));"
      "      var show = true;"
      "      if (show_attached) show &= ((mask & 1) != 0);"
      "      if (show_visible) show &= ((mask & 2) != 0);"
      "      if (show_nonempty) show &= ((mask & 4) != 0);"
      "      if (show) {"
      "        items[i].style.display = 'table-row';"
      "      } else {"
      "        items[i].style.display = 'none';"
      "      }"
      "    }"
      "  }"
      "}"
      "var refreshInterval = 0;"
      "function toggleRefresh() {"
      "  clearInterval(refreshInterval);"
      "  var enabled = document.getElementById('refresh').checked;"
      "  if (enabled) {"
      "    var time = document.getElementById('refresh_time').value * 1000;"
      "    refreshInterval = setInterval(onLoad, time);"
      "  }"
      "}"
      "function clear() {"
      "  var items = document.getElementById('items');"
      "  for (var i = 0; i < items.childNodes.length; i++) {"
      "    items.removeChild(items.childNodes[i]);"
      "  }"
      "}"
      "</script>"
      "</head>"
      "<body onload='onLoad()'>"
      "  <div id='caption'>Inspectable WebViews</div>"
      "  <div><p>Only show:</p>"
      "    <form>"
      "      <input type='checkbox' id='show_attached' onclick='filter()'>"
      "      Attached<br/>"
      "      <input type='checkbox' id='show_visible' onclick='filter()'>"
      "      Visible<br/>"
      "      <input type='checkbox' id='show_nonempty' onclick='filter()'>"
      "      Non-empty<br/>"
      "      <input type='checkbox' id='refresh' onclick='toggleRefresh()'>"
      "      Auto refresh every "
      "      <input type='number' id='refresh_time' value='2' min='1' max='99'"
      "        onchange='toggleRefresh();' /> seconds<br/>"
      "    </form>"
      "  </div>"
      "  <table>"
      "    <tr><th style='width:200px; text-align:left;'>Title</th>"
      "      <th>Attached</th><th>Visible</th><th>Empty</th>"
      "      <th style='width:100px; text-align:left;'>Position</th>"
      "      <th style='width:100px; text-align:left;'>Size</th>"
      "    </tr>"
      "    <tbody id='items'></tbody>"
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
