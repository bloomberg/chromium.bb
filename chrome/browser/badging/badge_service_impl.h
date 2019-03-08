// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
#define CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_

#include "base/optional.h"
#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"

namespace content {
class RenderFrameHost;
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace badging {
class BadgeManager;
}

// Desktop implementation of the BadgeService mojo service.
class BadgeServiceImpl
    : public content::FrameServiceBase<blink::mojom::BadgeService> {
 public:
  static void Create(blink::mojom::BadgeServiceRequest request,
                     content::RenderFrameHost* render_frame_host);

  // blink::mojom::BadgeService overrides.
  void SetInteger(uint64_t content) override;
  void SetFlag() override;
  void ClearBadge() override;

 private:
  BadgeServiceImpl(content::RenderFrameHost* render_frame_host,
                   blink::mojom::BadgeServiceRequest request);
  ~BadgeServiceImpl() override;

  void SetBadge(base::Optional<uint64_t> content);
  const extensions::Extension* ExtensionFromLastUrl();
  bool IsInApp();

  content::RenderFrameHost* render_frame_host_;
  content::BrowserContext* browser_context_;
  content::WebContents* web_contents_;
  badging::BadgeManager* badge_manager_;
};

#endif  // CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
