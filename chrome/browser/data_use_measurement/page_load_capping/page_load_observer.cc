// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_observer.h"

namespace data_use_measurement {
namespace page_load_capping {

PageLoadObserver::PageLoadObserver() = default;
PageLoadObserver::~PageLoadObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// TODO(ryansturm): Add heavy page detection. https://crbug.com/797976
void PageLoadObserver::OnPageLoadCommit(DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PageLoadObserver::OnPageResourceLoad(const net::URLRequest& request,
                                          DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PageLoadObserver::OnPageDidFinishLoad(DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PageLoadObserver::OnPageLoadConcluded(DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PageLoadObserver::OnNetworkBytesUpdate(const net::URLRequest& request,
                                            DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace page_load_capping
}  // namespace data_use_measurement
