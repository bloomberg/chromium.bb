// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    dom_distiller::DistillabilityDriver);

namespace dom_distiller {

// Implementation of the Mojo DistillabilityService. This is called by the
// renderer to notify the browser that a page is distillable.
class DistillabilityServiceImpl : public mojom::DistillabilityService {
 public:
  explicit DistillabilityServiceImpl(
      base::WeakPtr<DistillabilityDriver> distillability_driver)
      : distillability_driver_(distillability_driver) {}

  ~DistillabilityServiceImpl() override {
    if (!distillability_driver_) return;
    distillability_driver_->SetNeedsMojoSetup();
  }

  void NotifyIsDistillable(bool is_distillable, bool is_last_update) override {
    if (!distillability_driver_) return;
    distillability_driver_->OnDistillability(is_distillable, is_last_update);
  }

 private:
  base::WeakPtr<DistillabilityDriver> distillability_driver_;
};

DistillabilityDriver::DistillabilityDriver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      mojo_needs_setup_(true),
      weak_factory_(this) {
  if (!web_contents) return;
  SetupMojoService(web_contents->GetMainFrame());
}

DistillabilityDriver::~DistillabilityDriver() {
  content::WebContentsObserver::Observe(nullptr);
}

void DistillabilityDriver::CreateDistillabilityService(
    mojom::DistillabilityServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<DistillabilityServiceImpl>(weak_factory_.GetWeakPtr()),
      std::move(request));
}

void DistillabilityDriver::SetDelegate(
    const base::Callback<void(bool, bool)>& delegate) {
  m_delegate_ = delegate;
}

void DistillabilityDriver::OnDistillability(
    bool distillable, bool is_last) {
  if (m_delegate_.is_null()) return;

  m_delegate_.Run(distillable, is_last);
}

void DistillabilityDriver::SetNeedsMojoSetup() {
  mojo_needs_setup_ = true;
}

void DistillabilityDriver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // This method is invoked if any of the active RenderFrameHosts are swapped.
  // Only add the mojo service to the main frame host.
  if (!web_contents() || web_contents()->GetMainFrame() != new_host) return;

  // If the RenderFrameHost changes (this will happen if the user navigates to
  // or from a native page), the service needs to be attached to that host.
  mojo_needs_setup_ = true;
  SetupMojoService(new_host);
}

void DistillabilityDriver::ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsSameDocument())
    SetupMojoService(navigation_handle->GetRenderFrameHost());
}

void DistillabilityDriver::SetupMojoService(
    content::RenderFrameHost* frame_host) {
  if (!frame_host || !frame_host->GetInterfaceRegistry()
      || !mojo_needs_setup_) {
    return;
  }

  frame_host->GetInterfaceRegistry()->AddInterface(
      base::Bind(&DistillabilityDriver::CreateDistillabilityService,
          weak_factory_.GetWeakPtr()));
  mojo_needs_setup_ = false;
}

}  // namespace dom_distiller
