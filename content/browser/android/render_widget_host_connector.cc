// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/render_widget_host_connector.h"

#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// Observes RenderWidgetHostViewAndroid to keep the instance up to date.
class RenderWidgetHostConnector::Observer
    : public WebContentsObserver,
      public RenderWidgetHostViewAndroid::DestructionObserver {
 public:
  Observer(WebContents* web_contents, RenderWidgetHostConnector* connector);
  ~Observer() override;

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void WebContentsDestroyed() override;

  // RenderWidgetHostViewAndroid::DestructionObserver implementation.
  void RenderWidgetHostViewDestroyed(
      RenderWidgetHostViewAndroid* rwhva) override;

  void UpdateRenderWidgetHostView(RenderWidgetHostViewAndroid* new_rwhva);
  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;
  RenderWidgetHostViewAndroid* active_rwhva() const { return active_rwhva_; }

 private:
  RenderWidgetHostConnector* const connector_;

  // Active RenderWidgetHostView connected to this instance. Can also point to
  // an interstitial while it is showing.
  RenderWidgetHostViewAndroid* active_rwhva_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

RenderWidgetHostConnector::Observer::Observer(
    WebContents* web_contents,
    RenderWidgetHostConnector* connector)
    : WebContentsObserver(web_contents),
      connector_(connector),
      active_rwhva_(nullptr) {}

RenderWidgetHostConnector::Observer::~Observer() {
  DCHECK(!active_rwhva_);
}

void RenderWidgetHostConnector::Observer::RenderViewReady() {
  UpdateRenderWidgetHostView(GetRenderWidgetHostViewAndroid());
}

void RenderWidgetHostConnector::Observer::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  // |RenderViewHostChanged| is called only for main rwhva change.
  // No need to update connection if an interstitial page is active.
  if (web_contents()->ShowingInterstitialPage())
    return;

  auto* new_view = new_host ? static_cast<RenderWidgetHostViewAndroid*>(
                                  new_host->GetWidget()->GetView())
                            : nullptr;
  UpdateRenderWidgetHostView(new_view);
}

void RenderWidgetHostConnector::Observer::DidAttachInterstitialPage() {
  UpdateRenderWidgetHostView(GetRenderWidgetHostViewAndroid());
}

void RenderWidgetHostConnector::Observer::DidDetachInterstitialPage() {
  UpdateRenderWidgetHostView(GetRenderWidgetHostViewAndroid());
}

void RenderWidgetHostConnector::Observer::WebContentsDestroyed() {
  DCHECK_EQ(active_rwhva_, GetRenderWidgetHostViewAndroid());
  UpdateRenderWidgetHostView(nullptr);
  delete connector_;
}

void RenderWidgetHostConnector::Observer::RenderWidgetHostViewDestroyed(
    RenderWidgetHostViewAndroid* destroyed_rwhva) {
  // Null out the raw pointer here and in the connector impl to keep
  // them from referencing the rwvha about to be destroyed.
  if (destroyed_rwhva == active_rwhva_)
    UpdateRenderWidgetHostView(nullptr);
}

void RenderWidgetHostConnector::Observer::UpdateRenderWidgetHostView(
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (active_rwhva_ == new_rwhva)
    return;
  if (active_rwhva_)
    active_rwhva_->RemoveDestructionObserver(this);
  if (new_rwhva)
    new_rwhva->AddDestructionObserver(this);

  connector_->UpdateRenderProcessConnection(active_rwhva_, new_rwhva);
  active_rwhva_ = new_rwhva;
}

RenderWidgetHostViewAndroid*
RenderWidgetHostConnector::Observer::GetRenderWidgetHostViewAndroid() const {
  RenderWidgetHostView* rwhv = web_contents()->GetRenderWidgetHostView();
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  if (web_contents_impl->ShowingInterstitialPage()) {
    rwhv = web_contents_impl->GetInterstitialPage()
               ->GetMainFrame()
               ->GetRenderViewHost()
               ->GetWidget()
               ->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

RenderWidgetHostConnector::RenderWidgetHostConnector(WebContents* web_contents)
    : render_widget_observer_(new Observer(web_contents, this)) {}

void RenderWidgetHostConnector::Initialize() {
  render_widget_observer_->UpdateRenderWidgetHostView(
      render_widget_observer_->GetRenderWidgetHostViewAndroid());
}

RenderWidgetHostConnector::~RenderWidgetHostConnector() {}

RenderWidgetHostViewAndroid* RenderWidgetHostConnector::GetRWHVAForTesting()
    const {
  return render_widget_observer_->active_rwhva();
}

}  // namespace content
