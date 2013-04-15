// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/eula_accepted_notifier.h"

#include "base/logging.h"

EulaAcceptedNotifier::EulaAcceptedNotifier() : observer_(NULL) {
}

EulaAcceptedNotifier::~EulaAcceptedNotifier() {
}

void EulaAcceptedNotifier::Init(Observer* observer) {
  DCHECK(!observer_ && observer);
  observer_ = observer;
}

void EulaAcceptedNotifier::NotifyObserver() {
  observer_->OnEulaAccepted();
}

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
// static
EulaAcceptedNotifier* EulaAcceptedNotifier::Create() {
  return NULL;
}
#endif
