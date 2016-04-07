// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sth_distributor.h"

#include "net/cert/signed_tree_head.h"

namespace net {

namespace ct {

STHDistributor::STHDistributor()
    : observer_list_(base::ObserverList<STHObserver>::NOTIFY_EXISTING_ONLY) {}

STHDistributor::~STHDistributor() {}

void STHDistributor::NewSTHObserved(const SignedTreeHead& sth) {
  FOR_EACH_OBSERVER(STHObserver, observer_list_, NewSTHObserved(sth));
}

void STHDistributor::RegisterObserver(STHObserver* observer) {
  observer_list_.AddObserver(observer);
}

void STHDistributor::UnregisterObserver(STHObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace ct

}  // namespace net
