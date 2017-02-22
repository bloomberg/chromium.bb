// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/renderer_page_track_decider.h"

#include <string>

#include "chrome/renderer/searchbox/search_bouncer.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "url/gurl.h"

namespace page_load_metrics {

RendererPageTrackDecider::RendererPageTrackDecider(
    const blink::WebDocument* document,
    const blink::WebDataSource* data_source)
    : document_(document), data_source_(data_source) {}

RendererPageTrackDecider::~RendererPageTrackDecider() {}

bool RendererPageTrackDecider::HasCommitted() {
  // RendererPageTrackDecider is only instantiated for committed pages.
  return true;
}

bool RendererPageTrackDecider::IsHttpOrHttpsUrl() {
  return static_cast<GURL>(document_->url()).SchemeIsHTTPOrHTTPS();
}

bool RendererPageTrackDecider::IsNewTabPageUrl() {
  return SearchBouncer::GetInstance()->IsNewTabPage(document_->url());
}

bool RendererPageTrackDecider::IsChromeErrorPage() {
  return data_source_->hasUnreachableURL();
}

int RendererPageTrackDecider::GetHttpStatusCode() {
  return data_source_->response().httpStatusCode();
}

bool RendererPageTrackDecider::IsHtmlOrXhtmlPage() {
  // Ignore non-HTML documents (e.g. SVG). Note that images are treated by
  // Blink as HTML documents, so to exclude images, we must perform
  // additional mime type checking below. MHTML is tracked as HTML in blink.
  if (!document_->isHTMLDocument() && !document_->isXHTMLDocument())
    return false;

  // Ignore non-HTML mime types (e.g. images).
  blink::WebString mime_type = data_source_->response().mimeType();
  return mime_type == "text/html" || mime_type == "application/xhtml+xml" ||
         mime_type == "multipart/related";
}

}  // namespace page_load_metrics
