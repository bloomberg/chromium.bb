// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_

#include "components/offline_pages/core/background/offliner.h"

namespace offline_pages {

// Test class stubbing out the functionality of Offliner.
// It is only used for test support.
class OfflinerStub : public Offliner {
 public:
  OfflinerStub();
  ~OfflinerStub() override;

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& completion_callback,
                   const ProgressCallback& progress_callback) override;

  void Cancel(const CancelCallback& callback) override;

  void disable_loading() { disable_loading_ = true; }

  void enable_callback(bool enable) { enable_callback_ = enable; }

  bool cancel_called() { return cancel_called_; }

  void reset_cancel_called() { cancel_called_ = false; }

  bool HandleTimeout(const SavePageRequest& request) override;

  void enable_snapshot_on_last_retry() { snapshot_on_last_retry_ = true; }

 private:
  CompletionCallback completion_callback_;
  ProgressCallback progress_callback_;
  bool disable_loading_;
  bool enable_callback_;
  bool cancel_called_;
  bool snapshot_on_last_retry_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_
