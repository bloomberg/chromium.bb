// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_STUB_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_STUB_H_

#include "components/offline_pages/background/offliner.h"

namespace offline_pages {

// Test class stubbing out the functionality of Offliner.
// It is only used for test support.
class OfflinerStub : public Offliner {
 public:
  OfflinerStub();
  ~OfflinerStub() override;

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override;

  void Cancel() override;

  void disable_loading() { disable_loading_ = true; }

  void enable_callback(bool enable) { enable_callback_ = enable; }

  bool cancel_called() { return cancel_called_; }

 private:
  CompletionCallback callback_;
  bool disable_loading_;
  bool enable_callback_;
  bool cancel_called_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_STUB_H_
