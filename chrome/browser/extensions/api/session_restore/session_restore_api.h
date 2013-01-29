// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSION_RESTORE_SESSION_RESTORE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSION_RESTORE_SESSION_RESTORE_API_H__

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/common/extensions/api/session_restore.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"

class Profile;

namespace extensions {

class SessionRestoreGetRecentlyClosedFunction : public SyncExtensionFunction {
 protected:
  virtual ~SessionRestoreGetRecentlyClosedFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessionRestore.getRecentlyClosed",
      SESSIONRESTORE_GETRECENTLYCLOSED)

 private:
  scoped_ptr<api::tabs::Tab> CreateTabModel(
      const TabRestoreService::Tab& tab, int selected_index);
  scoped_ptr<api::windows::Window> CreateWindowModel(
      const TabRestoreService::Window& window);
  scoped_ptr<api::session_restore::ClosedEntry> CreateEntryModel(
      const TabRestoreService::Entry* entry);
};

class SessionRestoreRestoreFunction : public SyncExtensionFunction {
 protected:
  virtual ~SessionRestoreRestoreFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessionRestore.restore", SESSIONRESTORE_RESTORE)
};

class SessionRestoreAPI : public ProfileKeyedAPI {
 public:
  explicit SessionRestoreAPI(Profile* profile);
  virtual ~SessionRestoreAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SessionRestoreAPI>* GetFactoryInstance();
 private:
  friend class ProfileKeyedAPIFactory<SessionRestoreAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SessionRestoreAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSION_RESTORE_SESSION_RESTORE_API_H__
