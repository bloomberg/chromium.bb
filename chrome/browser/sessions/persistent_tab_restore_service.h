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

  virtual ~PersistentTabRestoreService();

  // TabRestoreService:
  virtual void AddObserver(TabRestoreServiceObserver* observer) override;
  virtual void RemoveObserver(TabRestoreServiceObserver* observer) override;
  virtual void CreateHistoricalTab(content::WebContents* contents,
                                   int index) override;
  virtual void BrowserClosing(TabRestoreServiceDelegate* delegate) override;
  virtual void BrowserClosed(TabRestoreServiceDelegate* delegate) override;
  virtual void ClearEntries() override;
  virtual const Entries& entries() const override;
  virtual std::vector<content::WebContents*> RestoreMostRecentEntry(
      TabRestoreServiceDelegate* delegate,
      chrome::HostDesktopType host_desktop_type) override;
  virtual Tab* RemoveTabEntryById(SessionID::id_type id) override;
  virtual std::vector<content::WebContents*> RestoreEntryById(
      TabRestoreServiceDelegate* delegate,
      SessionID::id_type id,
      chrome::HostDesktopType host_desktop_type,
      WindowOpenDisposition disposition) override;
  virtual void LoadTabsFromLastSession() override;
  virtual bool IsLoaded() const override;
  virtual void DeleteLastSession() override;
  virtual void Shutdown() override;

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
