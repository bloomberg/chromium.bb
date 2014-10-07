// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_IN_MEMORY_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_IN_MEMORY_TAB_RESTORE_SERVICE_H_

#include <vector>

#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_helper.h"

// Tab restore service that doesn't persist tabs on disk. This is used on
// Android where tabs persistence is implemented on the application side in
// Java. Other platforms should use PersistentTabRestoreService which can be
// instantiated through the TabRestoreServiceFactory.
class InMemoryTabRestoreService : public TabRestoreService {
 public:
  // Creates a new TabRestoreService and provides an object that provides the
  // current time. The TabRestoreService does not take ownership of
  // |time_factory|.
  InMemoryTabRestoreService(Profile* profile,
                            TimeFactory* time_factory);

  virtual ~InMemoryTabRestoreService();

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
  virtual std::vector<content::WebContents*>
    RestoreEntryById(TabRestoreServiceDelegate* delegate,
                     SessionID::id_type id,
                     chrome::HostDesktopType host_desktop_type,
                     WindowOpenDisposition disposition) override;
  virtual void LoadTabsFromLastSession() override;
  virtual bool IsLoaded() const override;
  virtual void DeleteLastSession() override;
  virtual void Shutdown() override;

 private:
  TabRestoreServiceHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryTabRestoreService);
};

#endif  // CHROME_BROWSER_SESSIONS_IN_MEMORY_TAB_RESTORE_SERVICE_H_
