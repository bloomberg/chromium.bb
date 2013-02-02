// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace {

// Records notifications, but only for extensions with specific IDs.
class FilteringNotificationObserver : public content::NotificationObserver {
 public:
  FilteringNotificationObserver(
      content::NotificationSource source,
      const std::set<std::string>& extension_ids)
      : extension_ids_(extension_ids) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED, source);
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED, source);
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED, source);
  }

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(chrome::NotificationType type) {
    return CheckNotifications(std::vector<chrome::NotificationType>(1, type));
  }

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(chrome::NotificationType t1,
                                              chrome::NotificationType t2) {
    std::vector<chrome::NotificationType> types;
    types.push_back(t1);
    types.push_back(t2);
    return CheckNotifications(types);
  }

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(chrome::NotificationType t1,
                                              chrome::NotificationType t2,
                                              chrome::NotificationType t3) {
    std::vector<chrome::NotificationType> types;
    types.push_back(t1);
    types.push_back(t2);
    types.push_back(t3);
    return CheckNotifications(types);
  }

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(chrome::NotificationType t1,
                                              chrome::NotificationType t2,
                                              chrome::NotificationType t3,
                                              chrome::NotificationType t4,
                                              chrome::NotificationType t5,
                                              chrome::NotificationType t6) {
    std::vector<chrome::NotificationType> types;
    types.push_back(t1);
    types.push_back(t2);
    types.push_back(t3);
    types.push_back(t4);
    types.push_back(t5);
    types.push_back(t6);
    return CheckNotifications(types);
  }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (extension_ids_.count(extension->id()))
          notifications_.push_back(static_cast<chrome::NotificationType>(type));
        break;
      }

      case chrome::NOTIFICATION_EXTENSION_LOADED: {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (extension_ids_.count(extension->id()))
          notifications_.push_back(static_cast<chrome::NotificationType>(type));
        break;
      }

      case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
        UnloadedExtensionInfo* reason =
            content::Details<UnloadedExtensionInfo>(details).ptr();
        if (extension_ids_.count(reason->extension->id())) {
          notifications_.push_back(static_cast<chrome::NotificationType>(type));
          // The only way that extensions are unloaded in these tests is
          // by blacklisting.
          EXPECT_EQ(extension_misc::UNLOAD_REASON_BLACKLIST,
                    reason->reason);
        }
        break;
      }

      default:
        NOTREACHED();
        break;
    }
  }

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(
      const std::vector<chrome::NotificationType>& types) {
    testing::AssertionResult result = (notifications_ == types) ?
        testing::AssertionSuccess() :
        testing::AssertionFailure() << "Expected " << Str(types) << ", " <<
                                       "Got " << Str(notifications_);
    notifications_.clear();
    return result;
  }

  std::string Str(const std::vector<chrome::NotificationType>& types) {
    std::string str = "[";
    bool needs_comma = false;
    for (std::vector<chrome::NotificationType>::const_iterator it =
         types.begin(); it != types.end(); ++it) {
      if (needs_comma)
        str += ",";
      needs_comma = true;
      str += base::StringPrintf("%d", *it);
    }
    return str + "]";
  }

  const std::set<std::string> extension_ids_;

  std::vector<chrome::NotificationType> notifications_;

  content::NotificationRegistrar registrar_;
};

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
  FilteringNotificationObserver notifications(
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

  ASSERT_TRUE(extension_a);
  ASSERT_TRUE(extension_b);
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

  EXPECT_TRUE(IsSafe(extension_a));
  EXPECT_TRUE(IsSafe(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));

  // Blacklist a and b.
  blacklist()->SetFromUpdater(vector_ab, "1");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Un-blacklist a.
  blacklist()->SetFromUpdater(vector_b, "2");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_LOADED));

  // Blacklist a then switch with c.
  blacklist()->SetFromUpdater(vector_ab, "3");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  blacklist()->SetFromUpdater(vector_bc, "4");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Add a to blacklist.
  blacklist()->SetFromUpdater(vector_abc, "5");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED));

  // Clear blacklist.
  blacklist()->SetFromUpdater(empty_vector, "6");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsSafe(extension_a));
  EXPECT_TRUE(IsSafe(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_LOADED,
      chrome::NOTIFICATION_EXTENSION_LOADED));

  // Add a and b back again for the next test.
  blacklist()->SetFromUpdater(vector_ab, "7");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      chrome::NOTIFICATION_EXTENSION_UNLOADED));
}

// Stage 2: blacklisting with extensions A and B having been installed,
// with A actually in the blacklist.
IN_PROC_BROWSER_TEST_F(ExtensionBlacklistBrowserTest, Blacklist) {
  FilteringNotificationObserver notifications(
      content::Source<Profile>(profile()), GetTestExtensionIDs());

  scoped_refptr<const Extension> extension_a =
      extension_service()->blacklisted_extensions()->GetByID(info_a_.id());
  ASSERT_TRUE(extension_a);

  scoped_refptr<const Extension> extension_b =
      extension_service()->blacklisted_extensions()->GetByID(info_b_.id());
  ASSERT_TRUE(extension_b);

  scoped_refptr<const Extension> extension_c =
      extension_service()->extensions()->GetByID(info_c_.id());
  ASSERT_TRUE(extension_c);

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsSafe(extension_c));

  // Make sure that we can still blacklist c and unblacklist b.
  std::vector<std::string> vector_ac(1, extension_a->id());
  vector_ac.push_back(extension_c->id());
  blacklist()->SetFromUpdater(vector_ac, "8");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsSafe(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_TRUE(notifications.CheckNotifications(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        chrome::NOTIFICATION_EXTENSION_UNLOADED));
}

}  // namespace extensions
