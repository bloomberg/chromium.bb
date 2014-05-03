// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__

#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/common/extensions/api/sessions.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

class Profile;

namespace browser_sync {
struct SyncedSession;
}

namespace extensions {

class SessionId;

class SessionsGetRecentlyClosedFunction : public ChromeSyncExtensionFunction {
 protected:
  virtual ~SessionsGetRecentlyClosedFunction() {}
  virtual bool RunSync() OVERRIDE;
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

class SessionsGetDevicesFunction : public ChromeSyncExtensionFunction {
 protected:
  virtual ~SessionsGetDevicesFunction() {}
  virtual bool RunSync() OVERRIDE;
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

class SessionsRestoreFunction : public ChromeSyncExtensionFunction {
 protected:
  virtual ~SessionsRestoreFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("sessions.restore", SESSIONS_RESTORE)

 private:
  void SetInvalidIdError(const std::string& invalid_id);
  void SetResultRestoredTab(content::WebContents* contents);
  bool SetResultRestoredWindow(int window_id);
  bool RestoreMostRecentlyClosed(Browser* browser);
  bool RestoreLocalSession(const SessionId& session_id, Browser* browser);
  bool RestoreForeignSession(const SessionId& session_id,
                             Browser* browser);
};

class SessionsEventRouter : public TabRestoreServiceObserver {
 public:
  explicit SessionsEventRouter(Profile* profile);
  virtual ~SessionsEventRouter();

  // Observer callback for TabRestoreServiceObserver. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

 private:
  Profile* profile_;

  // TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(SessionsEventRouter);
};

class SessionsAPI : public BrowserContextKeyedAPI,
                    public extensions::EventRouter::Observer {
 public:
  explicit SessionsAPI(content::BrowserContext* context);
  virtual ~SessionsAPI();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<SessionsAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SessionsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<SessionsEventRouter> sessions_event_router_;

  DISALLOW_COPY_AND_ASSIGN(SessionsAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
