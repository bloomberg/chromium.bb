// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_OBSERVER_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_OBSERVER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"

namespace data_use_measurement {
namespace page_load_capping {

class PageLoadObserver : public DataUseAscriber::PageLoadObserver {
 public:
  PageLoadObserver();
  ~PageLoadObserver() override;

 private:
  // DataUseAscriber::PageLoadObserver:
  void OnPageLoadCommit(DataUse* data_use) override;
  void OnPageResourceLoad(const net::URLRequest& request,
                          DataUse* data_use) override;
  void OnPageDidFinishLoad(DataUse* data_use) override;
  void OnPageLoadConcluded(DataUse* data_use) override;
  void OnNetworkBytesUpdate(const net::URLRequest& request,
                            DataUse* data_use) override;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PageLoadObserver);
};

}  // namespace page_load_capping
}  // namespace data_use_measurement

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_OBSERVER_H_
