// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"
#include "components/arc/common/auth.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcFetcherBase;
class ArcBridgeService;

// Implementation of ARC authorization.
class ArcAuthService : public KeyedService,
                       public mojom::AuthHost,
                       public InstanceHolder<mojom::AuthInstance>::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAuthService* GetForBrowserContext(content::BrowserContext* context);

  ArcAuthService(content::BrowserContext* profile,
                 ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  // InstanceHolder<mojom::AuthInstance>::Observer:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::AuthHost:
  void OnSignInComplete() override;
  void OnSignInFailed(mojom::ArcSignInFailureReason reason) override;
  void RequestAccountInfo() override;
  void ReportMetrics(mojom::MetricsType metrics_type, int32_t value) override;
  void ReportAccountCheckStatus(mojom::AccountCheckStatus status) override;

  // Deprecated methods:
  // For security reason this code can be used only once and exists for specific
  // period of time.
  void GetAuthCodeDeprecated0(GetAuthCodeDeprecated0Callback callback) override;
  void GetAuthCodeDeprecated(GetAuthCodeDeprecatedCallback callback) override;
  void GetAuthCodeAndAccountTypeDeprecated(
      GetAuthCodeAndAccountTypeDeprecatedCallback callback) override;
  void GetIsAccountManagedDeprecated(
      GetIsAccountManagedDeprecatedCallback callback) override;

 private:
  using AccountInfoCallback = base::Callback<void(mojom::AccountInfoPtr)>;
  class AccountInfoNotifier;

  // Starts to request account info.
  void RequestAccountInfoInternal(
      std::unique_ptr<AccountInfoNotifier> account_info_notifier);

  // Callbacks when auth info is fetched.
  void OnEnrollmentTokenFetched(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status status,
      const std::string& enrollment_token,
      const std::string& user_id);
  void OnAuthCodeFetched(bool success, const std::string& auth_code);

  // Called to let ARC container know the account info.
  void OnAccountInfoReady(mojom::AccountInfoPtr account_info);

  Profile* const profile_;
  ArcBridgeService* const arc_bridge_service_;

  mojo::Binding<mojom::AuthHost> binding_;

  std::unique_ptr<AccountInfoNotifier> notifier_;
  std::unique_ptr<ArcFetcherBase> fetcher_;

  base::WeakPtrFactory<ArcAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_
