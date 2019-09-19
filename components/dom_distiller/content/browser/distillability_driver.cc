// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {

// Implementation of the Mojo DistillabilityService. This is called by the
// renderer to notify the browser that a page is distillable.
class DistillabilityServiceImpl : public mojom::DistillabilityService {
 public:
  explicit DistillabilityServiceImpl(
      base::WeakPtr<DistillabilityDriver> distillability_driver)
      : distillability_driver_(distillability_driver) {}

  ~DistillabilityServiceImpl() override {}

  void NotifyIsDistillable(bool is_distillable,
                           bool is_last_update,
                           bool is_mobile_friendly) override {
    if (!distillability_driver_)
      return;
    DistillabilityResult result;
    result.is_distillable = is_distillable;
    result.is_last = is_last_update;
    result.is_mobile_friendly = is_mobile_friendly;
    DVLOG(1) << "Notifying observers of distillability service result: "
             << result;
    distillability_driver_->OnDistillability(result);
  }

 private:
  base::WeakPtr<DistillabilityDriver> distillability_driver_;
};

DistillabilityDriver::DistillabilityDriver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      latest_result_(base::nullopt) {
  if (!web_contents)
    return;
  frame_interfaces_.AddInterface(
      base::BindRepeating(&DistillabilityDriver::CreateDistillabilityService,
                          base::Unretained(this)));
}

DistillabilityDriver::~DistillabilityDriver() {
  content::WebContentsObserver::Observe(nullptr);
}

void DistillabilityDriver::CreateDistillabilityService(
    mojo::PendingReceiver<mojom::DistillabilityService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DistillabilityServiceImpl>(weak_factory_.GetWeakPtr()),
      std::move(receiver));
}

void DistillabilityDriver::AddObserver(DistillabilityObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void DistillabilityDriver::OnDistillability(
    const DistillabilityResult& result) {
  latest_result_ = result;
  for (auto& observer : observers_)
    observer.OnResult(result);
}

void DistillabilityDriver::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  frame_interfaces_.TryBindInterface(interface_name, interface_pipe);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DistillabilityDriver)

}  // namespace dom_distiller
