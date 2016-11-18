// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/auth.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace net {
class URLRequestContextGetter;
}

namespace arc {

class ArcAuthCodeFetcher;
class ArcRobotAuth;

// Implementation of ARC authorization.
// TODO(hidehiko): Move to c/b/c/arc/auth with adding tests.
class ArcAuthService : public ArcService,
                       public mojom::AuthHost,
                       public InstanceHolder<mojom::AuthInstance>::Observer,
                       public ArcSupportHost::Observer {
 public:
  explicit ArcAuthService(ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

  // This is introduced to work with existing tests.
  // TODO(crbug.com/664095): Clean up the test and remove this method.
  static ArcAuthService* GetForTest();

  // InstanceHolder<mojom::AuthInstance>::Observer:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::AuthHost:
  void OnSignInComplete() override;
  void OnSignInFailed(mojom::ArcSignInFailureReason reason) override;
  void RequestAccountInfo() override;

  // Deprecated methods:
  // For security reason this code can be used only once and exists for specific
  // period of time.
  void GetAuthCodeDeprecated0(
      const GetAuthCodeDeprecated0Callback& callback) override;
  void GetAuthCodeDeprecated(
      const GetAuthCodeDeprecatedCallback& callback) override;
  void GetAuthCodeAndAccountTypeDeprecated(
      const GetAuthCodeAndAccountTypeDeprecatedCallback& callback) override;
  void GetIsAccountManagedDeprecated(
      const GetIsAccountManagedDeprecatedCallback& callback) override;

  // ArcSupportHost::Observer:
  void OnAuthSucceeded(const std::string& auth_code) override;
  void OnRetryClicked() override;

 private:
  using AccountInfoCallback = base::Callback<void(mojom::AccountInfoPtr)>;
  class AccountInfoNotifier;

  // Starts to request account info.
  void RequestAccountInfoInternal(
      std::unique_ptr<AccountInfoNotifier> account_info_notifier);

  // Called when HTTP context is prepared.
  void OnContextPrepared(net::URLRequestContextGetter* request_context_getter);

  void OnAccountInfoReady(mojom::AccountInfoPtr account_info);

  // Callback for Robot auth in Kiosk mode.
  void OnRobotAuthCodeFetched(const std::string& auth_code);

  // Callback for automatic auth code fetching when --arc-user-auth-endpoint
  // flag is set.
  void OnAuthCodeFetched(const std::string& auth_code);

  // Common procedure across LSO auth code fetching, automatic auth code
  // fetching, and Robot auth.
  void OnAuthCodeObtained(const std::string& auth_code);

  mojo::Binding<mojom::AuthHost> binding_;

  std::unique_ptr<ArcAuthCodeFetcher> auth_code_fetcher_;
  std::unique_ptr<ArcRobotAuth> arc_robot_auth_;

  std::unique_ptr<AccountInfoNotifier> notifier_;

  base::WeakPtrFactory<ArcAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
