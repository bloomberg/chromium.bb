// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/interfaces/platform_verification.mojom.h"

namespace chromeos {
namespace attestation {

// Implements media::mojom::PlatformVerification on ChromeOS using
// PlatformVerificationFlow. Can only be used on the UI thread because
// PlatformVerificationFlow lives on the UI thread.
class PlatformVerificationImpl final
    : public content::FrameServiceBase<media::mojom::PlatformVerification> {
 public:
  static void Create(content::RenderFrameHost* render_frame_host,
                     media::mojom::PlatformVerificationRequest request);

  PlatformVerificationImpl(content::RenderFrameHost* render_frame_host,
                           media::mojom::PlatformVerificationRequest request);

  // mojo::InterfaceImpl<PlatformVerification> implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCallback callback) final;

 private:
  // |this| can only be destructed as a FrameServiceBase.
  ~PlatformVerificationImpl() final;

  using Result = PlatformVerificationFlow::Result;

  void OnPlatformChallenged(ChallengePlatformCallback callback,
                            Result result,
                            const std::string& signed_data,
                            const std::string& signature,
                            const std::string& platform_key_certificate);

  scoped_refptr<PlatformVerificationFlow> platform_verification_flow_;
  base::WeakPtrFactory<PlatformVerificationImpl> weak_factory_;
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_
