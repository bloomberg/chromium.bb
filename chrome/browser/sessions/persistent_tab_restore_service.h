// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_PERSISTENT_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_PERSISTENT_TAB_RESTORE_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_helper.h"

class Profile;

// Tab restore service that persists data on disk.
class PersistentTabRestoreService : public TabRestoreService {
 public:
  // Does not take ownership of |time_factory|.
  PersistentTabRestoreService(Profile* profile,
                              TimeFactory* time_factory);

  ~PersistentTabRestoreService() override;

  // TabRestoreService:
  void AddObserver(TabRestoreServiceObserver* observer) override;
  void RemoveObserver(TabRestoreServiceObserver* observer) override;
  void CreateHistoricalTab(content::WebContents* contents, int index) override;
  void BrowserClosing(TabRestoreServiceDelegate* delegate) override;
  void BrowserClosed(TabRestoreServiceDelegate* delegate) override;
  void ClearEntries() override;
  const Entries& entries() const override;
  std::vector<content::WebContents*> RestoreMostRecentEntry(
      TabRestoreServiceDelegate* delegate,
      chrome::HostDesktopType host_desktop_type) override;
  Tab* RemoveTabEntryById(SessionID::id_type id) override;
  std::vector<content::WebContents*> RestoreEntryById(
      TabRestoreServiceDelegate* delegate,
      SessionID::id_type id,
      chrome::HostDesktopType host_desktop_type,
      WindowOpenDisposition disposition) override;
  void LoadTabsFromLastSession() override;
  bool IsLoaded() const override;
  void DeleteLastSession() override;
  void Shutdown() override;

 private:
  friend class PersistentTabRestoreServiceTest;

  class Delegate;

  // Exposed for testing.
  Entries* mutable_entries();
  void PruneEntries();

  scoped_ptr<Delegate> delegate_;
  TabRestoreServiceHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(PersistentTabRestoreService);
};

#endif  // CHROME_BROWSER_SESSIONS_PERSISTENT_TAB_RESTORE_SERVICE_H_
