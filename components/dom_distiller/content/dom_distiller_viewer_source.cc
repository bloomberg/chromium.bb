// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace dom_distiller {

DomDistillerViewerSource::DomDistillerViewerSource(const std::string& scheme)
    : scheme_(scheme) {}

DomDistillerViewerSource::~DomDistillerViewerSource() {}

std::string DomDistillerViewerSource::GetSource() const { return scheme_; }

void DomDistillerViewerSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);
  content::RenderViewHost* render_view_host =
      render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host);
  CHECK_EQ(0, render_view_host->GetEnabledBindings());

  std::string page_template = "Aloha!";
  callback.Run(base::RefCountedString::TakeString(&page_template));
};

std::string DomDistillerViewerSource::GetMimeType(const std::string& path)
    const {
  return "text/plain";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(scheme_.c_str());
}

}  // namespace dom_distiller
