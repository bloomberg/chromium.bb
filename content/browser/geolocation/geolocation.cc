// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProvider::GetInstance()->OverrideLocationForTesting(position);
  callback_loop->PostTask(FROM_HERE, completion_callback);
}

void GeolocationUpdateCallbackOnIOThread(
    const GeolocationUpdateCallback& client_callback,
    scoped_refptr<base::MessageLoopProxy> client_loop,
    const Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (base::MessageLoopProxy::current() != client_loop)
    client_loop->PostTask(FROM_HERE, base::Bind(client_callback, position));
  else
    client_callback.Run(position);
}

void RequestLocationUpdateOnIOThread(
    const GeolocationUpdateCallback& client_callback,
    scoped_refptr<base::MessageLoopProxy> client_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationUpdateCallback io_thread_callback = base::Bind(
      &GeolocationUpdateCallbackOnIOThread,
      client_callback,
      client_loop);
  GeolocationProvider::GetInstance()->RequestCallback(io_thread_callback);
}

}  // namespace

void OverrideLocationForTesting(
    const Geoposition& position,
    const base::Closure& completion_callback) {
  base::Closure closure = base::Bind(&OverrideLocationForTestingOnIOThread,
                                     position,
                                     completion_callback,
                                     base::MessageLoopProxy::current());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, closure);
  else
    closure.Run();
}

void RequestLocationUpdate(const GeolocationUpdateCallback& callback) {
  base::Closure closure = base::Bind(&RequestLocationUpdateOnIOThread,
                                     callback,
                                     base::MessageLoopProxy::current());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, closure);
  else
    closure.Run();
}

}  // namespace content
