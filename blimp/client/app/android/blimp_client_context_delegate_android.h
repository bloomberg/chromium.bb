// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_

#include "blimp/client/public/blimp_client_context_delegate.h"
#include "google_apis/gaia/identity_provider.h"

namespace blimp {
namespace client {

class BlimpClientContext;
class BlimpContents;

class BlimpClientContextDelegateAndroid : public BlimpClientContextDelegate {
 public:
  explicit BlimpClientContextDelegateAndroid(
      BlimpClientContext* blimp_client_context);
  ~BlimpClientContextDelegateAndroid() override;

  // BlimpClientContextDelegate implementation.
  void AttachBlimpContentsHelpers(BlimpContents* blimp_contents) override;
  void OnAssignmentConnectionAttempted(AssignmentRequestResult result,
                                       const Assignment& assignment) override;
  std::unique_ptr<IdentityProvider> CreateIdentityProvider() override;
  void OnAuthenticationError(const GoogleServiceAuthError& error) override;
  void OnConnected() override;
  void OnEngineDisconnected(int result) override;
  void OnNetworkDisconnected(int result) override;

 private:
  void OnDisconnected(const base::string16& reason);
  void ShowMessage(const base::string16& message);

  // The BlimpClientContext this is the delegate for.
  BlimpClientContext* blimp_client_context_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextDelegateAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_
