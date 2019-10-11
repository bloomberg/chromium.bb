// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal_interceptor_for_testing.h"

#include <utility>
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"

namespace content {

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::AssociatedRemote<blink::mojom::PortalClient> client) {
  auto test_portal_ptr =
      base::WrapUnique(new PortalInterceptorForTesting(render_frame_host_impl));
  PortalInterceptorForTesting* test_portal = test_portal_ptr.get();
  test_portal->GetPortal()->SetBindingForTesting(
      mojo::MakeStrongAssociatedBinding<blink::mojom::Portal>(
          std::move(test_portal_ptr), std::move(receiver)));
  test_portal->GetPortal()->SetClientForTesting(std::move(client));
  return test_portal;
}

PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    content::Portal* portal) {
  // Take ownership of the portal.
  std::unique_ptr<blink::mojom::Portal> mojom_portal_ptr =
      portal->GetBindingForTesting()->SwapImplForTesting(nullptr);
  std::unique_ptr<content::Portal> portal_ptr = base::WrapUnique(
      static_cast<content::Portal*>(mojom_portal_ptr.release()));

  // Create PortalInterceptorForTesting.
  auto test_portal_ptr = base::WrapUnique(new PortalInterceptorForTesting(
      render_frame_host_impl, std::move(portal_ptr)));
  PortalInterceptorForTesting* test_portal = test_portal_ptr.get();

  // Set the binding for the PortalInterceptorForTesting.
  portal->GetBindingForTesting()->SwapImplForTesting(
      std::move(test_portal_ptr));

  return test_portal;
}

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::From(
    content::Portal* portal) {
  blink::mojom::Portal* impl = portal->GetBindingForTesting()->impl();
  auto* interceptor = static_cast<PortalInterceptorForTesting*>(impl);
  CHECK_NE(static_cast<blink::mojom::Portal*>(portal), impl);
  CHECK_EQ(interceptor->GetPortal(), portal);
  return interceptor;
}

PortalInterceptorForTesting::PortalInterceptorForTesting(
    RenderFrameHostImpl* render_frame_host_impl)
    : portal_(content::Portal::CreateForTesting(render_frame_host_impl)) {}

PortalInterceptorForTesting::PortalInterceptorForTesting(
    RenderFrameHostImpl* render_frame_host_impl,
    std::unique_ptr<content::Portal> portal)
    : portal_(std::move(portal)) {}

PortalInterceptorForTesting::~PortalInterceptorForTesting() = default;

blink::mojom::Portal* PortalInterceptorForTesting::GetForwardingInterface() {
  return portal_.get();
}

void PortalInterceptorForTesting::Activate(blink::TransferableMessage data,
                                           ActivateCallback callback) {
  portal_activated_ = true;

  if (run_loop_) {
    run_loop_->Quit();
    run_loop_ = nullptr;
  }

  // |this| can be destroyed after Activate() is called.
  portal_->Activate(std::move(data), std::move(callback));
}

void PortalInterceptorForTesting::Navigate(
    const GURL& url,
    blink::mojom::ReferrerPtr referrer,
    blink::mojom::Portal::NavigateCallback callback) {
  if (navigate_callback_) {
    navigate_callback_.Run(url, std::move(referrer), std::move(callback));
    return;
  }

  portal_->Navigate(url, std::move(referrer), std::move(callback));
}

void PortalInterceptorForTesting::WaitForActivate() {
  if (portal_activated_)
    return;

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
}

}  // namespace content
