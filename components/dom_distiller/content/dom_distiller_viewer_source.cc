// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/viewer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "net/url_request/url_request.h"

namespace dom_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source.
class DomDistillerViewerSource::RequestViewerHandle
    : public ViewRequestDelegate {
 public:
  explicit RequestViewerHandle(
      const content::URLDataSource::GotDataCallback& callback);
  virtual ~RequestViewerHandle();

  // ViewRequestDelegate implementation.
  virtual void OnArticleReady(
      const DistilledArticleProto* article_proto) OVERRIDE;

  virtual void OnArticleUpdated(
      ArticleDistillationUpdate article_update) OVERRIDE;

  void TakeViewerHandle(scoped_ptr<ViewerHandle> viewer_handle);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  scoped_ptr<ViewerHandle> viewer_handle_;

  // This holds the callback to where the data retrieved is sent back.
  content::URLDataSource::GotDataCallback callback_;
};

DomDistillerViewerSource::RequestViewerHandle::RequestViewerHandle(
    const content::URLDataSource::GotDataCallback& callback)
    : callback_(callback) {
}

DomDistillerViewerSource::RequestViewerHandle::~RequestViewerHandle() {
}

void DomDistillerViewerSource::RequestViewerHandle::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  std::string unsafe_page_html = viewer::GetUnsafeHtml(article_proto);
  callback_.Run(base::RefCountedString::TakeString(&unsafe_page_html));
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void DomDistillerViewerSource::RequestViewerHandle::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {
  // TODO(nyquist): Add support for displaying pages incrementally.
}

void DomDistillerViewerSource::RequestViewerHandle::TakeViewerHandle(
    scoped_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = viewer_handle.Pass();
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& scheme)
    : scheme_(scheme), dom_distiller_service_(dom_distiller_service) {
}

DomDistillerViewerSource::~DomDistillerViewerSource() {
}

std::string DomDistillerViewerSource::GetSource() const {
  return scheme_ + "://";
}

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

  if (kCssPath == path) {
    std::string css = viewer::GetCss();
    callback.Run(base::RefCountedString::TakeString(&css));
    return;
  }

  RequestViewerHandle* request_viewer_handle =
      new RequestViewerHandle(callback);
  scoped_ptr<ViewerHandle> viewer_handle = viewer::CreateViewRequest(
      dom_distiller_service_, path, request_viewer_handle);

  if (viewer_handle) {
    // The service returned a |ViewerHandle| and guarantees it will call
    // the |RequestViewerHandle|, so passing ownership to it, to ensure the
    // request is not cancelled. The |RequestViewerHandle| will delete itself
    // after receiving the callback.
    request_viewer_handle->TakeViewerHandle(viewer_handle.Pass());
  } else {
    // The service did not return a |ViewerHandle|, which means the
    // |RequestViewerHandle| will never be called, so clean up now.
    delete request_viewer_handle;

    std::string error_page_html = viewer::GetErrorPageHtml();
    callback.Run(base::RefCountedString::TakeString(&error_page_html));
  }
};

std::string DomDistillerViewerSource::GetMimeType(
    const std::string& path) const {
  if (path == kCssPath)
    return "text/css";
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(scheme_.c_str());
}

// TODO(nyquist): Start tracking requests using this method.
void DomDistillerViewerSource::WillServiceRequest(
    const net::URLRequest* request,
    std::string* path) const {
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyObjectSrc()
    const {
  return "object-src 'none'; style-src 'self';";
}

}  // namespace dom_distiller
