// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_STH_DISTRIBUTOR_H_
#define NET_CERT_STH_DISTRIBUTOR_H_

#include "base/observer_list.h"
#include "net/base/net_export.h"
#include "net/cert/sth_observer.h"
#include "net/cert/sth_reporter.h"

namespace net {

namespace ct {

// A proxy for delegating new STH notifications to all registered
// observers.
// For each |observer| registered with RegisterObserver, the
// NewSTHObserved method will be called whenever the STHDistributor's
// NewSTHObserved method is invoked.
class NET_EXPORT STHDistributor : public STHObserver, public STHReporter {
 public:
  STHDistributor();
  ~STHDistributor() override;

  // STHObserver implementation.
  void NewSTHObserved(const SignedTreeHead& sth) override;

  // STHReporter implementation
  void RegisterObserver(STHObserver* observer) override;
  void UnregisterObserver(STHObserver* observer) override;

 private:
  base::ObserverList<STHObserver> observer_list_;
};

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_STH_DISTRIBUTOR_H_
