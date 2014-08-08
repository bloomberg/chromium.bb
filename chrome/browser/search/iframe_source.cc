// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/iframe_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

IframeSource::IframeSource() {
}

IframeSource::~IframeSource() {
}

std::string IframeSource::GetMimeType(
    const std::string& path_and_query) const {
  std::string path(GURL("chrome-search://host/" + path_and_query).path());
  if (EndsWith(path, ".js", false))
    return "application/javascript";
  if (EndsWith(path, ".png", false))
    return "image/png";
  if (EndsWith(path, ".css", false))
    return "text/css";
  if (EndsWith(path, ".html", false))
    return "text/html";
  return "";
}

bool IframeSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  const std::string& path = request->url().path();
  return InstantIOContext::ShouldServiceRequest(request) &&
      request->url().SchemeIs(chrome::kChromeSearchScheme) &&
      request->url().host() == GetSource() &&
      ServesPath(path);
}

bool IframeSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool IframeSource::GetOrigin(
    int render_process_id,
    int render_frame_id,
    std::string* origin) const {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (contents == NULL)
    return false;
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (entry == NULL)
    return false;

  *origin = entry->GetURL().GetOrigin().spec();
  // Origin should not include a trailing slash. That is part of the path.
  base::TrimString(*origin, "/", origin);
  return true;
}

void IframeSource::SendResource(
    int resource_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id));
  callback.Run(response.get());
}

void IframeSource::SendJSWithOrigin(
    int resource_id,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string origin;
  if (!GetOrigin(render_process_id, render_frame_id, &origin)) {
    callback.Run(NULL);
    return;
  }

  base::StringPiece template_js =
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  std::string response(template_js.as_string());
  ReplaceFirstSubstringAfterOffset(&response, 0, "{{ORIGIN}}", origin);
  callback.Run(base::RefCountedString::TakeString(&response));
}
