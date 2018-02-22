// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/fake_web_contents_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromecast/common/application_media_capabilities.mojom.h"

namespace chromecast {
namespace shell {

FakeWebContentsObserver::FakeWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  registry_.AddInterface<mojom::ApplicationMediaCapabilities>(
      base::BindRepeating(&ApplicationMediaCapabilities::AddBinding,
                          base::Unretained(&app_media_capabilities_)));
}

FakeWebContentsObserver::~FakeWebContentsObserver() = default;

void FakeWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* /* navigation_handle */) {}

void FakeWebContentsObserver::DidFirstVisuallyNonEmptyPaint() {}

void FakeWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* /* render_view_host */) {}

void FakeWebContentsObserver::RenderViewReady() {}

void FakeWebContentsObserver::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* /* render_frame_host */,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

}  // namespace shell
}  // namespace chromecast
