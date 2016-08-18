// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

class IdentityProvider;
class Profile;

namespace blimp {
namespace client {
class BlimpClientContext;
class BlimpContents;
}  // namespace client
}  // namespace blimp

// The BlimpClientContextDelegate for //chrome which provides the necessary
// functionality required to run Blimp.
class ChromeBlimpClientContextDelegate
    : public blimp::client::BlimpClientContextDelegate {
 public:
  // Constructs the object and attaches it to the BlimpClientContext that is
  // used for the given profile.
  explicit ChromeBlimpClientContextDelegate(Profile* profile);
  // Unattaches itself from the BlimpClientContext before going away.
  ~ChromeBlimpClientContextDelegate() override;

  // BlimpClientContextDelegate implementation.
  void AttachBlimpContentsHelpers(
      blimp::client::BlimpContents* blimp_contents) override;
  void OnAssignmentConnectionAttempted(
      blimp::client::AssignmentRequestResult result,
      const blimp::client::Assignment& assignment) override;
  std::unique_ptr<IdentityProvider> CreateIdentityProvider() override;
  void OnAuthenticationError(
      BlimpClientContextDelegate::AuthError error) override;

 private:
  // The profile this delegate is used for.
  Profile* profile_;

  // The BlimpClientContext this is the delegate for.
  blimp::client::BlimpClientContext* blimp_client_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBlimpClientContextDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
