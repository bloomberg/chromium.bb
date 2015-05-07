// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "grit/theme_resources.h"
#include "net/test/url_request/url_request_slow_download_job.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "url/gurl.h"

namespace {

enum {
  DUMMY_ACCOUNT_INDEX = 0,
  PRIMARY_ACCOUNT_INDEX = 1,
  SECONDARY_ACCOUNT_INDEX_START = 2,
};

// Structure to describe an account info.
struct TestAccountInfo {
  const char* const email;
  const char* const gaia_id;
  const char* const hash;
  const char* const display_name;
};

// Accounts for multi profile test.
static const TestAccountInfo kTestAccounts[] = {
    {"__dummy__@invalid.domain", "10000", "hashdummy", "Dummy Account"},
    {"alice@invalid.domain",     "10001", "hashalice", "Alice"},
    {"bob@invalid.domain",       "10002", "hashbobbo", "Bob"},
    {"charlie@invalid.domain",   "10003", "hashcharl", "Charlie"},
};

// Base class observing notification events.
class MessageCenterChangeObserver
    : public message_center::MessageCenterObserver {
 public:
  MessageCenterChangeObserver() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }

  ~MessageCenterChangeObserver() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

 protected:
  void RunLoop() {
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    run_loop_.Run();
  }

  void QuitRunLoop() {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterChangeObserver);
};

// Class observing of "ADD" notification events.
class NotificationAddObserver : public MessageCenterChangeObserver {
 public:
  NotificationAddObserver() : count_(1) {
    MessageCenterChangeObserver();
  }
  explicit NotificationAddObserver(int count) : count_(count) {
    MessageCenterChangeObserver();
  }
  ~NotificationAddObserver() override {}

  bool Wait() {
    if (count_ <= 0)
      return count_ == 0;

    waiting_ = true;
    RunLoop();
    waiting_ = false;
    return count_ == 0;
  }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    count_--;

    if (notification_id_.empty())
      notification_id_ = notification_id;

    if (waiting_)
      QuitRunLoop();
  }

  std::string notification_id() { return notification_id_; }

 private:
  std::string notification_id_;
  bool waiting_ = false;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(NotificationAddObserver);
};

// Class observing of "UPDATE" notification events.
class NotificationUpdateObserver : public MessageCenterChangeObserver {
 public:
  NotificationUpdateObserver() : waiting_(false) {
    MessageCenterChangeObserver();
  }
  ~NotificationUpdateObserver() override {}

  std::string Wait() {
    if (!notification_id_.empty())
      return notification_id_;

    waiting_ = true;
    RunLoop();
    waiting_ = false;
    return notification_id_;
  }

  void OnNotificationUpdated(const std::string& notification_id) override {
    if (notification_id_.empty()) {
      notification_id_ = notification_id;

      if (waiting_)
        QuitRunLoop();
    }
  }

 private:
  std::string notification_id_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUpdateObserver);
};

class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile), opened_(false) {}
  ~TestChromeDownloadManagerDelegate() override {}

  // ChromeDownloadManagerDelegate override:
  void OpenDownload(content::DownloadItem* item) override { opened_ = true; }

  // Return if  the download is opened.
  bool opened() const { return opened_; }

 private:
  bool opened_;
};

// Utility method to retrieve a message center.
message_center::MessageCenter* GetMessageCenter() {
  return message_center::MessageCenter::Get();
}

// Utility method to retrieve a notification object by id.
message_center::Notification* GetNotification(const std::string& id) {
  return GetMessageCenter()->FindVisibleNotificationById(id);
}

}  // anonnymous namespace

// Base class for tests
class DownloadNotificationTestBase : public InProcessBrowserTest {
 public:
  ~DownloadNotificationTestBase() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(yoshiki): Remove this after the download notification launches.
    command_line->AppendSwitch(switches::kEnableDownloadNotification);
  }

  void SetUpOnMainThread() override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&net::URLRequestSlowDownloadJob::AddUrlHandler));
  }

  content::DownloadManager* GetDownloadManager(Browser* browser) {
    return content::BrowserContext::GetDownloadManager(browser->profile());
  }
};

//////////////////////////////////////////////////
// Test with a single profile
//////////////////////////////////////////////////

class DownloadNotificationTest : public DownloadNotificationTestBase {
 public:
  ~DownloadNotificationTest() override {}

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();

    scoped_ptr<TestChromeDownloadManagerDelegate> test_delegate;
    test_delegate.reset(new TestChromeDownloadManagerDelegate(profile));
    test_delegate->GetDownloadIdReceiverCallback().Run(
        content::DownloadItem::kInvalidId + 1);

    DownloadServiceFactory::GetForBrowserContext(profile)
        ->SetDownloadManagerDelegateForTesting(test_delegate.Pass());

    DownloadNotificationTestBase::SetUpOnMainThread();
  }

  TestChromeDownloadManagerDelegate* GetDownloadManagerDelegate() const {
    return static_cast<TestChromeDownloadManagerDelegate*>(
        DownloadServiceFactory::GetForBrowserContext(browser()->profile())
            ->GetDownloadManagerDelegate());
  }
};

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadFile) {
  CreateDownload();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id())->type());

  // Requests to complete the download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for download completion.
  while (download_item()->GetState() != content::DownloadItem::COMPLETE) {
    NotificationUpdateObserver download_change_notification_observer;
    download_change_notification_observer.Wait();
  }

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id())->type());

  // Try to open the downloaded item by clicking the notification.
  EXPECT_FALSE(GetDownloadManagerDelegate()->opened());
  GetMessageCenter()->ClickOnNotification(notification_id());
  EXPECT_TRUE(GetDownloadManagerDelegate()->opened());

  EXPECT_FALSE(GetNotification(notification_id()));
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       CloseNotificationAfterDownload) {
  CreateDownload();

  // Requests to complete the download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for download completion.
  while (download_item()->GetState() != content::DownloadItem::COMPLETE) {
    NotificationUpdateObserver download_change_notification_observer;
    download_change_notification_observer.Wait();
  }

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);

  // Closes the notification.
  NotificationRemoveObserver notification_close_observer;
  GetMessageCenter()->RemoveNotification(notification_id(), true /* by_user */);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());

  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is also started.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::COMPLETE, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       CloseNotificationWhileDownloading) {
  CreateDownload();

  // Closes the notification.
  NotificationRemoveObserver notification_close_observer;
  GetMessageCenter()->RemoveNotification(notification_id(), true /* by_user */);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());

  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is still in progress.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Cleans the downloading.
  downloads[0]->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadRemoved) {
  CreateDownload();

  NotificationRemoveObserver notification_close_observer;
  download_item()->Remove();
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());

  // Confirms that the notification is removed.
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that the download item is removed.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadMultipleFiles) {
  GURL url1(net::URLRequestSlowDownloadJob::kUnknownSizeUrl);
  GURL url2(net::URLRequestSlowDownloadJob::kKnownSizeUrl);

  // Starts the 1st download.
  NotificationAddObserver download_start_notification_observer1;
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_TRUE(download_start_notification_observer1.Wait());
  std::string notification_id1 =
      download_start_notification_observer1.notification_id();
  EXPECT_FALSE(notification_id1.empty());

  // Confirms that there is a download.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  content::DownloadItem* download1or2 = downloads[0];

  // Starts the 2nd download.
  NotificationAddObserver download_start_notification_observer2;
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_TRUE(download_start_notification_observer2.Wait());
  std::string notification_id2 =
      download_start_notification_observer2.notification_id();
  EXPECT_FALSE(notification_id2.empty());

  // Confirms that there are 2 downloads.
  downloads.clear();
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  content::DownloadItem* download1 = downloads[0];
  content::DownloadItem* download2 = downloads[1];
  EXPECT_EQ(2u, downloads.size());
  EXPECT_NE(download1, download2);
  EXPECT_TRUE(download1 == download1or2 || download2 == download1or2);

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id2)->type());

  // Requests to complete the downloads.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for the completion of downloads.
  while (download1->GetState() != content::DownloadItem::COMPLETE ||
         download2->GetState() != content::DownloadItem::COMPLETE) {
    NotificationUpdateObserver download_change_notification_observer;
    download_change_notification_observer.Wait();
  }

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id2)->type());
}

//////////////////////////////////////////////////
// Test with multi profiles
//////////////////////////////////////////////////

class MultiProfileDownloadNotificationTest
    : public DownloadNotificationTestBase {
 public:
  ~MultiProfileDownloadNotificationTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DownloadNotificationTestBase::SetUpCommandLine(command_line);

    // Logs in to a dummy profile.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].email);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].hash);
  }

  // Logs in to the primary profile.
  void SetUpOnMainThread() override {
    const TestAccountInfo& info = kTestAccounts[PRIMARY_ACCOUNT_INDEX];

    AddUser(info, true);
    DownloadNotificationTestBase::SetUpOnMainThread();
  }

  // Loads all users to the current session and sets up necessary fields.
  // This is used for preparing all accounts in PRE_ test setup, and for testing
  // actual login behavior.
  void AddAllUsers() {
    for (size_t i = 0; i < arraysize(kTestAccounts); ++i)
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
  }

  Profile* GetProfileByIndex(int index) {
    return chromeos::ProfileHelper::GetProfileByUserIdHash(
        kTestAccounts[index].hash);
  }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    if (log_in)
      user_manager->UserLoggedIn(info.email, info.hash, false);
    user_manager->SaveUserDisplayName(info.email,
                                      base::UTF8ToUTF16(info.display_name));
    SigninManagerFactory::GetForProfile(
        chromeos::ProfileHelper::GetProfileByUserIdHash(info.hash))
            ->SetAuthenticatedAccountInfo(info.gaia_id, info.email);
  }
};

IN_PROC_BROWSER_TEST_F(MultiProfileDownloadNotificationTest,
                       PRE_DownloadMultipleFiles) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileDownloadNotificationTest,
                       DownloadMultipleFiles) {
  AddAllUsers();

  GURL url(net::URLRequestSlowDownloadJob::kUnknownSizeUrl);

  Profile* profile1 = GetProfileByIndex(1);
  Profile* profile2 = GetProfileByIndex(2);
  Browser* browser1 = CreateBrowser(profile1);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_NE(browser1, browser2);

  // First user starts a download.
  NotificationAddObserver download_start_notification_observer1;
  ui_test_utils::NavigateToURL(browser1, url);
  download_start_notification_observer1.Wait();

  // Confirms that the download is started.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser1)->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  content::DownloadItem* download1 = downloads[0];

  // Confirms that a download notification is generated.
  std::string notification_id1 =
      download_start_notification_observer1.notification_id();
  EXPECT_FALSE(notification_id1.empty());

  // Second user starts a download.
  NotificationAddObserver download_start_notification_observer2;
  ui_test_utils::NavigateToURL(browser2, url);
  download_start_notification_observer2.Wait();
  std::string notification_id2 =
      download_start_notification_observer2.notification_id();
  EXPECT_FALSE(notification_id2.empty());

  // Confirms that the second user has only 1 download.
  downloads.clear();
  GetDownloadManager(browser2)->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  // Second user starts another download.
  NotificationAddObserver download_start_notification_observer3;
  ui_test_utils::NavigateToURL(browser2, url);
  download_start_notification_observer3.Wait();
  std::string notification_id3 =
      download_start_notification_observer3.notification_id();
  EXPECT_FALSE(notification_id3.empty());

  // Confirms that the second user has 2 downloads.
  downloads.clear();
  GetDownloadManager(browser2)->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());
  content::DownloadItem* download2 = downloads[0];
  content::DownloadItem* download3 = downloads[1];
  EXPECT_NE(download1, download2);
  EXPECT_NE(download1, download3);
  EXPECT_NE(download2, download3);

  // Confirms that the first user still has only 1 download.
  downloads.clear();
  GetDownloadManager(browser1)->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(download1, downloads[0]);

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id2)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id3)->type());

  // Requests to complete the downloads.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for the completion of downloads.
  while (download1->GetState() != content::DownloadItem::COMPLETE ||
         download2->GetState() != content::DownloadItem::COMPLETE ||
         download3->GetState() != content::DownloadItem::COMPLETE) {
    NotificationUpdateObserver download_change_notification_observer;
    download_change_notification_observer.Wait();
  }

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id2)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id3)->type());
}
