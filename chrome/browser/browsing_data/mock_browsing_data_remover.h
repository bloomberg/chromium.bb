// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_H_

#include <list>

#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

// A BrowsingDataRemover that only records Remove*() calls.
// Some of the other methods are NOTIMPLEMENTED() as they are not needed for
// existing testcases.
class MockBrowsingDataRemover : public BrowsingDataRemover,
                                public KeyedService {
 public:
  explicit MockBrowsingDataRemover(content::BrowserContext* context);

  ~MockBrowsingDataRemover() override;

  // KeyedService:
  void Shutdown() override;

  // BrowsingDataRemover:
  void Remove(const base::Time& delete_begin,
              const base::Time& delete_end,
              int remove_mask,
              int origin_type_mask) override;
  void RemoveAndReply(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      int remove_mask,
                      int origin_type_mask,
                      Observer* observer) override;
  void RemoveWithFilter(const base::Time& delete_begin,
                        const base::Time& delete_end,
                        int remove_mask,
                        int origin_type_mask,
                        std::unique_ptr<content::BrowsingDataFilterBuilder>
                            filter_builder) override;
  void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const base::Time& GetLastUsedBeginTime() override;
  const base::Time& GetLastUsedEndTime() override;
  int GetLastUsedRemovalMask() override;
  int GetLastUsedOriginTypeMask() override;
  void SetEmbedderDelegate(
      std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) override;
  BrowsingDataRemoverDelegate* GetEmbedderDelegate() const override;

  // Add an expected call for testing.
  void ExpectCall(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder);

  // Add an expected call that doesn't have to match the filter_builder.
  void ExpectCallDontCareAboutFilterBuilder(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            int remove_mask,
                                            int origin_type_mask);

  // Verify that expected and actual calls match.
  void VerifyAndClearExpectations();

 private:
  class CallParameters {
   public:
    CallParameters(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        int remove_mask,
        int origin_type_mask,
        std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
        bool should_compare_filter);
    ~CallParameters();

    bool operator==(const CallParameters& other) const;

   private:
    base::Time delete_begin_;
    base::Time delete_end_;
    int remove_mask_;
    int origin_type_mask_;
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder_;
    bool should_compare_filter_;
  };

  void RemoveInternal(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

  std::list<CallParameters> actual_calls_;
  std::list<CallParameters> expected_calls_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_H_
