// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
#define CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_

#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/platform/modules/badging/badging.mojom.h"

namespace content {
class RenderFrameHost;
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

#if defined(OS_CHROMEOS)
namespace badging {
class BadgeManager;
}
#else
class BadgeServiceDelegate;
#endif

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
                   blink::mojom::BadgeServiceRequest request);
  ~BadgeServiceImpl() override;

  const extensions::Extension* ExtensionFromLastUrl();

  content::RenderFrameHost* render_frame_host_;
  content::BrowserContext* browser_context_;
  content::WebContents* web_contents_;
#if defined(OS_CHROMEOS)
  badging::BadgeManager* badge_manager_;
#else
  BadgeServiceDelegate* delegate_;
  bool is_hosted_app_;
#endif
};

#endif  // CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
