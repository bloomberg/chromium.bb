// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_
#define CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_

#include "content/public/browser/render_frame_host.h"
#include "media/mojo/interfaces/output_protection.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

// Implements media::mojom::OutputProtection to check display links and
// their statuses. On all platforms we'll check the network links. On ChromeOS
// we'll also check the hardware links. Can only be used on the UI thread.
class OutputProtectionImpl : public media::mojom::OutputProtection {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<media::mojom::OutputProtection> request);

  OutputProtectionImpl(content::RenderFrameHost* render_frame_host,
                       mojo::InterfaceRequest<OutputProtection> request);
  ~OutputProtectionImpl() final;

  // media::mojom::OutputProtection implementation.
  void QueryStatus(const QueryStatusCallback& callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        const EnableProtectionCallback& callback) final;

 private:
  mojo::StrongBinding<media::mojom::OutputProtection> binding_;

  content::RenderFrameHost* const render_frame_host_;

  base::WeakPtrFactory<OutputProtectionImpl> weak_factory_;
};

#endif  // CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_
