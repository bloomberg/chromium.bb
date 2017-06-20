// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_

#include <memory>
#include <queue>

#include "base/macros.h"
#include "components/password_manager/core/browser/affiliation_fetcher.h"
#include "components/password_manager/core/browser/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/test_affiliation_fetcher_factory.h"

namespace password_manager {

// A fake AffiliationFetcher that can be used in tests to return fake API
// responses to users of AffiliationFetcher.
class FakeAffiliationFetcher : public AffiliationFetcher {
 public:
  FakeAffiliationFetcher(net::URLRequestContextGetter* request_context_getter,
                         const std::vector<FacetURI>& facet_ids,
                         AffiliationFetcherDelegate* delegate);
  ~FakeAffiliationFetcher() override;

  // Simulates successful completion of the request with |fake_result|. Note
  // that the consumer may choose to destroy |this| from within this call.
  void SimulateSuccess(
      std::unique_ptr<AffiliationFetcherDelegate::Result> fake_result);

  // Simulates completion of the request with failure. Note that the consumer
  // may choose to destroy |this| from within this call.
  void SimulateFailure();

 private:
  void StartRequest() override;

  DISALLOW_COPY_AND_ASSIGN(FakeAffiliationFetcher);
};

// While this factory is in scope, calls to AffiliationFetcher::Create() will
// produce FakeAffiliationFetchers that can be used in tests to return fake API
// responses to users of AffiliationFetcher. Nesting is not supported.
class ScopedFakeAffiliationFetcherFactory
    : public TestAffiliationFetcherFactory {
 public:
  ScopedFakeAffiliationFetcherFactory();
  ~ScopedFakeAffiliationFetcherFactory() override;

  // Returns the next FakeAffiliationFetcher instance previously produced, so
  // that that the testing code can inject a response and simulate completion
  // or failure of the request. The fetcher is removed from the queue of pending
  // fetchers.
  //
  // Note that the factory does not retain ownership of the produced fetchers,
  // so that the tests should ensure that the corresponding production code will
  // not destroy them before they are accessed here.
  FakeAffiliationFetcher* PopNextFetcher();

  // Same as above, but the fetcher is not removed from the queue of pending
  // fetchers.
  FakeAffiliationFetcher* PeekNextFetcher();

  bool has_pending_fetchers() const { return !pending_fetchers_.empty(); }

  // AffiliationFetcherFactory:
  AffiliationFetcher* CreateInstance(
      net::URLRequestContextGetter* request_context_getter,
      const std::vector<FacetURI>& facet_ids,
      AffiliationFetcherDelegate* delegate) override;

 private:
  // Fakes created by this factory. The elements are owned by the production
  // code that normally owns the result of AffiliationFetcher::Create().
  std::queue<FakeAffiliationFetcher*> pending_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeAffiliationFetcherFactory);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_
