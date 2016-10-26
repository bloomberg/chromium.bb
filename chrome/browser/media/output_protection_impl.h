// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_
#define CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_

#include "content/public/browser/render_frame_host.h"
#include "media/mojo/interfaces/output_protection.mojom.h"

namespace chrome {
class OutputProtectionProxy;
}

// Implements media::mojom::OutputProtection to check display links and
// their statuses. On all platforms we'll check the network links. On ChromeOS
// we'll also check the hardware links. Can only be used on the UI thread.
class OutputProtectionImpl : public media::mojom::OutputProtection {
 public:
  static void Create(content::RenderFrameHost* render_frame_host,
                     media::mojom::OutputProtectionRequest request);

  OutputProtectionImpl(int render_process_id, int render_frame_id);
  ~OutputProtectionImpl() final;

  // media::mojom::OutputProtection implementation.
  void QueryStatus(const QueryStatusCallback& callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        const EnableProtectionCallback& callback) final;

 private:
  // Callbacks for QueryStatus and EnableProtection results.
  // Note: These are bound using weak pointers so that we won't fire |callback|
  // after the binding is destroyed.
  void OnQueryStatusResult(const QueryStatusCallback& callback,
                           bool success,
                           uint32_t link_mask,
                           uint32_t protection_mask);
  void OnEnableProtectionResult(const EnableProtectionCallback& callback,
                                bool success);

  // Helper function to lazily create the |proxy_| and return it.
  chrome::OutputProtectionProxy* GetProxy();

  const int render_process_id_;
  const int render_frame_id_;

  std::unique_ptr<chrome::OutputProtectionProxy> proxy_;

  base::WeakPtrFactory<OutputProtectionImpl> weak_factory_;
};

#endif  // CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_
