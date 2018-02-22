// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TEST_FAKE_WEB_CONTENTS_OBSERVER_H_
#define CHROMECAST_BROWSER_TEST_FAKE_WEB_CONTENTS_OBSERVER_H_

#include <string>

#include "chromecast/browser/application_media_capabilities.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class RenderViewHost;
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class FakeWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit FakeWebContentsObserver(content::WebContents* web_contents);
  ~FakeWebContentsObserver() override;

 private:
  // content::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;

  service_manager::BinderRegistry registry_;
  ApplicationMediaCapabilities app_media_capabilities_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsObserver);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_FAKE_WEB_CONTENTS_OBSERVER_H_
