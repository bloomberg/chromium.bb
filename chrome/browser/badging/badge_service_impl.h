// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
#define CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_

#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/platform/modules/badging/badging.mojom.h"

namespace badging {
class BadgeManager;
}

namespace content {
class RenderFrameHost;
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
}

// Desktop implementation of the BadgeService mojo service.
class BadgeServiceImpl
    : public content::FrameServiceBase<blink::mojom::BadgeService> {
 public:
  static void Create(blink::mojom::BadgeServiceRequest request,
                     content::RenderFrameHost* render_frame_host);

  // blink::mojom::BadgeService overrides.
  // TODO(crbug.com/719176): Support non-flag badges.
  void SetBadge() override;
  void ClearBadge() override;

 private:
  BadgeServiceImpl(content::RenderFrameHost* render_frame_host,
                   content::BrowserContext* browser_context,
                   badging::BadgeManager* badge_manager,
                   blink::mojom::BadgeServiceRequest request);
  ~BadgeServiceImpl() override;

  const extensions::Extension* ExtensionFromLastUrl();

  content::RenderFrameHost* render_frame_host_;
  content::BrowserContext* browser_context_;
  badging::BadgeManager* badge_manager_;
};

#endif  // CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
