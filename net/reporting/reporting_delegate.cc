// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delegate.h"

#include "base/memory/ptr_util.h"

namespace net {

namespace {

class ReportingDelegateImpl : public ReportingDelegate {
 public:
  ReportingDelegateImpl() {}

  ~ReportingDelegateImpl() override {}

  bool CanQueueReport(const url::Origin& origin) const override { return true; }

  bool CanSendReport(const url::Origin& origin) const override { return true; }

  bool CanSetClient(const url::Origin& origin,
                    const GURL& endpoint) const override {
    return true;
  }

  bool CanUseClient(const url::Origin& origin,
                    const GURL& endpoint) const override {
    return true;
  }
};

}  // namespace

// static
std::unique_ptr<ReportingDelegate> ReportingDelegate::Create() {
  return base::MakeUnique<ReportingDelegateImpl>();
}

ReportingDelegate::~ReportingDelegate() {}

}  // namespace net
