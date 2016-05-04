// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sth_distributor.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "net/cert/signed_tree_head.h"

namespace {
const char kPilotLogID[33] =
    "\xa4\xb9\x09\x90\xb4\x18\x58\x14\x87\xbb\x13\xa2\xcc\x67\x70\x0a\x3c\x35"
    "\x98\x04\xf9\x1b\xdf\xb8\xe3\x77\xcd\x0e\xc8\x0d\xdc\x10";
}

namespace net {

namespace ct {

STHDistributor::STHDistributor()
    : observer_list_(base::ObserverList<STHObserver>::NOTIFY_EXISTING_ONLY) {}

STHDistributor::~STHDistributor() {}

void STHDistributor::NewSTHObserved(const SignedTreeHead& sth) {
  FOR_EACH_OBSERVER(STHObserver, observer_list_, NewSTHObserved(sth));

  if (sth.log_id.compare(0, sth.log_id.size(), kPilotLogID,
                         arraysize(kPilotLogID) - 1) != 0)
    return;

  const base::TimeDelta sth_age = base::Time::Now() - sth.timestamp;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertificateTransparency.PilotSTHAge", sth_age,
                             base::TimeDelta::FromHours(1),
                             base::TimeDelta::FromDays(4), 100);
}

void STHDistributor::RegisterObserver(STHObserver* observer) {
  observer_list_.AddObserver(observer);
}

void STHDistributor::UnregisterObserver(STHObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace ct

}  // namespace net
