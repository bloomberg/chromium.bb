// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"

class Profile;

class SyncInternalsMessageHandler : public DOMMessageHandler,
                                    public ProfileSyncServiceObserver {
 public:
  // Does not take ownership of |profile|, which must outlive this
  // object.
  explicit SyncInternalsMessageHandler(Profile* profile);
  virtual ~SyncInternalsMessageHandler();

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged();

 protected:
  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  // Callback handlers.
  void HandleGetAboutInfo(const ListValue* args);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
