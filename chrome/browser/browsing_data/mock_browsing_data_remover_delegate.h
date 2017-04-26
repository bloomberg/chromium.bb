// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <list>
#include <memory>

#include "base/time/time.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"

// A BrowsingDataRemoverDelegate that only records RemoveEmbedderData() calls.
class MockBrowsingDataRemoverDelegate
    : public content::BrowsingDataRemoverDelegate {
 public:
  MockBrowsingDataRemoverDelegate();
  ~MockBrowsingDataRemoverDelegate() override;

  // BrowsingDataRemoverDelegate:
  content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
  GetOriginTypeMatcher() const override;
  bool MayRemoveDownloadHistory() const override;
  void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      const content::BrowsingDataFilterBuilder& filter_builder,
      int origin_type_mask,
      const base::Closure& callback) override;

  // Add an expected call for testing.
  void ExpectCall(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  int remove_mask,
                  int origin_type_mask,
                  const content::BrowsingDataFilterBuilder& filter_builder);

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

  std::list<CallParameters> actual_calls_;
  std::list<CallParameters> expected_calls_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
