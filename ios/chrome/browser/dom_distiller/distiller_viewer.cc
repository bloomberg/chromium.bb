// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"

#include <string>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/viewer.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ui/gfx/geometry/size.h"

namespace dom_distiller {

namespace {

class IOSContentDataCallback : public DistillerDataCallback {
 public:
  IOSContentDataCallback(
      const GURL& url,
      const DistillerViewer::DistillationFinishedCallback& callback,
      DistillerViewer* distiller_viewer_handle);
  ~IOSContentDataCallback() override{};
  void RunCallback(std::string& data) override;

 private:
  // Extra param needed by the callback specified below.
  GURL url_;
  // The callback to be run.
  const DistillerViewer::DistillationFinishedCallback callback_;
  // A handle to the DistillerViewer object.
  DistillerViewer* distiller_viewer_handle_;
};

IOSContentDataCallback::IOSContentDataCallback(
    const GURL& url,
    const DistillerViewer::DistillationFinishedCallback& callback,
    DistillerViewer* distiller_viewer_handle)
    : url_(url),
      callback_(callback),
      distiller_viewer_handle_(distiller_viewer_handle) {
}

void IOSContentDataCallback::RunCallback(std::string& data) {
  std::string htmlAndScript(data);
  htmlAndScript += "<script>" +
                   distiller_viewer_handle_->GetJavaScriptBuffer() +
                   "</script>";

  callback_.Run(url_, htmlAndScript);
}

}  // namespace

DistillerViewer::DistillerViewer(ios::ChromeBrowserState* browser_state,
                                 const GURL& url,
                                 const DistillationFinishedCallback& callback)
    : DomDistillerRequestViewBase(
          scoped_ptr<DistillerDataCallback>(
              new IOSContentDataCallback(url, callback, this)).Pass(),
          new DistilledPagePrefs(browser_state->GetPrefs())) {
  DCHECK(browser_state);
  DCHECK(url.is_valid());
  dom_distiller::DomDistillerService* distillerService =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserState(
          browser_state);

  viewer_handle_ = distillerService->ViewUrl(
      this, distillerService->CreateDefaultDistillerPage(gfx::Size()), url);
}

DistillerViewer::~DistillerViewer() {
}

void DistillerViewer::SendJavaScript(const std::string& buffer) {
  js_buffer_ += buffer;
}

std::string DistillerViewer::GetJavaScriptBuffer() {
  return js_buffer_;
}

}  // namespace dom_distiller
