// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace syncfs_internals {

// This class handles message from WebUI page of chrome://syncfs-internals/.
// All methods in this class should be called on UI thread.
class SyncFileSystemInternalsHandler : public content::WebUIMessageHandler {
 public:
  explicit SyncFileSystemInternalsHandler(Profile* profile);
  virtual ~SyncFileSystemInternalsHandler();
  virtual void RegisterMessages() OVERRIDE;

 private:
  void GetServiceStatus(const base::ListValue* args);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemInternalsHandler);
};
}  // syncfs_internals

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_
