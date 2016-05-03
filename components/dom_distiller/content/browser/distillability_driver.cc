// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/service_registry.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    dom_distiller::DistillabilityDriver);

namespace dom_distiller {

// Implementation of the Mojo DistillabilityService. This is called by the
// renderer to notify the browser that a page is distillable.
class DistillabilityServiceImpl : public DistillabilityService {
 public:
  DistillabilityServiceImpl(
      mojo::InterfaceRequest<DistillabilityService> request,
      base::WeakPtr<DistillabilityDriver> distillability_driver)
      : binding_(this, std::move(request)),
        distillability_driver_(distillability_driver) {}

  ~DistillabilityServiceImpl() override {}

  void NotifyIsDistillable(bool is_distillable, bool is_last_update) override {
    if (!distillability_driver_) return;
    distillability_driver_->OnDistillability(is_distillable, is_last_update);
  }

 private:
  mojo::StrongBinding<DistillabilityService> binding_;
  base::WeakPtr<DistillabilityDriver> distillability_driver_;
};

DistillabilityDriver::DistillabilityDriver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      weak_factory_(this) {
  SetupMojoService();
}

DistillabilityDriver::~DistillabilityDriver() {
  content::WebContentsObserver::Observe(nullptr);
}

void DistillabilityDriver::CreateDistillabilityService(
    mojo::InterfaceRequest<DistillabilityService> request) {
  new DistillabilityServiceImpl(std::move(request), weak_factory_.GetWeakPtr());
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

void DistillabilityDriver::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host, const GURL& validated_url,
    bool is_error_page, bool is_iframe_srcdoc) {
  SetupMojoService();
}

void DistillabilityDriver::SetupMojoService() {
  if (!web_contents()) return;
  web_contents()->GetMainFrame()->GetServiceRegistry()->AddService(
      base::Bind(&DistillabilityDriver::CreateDistillabilityService,
          weak_factory_.GetWeakPtr()));
}

}  // namespace dom_distiller
