// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/keep_alive_service_impl.h"

#include "base/message_loop/message_loop.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "content/public/browser/browser_thread.h"

ScopedKeepAlive::ScopedKeepAlive() {
  chrome::StartKeepAlive();
}

ScopedKeepAlive::~ScopedKeepAlive() {
  chrome::EndKeepAlive();
}

KeepAliveServiceImpl::KeepAliveServiceImpl() {
}

KeepAliveServiceImpl::~KeepAliveServiceImpl() {
}

void KeepAliveServiceImpl::EnsureKeepAlive() {
  if (!keep_alive_)
    keep_alive_.reset(new ScopedKeepAlive());
}

void KeepAliveServiceImpl::FreeKeepAlive() {
  if (keep_alive_) {
    // We may end up here as the result of the OS deleting the AppList's
    // widget (WidgetObserver::OnWidgetDestroyed). If this happens and there
    // are no browsers around then deleting the keep alive will result in
    // deleting the Widget again (by way of CloseAllSecondaryWidgets). When
    // the stack unravels we end up back in the Widget that was deleted and
    // crash. By delaying deletion of the keep alive we ensure the Widget has
    // correctly been destroyed before ending the keep alive so that
    // CloseAllSecondaryWidgets() won't attempt to delete the AppList's Widget
    // again.
    base::MessageLoop::current()->DeleteSoon(FROM_HERE,
                                             keep_alive_.release());
  }
}
