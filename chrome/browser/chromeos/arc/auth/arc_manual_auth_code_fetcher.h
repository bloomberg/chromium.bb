// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_MANUAL_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_MANUAL_AUTH_CODE_FETCHER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_info_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace arc {

class ArcAuthContext;

// Implements the auth token fetching procedure with user's "SIGN IN" button
// click.
class ArcManualAuthCodeFetcher : public ArcAuthInfoFetcher,
                                 public ArcSupportHost::Observer {
 public:
  ArcManualAuthCodeFetcher(ArcAuthContext* context,
                           ArcSupportHost* support_host);
  ~ArcManualAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(const FetchCallback& callback) override;

 private:
  void FetchInternal();

  // Called when HTTP request gets ready.
  void OnContextPrepared(net::URLRequestContextGetter* request_context_getter);

  // ArcSupportHost::Observer:
  void OnAuthSucceeded(const std::string& auth_code) override;
  void OnAuthFailed() override;
  void OnRetryClicked() override;

  ArcAuthContext* const context_;
  ArcSupportHost* const support_host_;

  FetchCallback pending_callback_;

  base::WeakPtrFactory<ArcManualAuthCodeFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcManualAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_MANUAL_AUTH_CODE_FETCHER_H_
