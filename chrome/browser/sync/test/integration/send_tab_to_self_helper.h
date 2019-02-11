// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_

#include <string>
#include <vector>

#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "url/gurl.h"

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace send_tab_to_self_helper {

// Class that allows waiting until a particular |url| is exposed by the
// SendTabToSelfModel in|service|.
class SendTabToSelfUrlChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  SendTabToSelfUrlChecker(send_tab_to_self::SendTabToSelfSyncService* service,
                          const GURL& url);
  ~SendTabToSelfUrlChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied() override;
  std::string GetDebugMessage() const override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void SendTabToSelfEntriesAdded(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void SendTabToSelfEntriesRemoved(
      const std::vector<std::string>& guids_removed) override;

 private:
  const GURL url_;
  send_tab_to_self::SendTabToSelfSyncService* const service_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfUrlChecker);
};

// Class that allows waiting the number of entries in until |service0|
// matches the number of entries in |service1|.
class SendTabToSelfModelEqualityChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  SendTabToSelfModelEqualityChecker(
      send_tab_to_self::SendTabToSelfSyncService* service0,
      send_tab_to_self::SendTabToSelfSyncService* service1);
  ~SendTabToSelfModelEqualityChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied() override;
  std::string GetDebugMessage() const override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void SendTabToSelfEntriesAdded(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void SendTabToSelfEntriesRemoved(
      const std::vector<std::string>& guids_removed) override;

 private:
  send_tab_to_self::SendTabToSelfSyncService* const service0_;
  send_tab_to_self::SendTabToSelfSyncService* const service1_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelEqualityChecker);
};

}  // namespace send_tab_to_self_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_
