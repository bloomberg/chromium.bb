// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestion_source.h"

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kLoaderHtmlPath[] = "/loader.html";
const char kLoaderJSPath[] = "/loader.js";
const char kResultHtmlPath[] = "/result.html";
const char kResultJSPath[] = "/result.js";

}  // namespace

SuggestionSource::SuggestionSource() {
}

SuggestionSource::~SuggestionSource() {
}

std::string SuggestionSource::GetSource() const {
  return chrome::kChromeSearchSuggestionHost;
}

void SuggestionSource::StartDataRequest(
    const std::string& path_and_query,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string path(GURL(chrome::kChromeSearchSuggestionUrl +
                        path_and_query).path());
  if (path == kLoaderHtmlPath) {
    SendResource(IDR_OMNIBOX_RESULT_LOADER_HTML, callback);
  } else if (path == kLoaderJSPath) {
    SendJSWithOrigin(IDR_OMNIBOX_RESULT_LOADER_JS, render_process_id,
                     render_view_id, callback);
  } else if (path == kResultHtmlPath) {
    SendResource(IDR_OMNIBOX_RESULT_HTML, callback);
  } else if (path == kResultJSPath) {
    SendJSWithOrigin(IDR_OMNIBOX_RESULT_JS, render_process_id, render_view_id,
                     callback);
  } else {
    callback.Run(NULL);
  }
}

void SuggestionSource::SendResource(
    int resource_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id));
  callback.Run(response);
}

void SuggestionSource::SendJSWithOrigin(
    int resource_id,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string origin;
  if (!GetOrigin(render_process_id, render_view_id, &origin)) {
    callback.Run(NULL);
    return;
  }

  std::string js_escaped_origin;
  base::JsonDoubleQuote(origin, false, &js_escaped_origin);
  base::StringPiece template_js =
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  std::string response(template_js.as_string());
  ReplaceFirstSubstringAfterOffset(&response, 0, "{{ORIGIN}}", origin);
  callback.Run(base::RefCountedString::TakeString(&response));
}

std::string SuggestionSource::GetMimeType(
    const std::string& path_and_query) const {
  std::string path(GURL(chrome::kChromeSearchSuggestionUrl +
                        path_and_query).path());
  if (path == kLoaderHtmlPath || path == kResultHtmlPath)
    return "text/html";
  if (path == kLoaderJSPath || path == kResultJSPath)
    return "application/javascript";
  return "";
}

bool SuggestionSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  const std::string& path = request->url().path();
  return InstantIOContext::ShouldServiceRequest(request) &&
      request->url().SchemeIs(chrome::kChromeSearchScheme) &&
      request->url().host() == chrome::kChromeSearchSuggestionHost &&
      (path == kLoaderHtmlPath || path == kLoaderJSPath ||
       path == kResultHtmlPath || path == kResultJSPath);
}

bool SuggestionSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool SuggestionSource::GetOrigin(
    int render_process_id,
    int render_view_id,
    std::string* origin) const {
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (rvh == NULL)
    return false;
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (contents == NULL)
    return false;
  *origin = contents->GetURL().GetOrigin().spec();
  // Origin should not include a trailing slash. That is part of the path.
  TrimString(*origin, "/", origin);
  return true;
}
