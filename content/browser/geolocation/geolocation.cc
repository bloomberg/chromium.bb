// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/geoposition.h"

namespace content {

namespace {

void OverrideLocationForTestingOnIOThread(
    const Geoposition& position,
    const base::Closure& completion_callback,
    scoped_refptr<base::MessageLoopProxy> callback_loop) {
  GeolocationProvider::GetInstance()->OverrideLocationForTesting(position);
  callback_loop->PostTask(FROM_HERE, completion_callback);
}

}  // namespace

void OverrideLocationForTesting(
    const Geoposition& position,
    const base::Closure& completion_callback) {
  base::Closure closure = base::Bind(&OverrideLocationForTestingOnIOThread,
                                     position,
                                     completion_callback,
                                     base::MessageLoopProxy::current());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, closure);
  } else {
    closure.Run();
  }
}

}  // namespace content
