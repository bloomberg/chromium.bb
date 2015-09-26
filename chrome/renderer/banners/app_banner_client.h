// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BANNERS_APP_BANNER_CLIENT_H_
#define CHROME_RENDERER_BANNERS_APP_BANNER_CLIENT_H_

#include <stdint.h>
#include <string>

#include "base/id_map.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerClient.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptResult.h"

namespace IPC {
class Message;
}  // namespace IPC

class AppBannerClient : public content::RenderFrameObserver,
                        public blink::WebAppBannerClient {
 public:
  explicit AppBannerClient(content::RenderFrame* render_frame);
  ~AppBannerClient() override;

 private:
  // content::RenderFrame::Observer implementation.
  void OnDestruct() override;

  bool OnMessageReceived(const IPC::Message& message) override;

  // WebAppBannerClient implementation.
  void registerBannerCallbacks(int request_id,
                               blink::WebAppBannerCallbacks*) override;

  void showAppBanner(int request_id) override;

  void ResolveEvent(int request_id,
                    const std::string& platform,
                    const blink::WebAppBannerPromptResult::Outcome& outcome);
  void OnBannerAccepted(int request_id, const std::string& platform);
  void OnBannerDismissed(int request_id);

  IDMap<blink::WebAppBannerCallbacks, IDMapOwnPointer>
      banner_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerClient);
};

#endif  // CHROME_RENDERER_BANNERS_APP_BANNER_CLIENT_H_
