// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"

namespace chromeos {

namespace android_sms {

ConnectionEstablisherImpl::ConnectionEstablisherImpl() = default;
ConnectionEstablisherImpl::~ConnectionEstablisherImpl() = default;

void ConnectionEstablisherImpl::EstablishConnection(
    content::ServiceWorkerContext* service_worker_context) {
  // TODO(azeemarshad): Send connection message to service worker using
  // ServiceWorkerContext::StartServiceWorkerAndDispatchLongRunningMessage
  // when https://chromium-review.googlesource.com/c/chromium/src/+/1119580
  // is ready.
  return;
}

}  // namespace android_sms

}  // namespace chromeos
