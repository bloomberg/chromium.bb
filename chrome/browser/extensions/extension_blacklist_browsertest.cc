// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_notification_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

namespace extensions {

namespace {

// Stores the paths to CRX files of extensions, and the extension's ID.
// Use arbitrary extensions; we're just testing blacklisting behavior.
class CrxInfo {
 public:
  CrxInfo(const std::string& path, const std::string& id)
      : path_(path), id_(id) {}

  const std::string& path() { return path_; }
  const std::string& id() { return id_; }

 private:
  const std::string path_;
  const std::string id_;
};

}  // namespace

class ExtensionBlacklistBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionBlacklistBrowserTest()
      : info_a_("install/install.crx", "ogdbpbegnmindpdjfafpmpicikegejdj"),
        info_b_("autoupdate/v1.crx",   "ogjcoiohnmldgjemafoockdghcjciccf"),
        info_c_("hosted_app.crx",      "kbmnembihfiondgfjekmnmcbddelicoi"),
        // Just disable the safe browsing altogether.
        // TODO(kalman): a different approach will be needed when the blacklist
        // comes entirely from safe browsing.
        scoped_blacklist_database_manager_(
            scoped_refptr<SafeBrowsingDatabaseManager>(NULL)) {}

  virtual ~ExtensionBlacklistBrowserTest() {}

 protected:
  // Returns whether |extension| is strictly safe: in one of ExtensionService's
  // non-blacklisted extension sets, and not in its blacklisted extensions.
  testing::AssertionResult IsSafe(const Extension* extension) {
    std::string id = extension->id();
    int include_mask =  ExtensionService::INCLUDE_EVERYTHING &
                       ~ExtensionService::INCLUDE_BLACKLISTED;
    if (!extension_service()->GetExtensionById(id, include_mask))
      return testing::AssertionFailure() << id << " is safe";
    return IsInValidState(extension);
  }

  // Returns whether |extension| is strictly blacklisted: in ExtensionService's
  // blacklist, and not in any of its other extension sets.
  testing::AssertionResult IsBlacklisted(const Extension* extension) {
    std::string id = extension->id();
    if (!extension_service()->blacklisted_extensions()->Contains(id))
      return testing::AssertionFailure() << id << " is not blacklisted";
    return IsInValidState(extension);
  }

  std::set<std::string> GetTestExtensionIDs() {
    std::set<std::string> extension_ids;
    extension_ids.insert(info_a_.id());
    extension_ids.insert(info_b_.id());
    extension_ids.insert(info_c_.id());
    return extension_ids;
  }

  Blacklist* blacklist() {
    return ExtensionSystem::Get(profile())->blacklist();
  }

  CrxInfo info_a_;
  CrxInfo info_b_;
  CrxInfo info_c_;

 private:
  // Returns whether |extension| is either installed or blacklisted, but
  // neither both nor neither.
  testing::AssertionResult IsInValidState(const Extension* extension) {
    std::string id = extension->id();
    bool is_blacklisted =
        extension_service()->blacklisted_extensions()->Contains(id);
    int safe_mask =  ExtensionService::INCLUDE_EVERYTHING &
                     ~ExtensionService::INCLUDE_BLACKLISTED;
    bool is_safe = extension_service()->GetExtensionById(id, safe_mask) != NULL;
    if (is_blacklisted && is_safe) {
      return testing::AssertionFailure() <<
          id << " is both safe and in blacklisted_extensions";
    }
    if (!is_blacklisted && !is_safe) {
      return testing::AssertionFailure() <<
          id << " is neither safe nor in blacklisted_extensions";
    }
    return testing::AssertionSuccess();
  }

  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_database_manager_;
};

// Stage 1: blacklisting when there weren't any extensions installed when the
// browser started.
IN_PROC_BROWSER_TEST_F(ExtensionBlacklistBrowserTest, PRE_Blacklist) {
  ExtensionNotificationObserver notifications(
      content::NotificationService::AllSources(), GetTestExtensionIDs());

  scoped_refptr<const Extension> extension_a =
      InstallExtension(test_data_dir_.AppendASCII(info_a_.path()), 1);
  scoped_refptr<const Extension> extension_b =
      InstallExtension(test_data_dir_.AppendASCII(info_b_.path()), 1);
  scoped_refptr<const Extension> extension_c =
      InstallExtension(test_data_dir_.AppendASCII(info_c_.path()), 1);

  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      chrome::NOTIFICATION_EXTENSION_LOADED));

  ASSERT_TRUE(extension_a.get());
  ASSERT_TRUE(extension_b.get());
  ASSERT_EQ(info_a_.id(), extension_a->id());
  ASSERT_EQ(info_b_.id(), extension_b->id());
  ASSERT_EQ(info_c_.id(), extension_c->id());

  std::vector<std::string> empty_vector;
  std::vector<std::string> vector_a(1, info_a_.id());
  std::vector<std::string> vector_b(1, info_b_.id());
  std::vector<std::string> vector_c(1, info_c_.id());
  std::vector<std::string> vector_ab(1, info_a_.id());
  vector_ab.push_back(info_b_.id());
  std::vector<std::string> vector_bc(1, info_b_.id());
  vector_bc.push_back(info_c_.id());
  std::vector<std::string> vector_abc(1, info_a_.id());
  vector_abc.push_back(info_b_.id());
  vector_abc.push_back(info_c_.id());

  EXPECT_TRUE(IsSafe(extension_a.get()));
  EXPECT_TRUE(IsSafe(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));

  // Blacklist a and b.
  blacklist()->SetFromUpdater(vector_ab, "1");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Un-blacklist a.
  blacklist()->SetFromUpdater(vector_b, "2");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));
  EXPECT_TRUE(
      notifications.CheckNotifications(chrome::NOTIFICATION_EXTENSION_LOADED));

  // Blacklist a then switch with c.
  blacklist()->SetFromUpdater(vector_ab, "3");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  blacklist()->SetFromUpdater(vector_bc, "4");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsBlacklisted(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Add a to blacklist.
  blacklist()->SetFromUpdater(vector_abc, "5");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsBlacklisted(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Clear blacklist.
  blacklist()->SetFromUpdater(empty_vector, "6");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a.get()));
  EXPECT_TRUE(IsSafe(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));
  EXPECT_TRUE(
      notifications.CheckNotifications(chrome::NOTIFICATION_EXTENSION_LOADED,
                                       chrome::NOTIFICATION_EXTENSION_LOADED,
                                       chrome::NOTIFICATION_EXTENSION_LOADED));

  // Add a and b back again for the next test.
  blacklist()->SetFromUpdater(vector_ab, "7");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));
}

// Stage 2: blacklisting with extensions A and B having been installed,
// with A actually in the blacklist.
IN_PROC_BROWSER_TEST_F(ExtensionBlacklistBrowserTest, Blacklist) {
  ExtensionNotificationObserver notifications(
      content::Source<Profile>(profile()), GetTestExtensionIDs());

  scoped_refptr<const Extension> extension_a =
      extension_service()->blacklisted_extensions()->GetByID(info_a_.id());
  ASSERT_TRUE(extension_a.get());

  scoped_refptr<const Extension> extension_b =
      extension_service()->blacklisted_extensions()->GetByID(info_b_.id());
  ASSERT_TRUE(extension_b.get());

  scoped_refptr<const Extension> extension_c =
      extension_service()->extensions()->GetByID(info_c_.id());
  ASSERT_TRUE(extension_c.get());

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsBlacklisted(extension_b.get()));
  EXPECT_TRUE(IsSafe(extension_c.get()));

  // Make sure that we can still blacklist c and unblacklist b.
  std::vector<std::string> vector_ac(1, extension_a->id());
  vector_ac.push_back(extension_c->id());
  blacklist()->SetFromUpdater(vector_ac, "8");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a.get()));
  EXPECT_TRUE(IsSafe(extension_b.get()));
  EXPECT_TRUE(IsBlacklisted(extension_c.get()));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));
}

}  // namespace extensions
