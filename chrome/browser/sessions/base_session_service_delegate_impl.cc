// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/base_session_service_delegate_impl.h"

#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

BaseSessionServiceDelegateImpl::BaseSessionServiceDelegateImpl(
    bool should_use_delayed_save)
      : should_use_delayed_save_(should_use_delayed_save) {}

base::SequencedWorkerPool* BaseSessionServiceDelegateImpl::GetBlockingPool() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return content::BrowserThread::GetBlockingPool();
}

bool BaseSessionServiceDelegateImpl::ShouldTrackEntry(const GURL& url) {
  // Blacklist chrome://quit and chrome://restart to avoid quit or restart
  // loops.
  return url.is_valid() &&
         !(url.SchemeIs(content::kChromeUIScheme) &&
          (url.host() == chrome::kChromeUIQuitHost ||
           url.host() == chrome::kChromeUIRestartHost));
}

bool BaseSessionServiceDelegateImpl::ShouldUseDelayedSave() {
  return should_use_delayed_save_;
}
