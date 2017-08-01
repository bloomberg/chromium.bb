// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/renderer_page_track_decider.h"

#include <string>

#include "chrome/renderer/searchbox/search_bouncer.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentLoader.h"
#include "url/gurl.h"

namespace page_load_metrics {

RendererPageTrackDecider::RendererPageTrackDecider(
    const blink::WebDocument* document,
    const blink::WebDocumentLoader* document_loader)
    : document_(document), document_loader_(document_loader) {}

RendererPageTrackDecider::~RendererPageTrackDecider() {}

bool RendererPageTrackDecider::HasCommitted() {
  // RendererPageTrackDecider is only instantiated for committed pages.
  return true;
}

bool RendererPageTrackDecider::IsHttpOrHttpsUrl() {
  return static_cast<GURL>(document_->Url()).SchemeIsHTTPOrHTTPS();
}

bool RendererPageTrackDecider::IsNewTabPageUrl() {
  return SearchBouncer::GetInstance()->IsNewTabPage(document_->Url());
}

bool RendererPageTrackDecider::IsChromeErrorPage() {
  return document_loader_->HasUnreachableURL();
}

int RendererPageTrackDecider::GetHttpStatusCode() {
  return document_loader_->GetResponse().HttpStatusCode();
}

bool RendererPageTrackDecider::IsHtmlOrXhtmlPage() {
  // Ignore non-HTML documents (e.g. SVG). Note that images are treated by
  // Blink as HTML documents, so to exclude images, we must perform
  // additional mime type checking below. MHTML is tracked as HTML in blink.
  if (!document_->IsHTMLDocument() && !document_->IsXHTMLDocument())
    return false;

  // Ignore non-HTML mime types (e.g. images).
  blink::WebString mime_type = document_loader_->GetResponse().MimeType();
  return mime_type == "text/html" || mime_type == "application/xhtml+xml" ||
         mime_type == "multipart/related";
}

}  // namespace page_load_metrics
