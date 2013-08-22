// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__

#include <string>

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/common/extensions/api/sessions.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"

class Profile;

namespace browser_sync {
struct SyncedSession;
}

namespace extensions {

class SessionId;

class SessionsGetRecentlyClosedFunction : public SyncExtensionFunction {
 protected:
  virtual ~SessionsGetRecentlyClosedFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessions.getRecentlyClosed",
                             SESSIONS_GETRECENTLYCLOSED)

 private:
  scoped_ptr<api::tabs::Tab> CreateTabModel(const TabRestoreService::Tab& tab,
                                            int session_id,
                                            int selected_index);
  scoped_ptr<api::windows::Window> CreateWindowModel(
      const TabRestoreService::Window& window,
      int session_id);
  scoped_ptr<api::sessions::Session> CreateSessionModel(
      const TabRestoreService::Entry* entry);
};

class SessionsGetDevicesFunction : public SyncExtensionFunction {
 protected:
  virtual ~SessionsGetDevicesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessions.getDevices", SESSIONS_GETDEVICES)

 private:
  scoped_ptr<api::tabs::Tab> CreateTabModel(const std::string& session_tag,
                                            const SessionTab& tab,
                                            int tab_index,
                                            int selected_index);
  scoped_ptr<api::windows::Window> CreateWindowModel(
      const SessionWindow& window,
      const std::string& session_tag);
  scoped_ptr<api::sessions::Session> CreateSessionModel(
      const SessionWindow& window,
      const std::string& session_tag);
  scoped_ptr<api::sessions::Device> CreateDeviceModel(
      const browser_sync::SyncedSession* session);
};

class SessionsRestoreFunction : public SyncExtensionFunction {
 protected:
  virtual ~SessionsRestoreFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessions.restore", SESSIONS_RESTORE)

 private:
  void SetInvalidIdError(const std::string& invalid_id);
  void SetResultRestoredTab(const content::WebContents* contents);
  bool SetResultRestoredWindow(int window_id);
  bool RestoreMostRecentlyClosed(Browser* browser);
  bool RestoreLocalSession(const SessionId& session_id, Browser* browser);
  bool RestoreForeignSession(const SessionId& session_id,
                             Browser* browser);
};

class SessionsAPI : public ProfileKeyedAPI {
 public:
  explicit SessionsAPI(Profile* profile);
  virtual ~SessionsAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SessionsAPI>* GetFactoryInstance();
 private:
  friend class ProfileKeyedAPIFactory<SessionsAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SessionsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
