// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "content/public/browser/render_frame_host.h"
#include "media/mojo/interfaces/platform_verification.mojom.h"

namespace chromeos {
namespace attestation {

// Implements media::mojom::PlatformVerification on ChromeOS using
// PlatformVerificationFlow. Can only be used on the UI thread because
// PlatformVerificationFlow lives on the UI thread.
class PlatformVerificationImpl : public media::mojom::PlatformVerification {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<media::mojom::PlatformVerification> request);

  explicit PlatformVerificationImpl(
      content::RenderFrameHost* render_frame_host);
  ~PlatformVerificationImpl() override;

  // mojo::InterfaceImpl<PlatformVerification> implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         const ChallengePlatformCallback& callback) override;

 private:
  using Result = PlatformVerificationFlow::Result;

  void OnPlatformChallenged(const ChallengePlatformCallback& callback,
                            Result result,
                            const std::string& signed_data,
                            const std::string& signature,
                            const std::string& platform_key_certificate);

  content::RenderFrameHost* const render_frame_host_;

  scoped_refptr<PlatformVerificationFlow> platform_verification_flow_;

  base::WeakPtrFactory<PlatformVerificationImpl> weak_factory_;
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_IMPL_H_
