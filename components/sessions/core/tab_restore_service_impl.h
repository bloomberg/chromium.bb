// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_helper.h"

class PrefService;
class TabRestoreServiceImplTest;

namespace sessions {

// Tab restore service that persists data on disk.
class SESSIONS_EXPORT TabRestoreServiceImpl : public TabRestoreService {
 public:
  // Does not take ownership of |time_factory|.
  TabRestoreServiceImpl(std::unique_ptr<TabRestoreServiceClient> client,
                        PrefService* pref_service,
                        TimeFactory* time_factory);

  ~TabRestoreServiceImpl() override;

  // TabRestoreService:
  void AddObserver(TabRestoreServiceObserver* observer) override;
  void RemoveObserver(TabRestoreServiceObserver* observer) override;
  void CreateHistoricalTab(LiveTab* live_tab, int index) override;
  void BrowserClosing(LiveTabContext* context) override;
  void BrowserClosed(LiveTabContext* context) override;
  void ClearEntries() override;
  void DeleteNavigationEntries(const DeletionPredicate& predicate) override;
  const Entries& entries() const override;
  std::vector<LiveTab*> RestoreMostRecentEntry(
      LiveTabContext* context) override;
  std::unique_ptr<Tab> RemoveTabEntryById(SessionID id) override;
  std::vector<LiveTab*> RestoreEntryById(
      LiveTabContext* context,
      SessionID id,
      WindowOpenDisposition disposition) override;
  void LoadTabsFromLastSession() override;
  bool IsLoaded() const override;
  void DeleteLastSession() override;
  bool IsRestoring() const override;
  void Shutdown() override;

 private:
  friend class ::TabRestoreServiceImplTest;

  class PersistenceDelegate;
  void UpdatePersistenceDelegate();

  // Exposed for testing.
  Entries* mutable_entries();
  void PruneEntries();

  std::unique_ptr<TabRestoreServiceClient> client_;
  std::unique_ptr<PersistenceDelegate> persistence_delegate_;
  TabRestoreServiceHelper helper_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreServiceImpl);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_
