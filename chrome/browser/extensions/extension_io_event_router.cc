// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_io_event_router.h"

#include "googleurl/src/gurl.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"

ExtensionIOEventRouter::ExtensionIOEventRouter(Profile* profile)
    : profile_(profile) {
}

ExtensionIOEventRouter::~ExtensionIOEventRouter() {
}

void ExtensionIOEventRouter::DispatchEvent(
  const std::string& event_name, const std::string& event_args) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &ExtensionIOEventRouter::DispatchEventOnUIThread,
                        event_name, event_args));
}

void ExtensionIOEventRouter::DispatchEventOnUIThread(
    const std::string& event_name, const std::string& event_args) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the profile has gone away, we're shutting down. If there's no event
  // router, the extension system hasn't been initialized.
  if (!profile_ || !profile_->GetExtensionEventRouter())
    return;

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, event_args, profile_, GURL());
}
