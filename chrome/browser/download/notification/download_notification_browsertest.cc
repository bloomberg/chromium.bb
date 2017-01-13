// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

template <typename T>
bool IsInNotifications(
    const T& notifications,
    const std::string& id) {
  for (const auto& notification : notifications) {
    if (notification->id() == id)
      return true;
  }
  return false;
}

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
    run_loop_->Run();
  }

  void QuitRunLoop() {
    run_loop_->Quit();
  }

  void ResetRunLoop() {
    run_loop_.reset(new base::RunLoop);
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterChangeObserver);
};

// Class observing of "ADD" notification events.
class NotificationAddObserver : public MessageCenterChangeObserver {
 public:
  NotificationAddObserver() : count_(1) {}
  explicit NotificationAddObserver(int count) : count_(count) {
    MessageCenterChangeObserver();
  }
  ~NotificationAddObserver() override {}

  bool Wait() {
    if (count_ <= 0)
      return count_ == 0;

    waiting_ = true;
    ResetRunLoop();
    RunLoop();
    waiting_ = false;
    return count_ == 0;
  }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    count_--;

    notification_ids_.push_back(notification_id);

    if (waiting_ && count_ == 0)
      QuitRunLoop();
  }

  const std::string& notification_id() const { return notification_ids_.at(0); }
  const std::vector<std::string>& notification_ids() const {
    return notification_ids_;
  }

 private:
  std::vector<std::string> notification_ids_;
  bool waiting_ = false;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(NotificationAddObserver);
};

// Class observing of "UPDATE" notification events.
class NotificationUpdateObserver : public MessageCenterChangeObserver {
 public:
  NotificationUpdateObserver() {}
  explicit NotificationUpdateObserver(const std::string& id)
      : target_notification_id_(id) {}
  ~NotificationUpdateObserver() override {}

  std::string Wait() {
    if (!notification_id_.empty())
      return notification_id_;

    waiting_ = true;
    ResetRunLoop();
    RunLoop();
    waiting_ = false;

    std::string notification_id(notification_id_);
    notification_id_.clear();
    return notification_id;
  }

  void OnNotificationUpdated(const std::string& notification_id) override {
    if (notification_id_.empty()) {
      if (!target_notification_id_.empty() &&
          target_notification_id_ != notification_id) {
        return;
      }

      notification_id_ = notification_id;

      if (waiting_)
        QuitRunLoop();
    }
  }

  void Reset() {
    notification_id_.clear();
  }

 private:
  std::string notification_id_;
  std::string target_notification_id_;
  bool waiting_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationUpdateObserver);
};

// Class observing of "REMOVE" notification events.
class NotificationRemoveObserver : public MessageCenterChangeObserver {
 public:
  NotificationRemoveObserver() {}
  ~NotificationRemoveObserver() override {}

  std::string Wait() {
    if (!notification_id_.empty())
      return notification_id_;

    waiting_ = true;
    ResetRunLoop();
    RunLoop();
    waiting_ = false;
    return notification_id_;
  }

  // message_center::MessageCenterObserver:
  void OnNotificationRemoved(
      const std::string& notification_id, bool by_user) override {
    if (notification_id_.empty()) {
      notification_id_ = notification_id;

      if (waiting_)
        QuitRunLoop();
    }
  }

 private:
  std::string notification_id_;
  bool waiting_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationRemoveObserver);
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

 protected:
  // Disable DownloadProtectionService in order to disable content checking.
  safe_browsing::DownloadProtectionService* GetDownloadProtectionService()
      override {
    return nullptr;
  }

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

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&net::URLRequestSlowDownloadJob::AddUrlHandler));

    GetMessageCenter()->DisableTimersForTest();

    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ASSERT_TRUE(SetDownloadsDirectory(browser()));
  }

 protected:
  // Must be called after browser creation. Assumes that |downloads_directory_|
  // is created and sets its path to be used for downloads by |browser|.
  // Returning false indicates a failure of the function, and should be
  // asserted in the caller.
  bool SetDownloadsDirectory(Browser* browser) {
    if (!browser)
      return false;

    browser->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, downloads_directory_.GetPath());
    browser->profile()->GetPrefs()->SetFilePath(
        prefs::kSaveFileDefaultDirectory, downloads_directory_.GetPath());

    return true;
  }

  content::DownloadManager* GetDownloadManager(Browser* browser) {
    return content::BrowserContext::GetDownloadManager(browser->profile());
  }

 private:
  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
};

//////////////////////////////////////////////////
// Test with a single profile
//////////////////////////////////////////////////

class DownloadNotificationTest : public DownloadNotificationTestBase {
 public:
  ~DownloadNotificationTest() override {}

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();

    std::unique_ptr<TestChromeDownloadManagerDelegate> test_delegate;
    test_delegate.reset(new TestChromeDownloadManagerDelegate(profile));
    test_delegate->GetDownloadIdReceiverCallback().Run(
        content::DownloadItem::kInvalidId + 1);
    DownloadServiceFactory::GetForBrowserContext(profile)
        ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));

    DownloadNotificationTestBase::SetUpOnMainThread();
  }

  TestChromeDownloadManagerDelegate* GetDownloadManagerDelegate() const {
    return static_cast<TestChromeDownloadManagerDelegate*>(
        DownloadServiceFactory::GetForBrowserContext(browser()->profile())
            ->GetDownloadManagerDelegate());
  }

  void PrepareIncognitoBrowser() {
    incognito_browser_ = CreateIncognitoBrowser();
    Profile* incognito_profile = incognito_browser_->profile();

    ASSERT_TRUE(SetDownloadsDirectory(incognito_browser_));

    std::unique_ptr<TestChromeDownloadManagerDelegate> incognito_test_delegate;
    incognito_test_delegate.reset(
        new TestChromeDownloadManagerDelegate(incognito_profile));
    DownloadServiceFactory::GetForBrowserContext(incognito_profile)
        ->SetDownloadManagerDelegateForTesting(
            std::move(incognito_test_delegate));
  }

  TestChromeDownloadManagerDelegate* GetIncognitoDownloadManagerDelegate()
      const {
    Profile* incognito_profile = incognito_browser()->profile();
    return static_cast<TestChromeDownloadManagerDelegate*>(
        DownloadServiceFactory::GetForBrowserContext(incognito_profile)->
        GetDownloadManagerDelegate());
  }

  void CreateDownload() {
    return CreateDownloadForBrowserAndURL(
        browser(),
        GURL(net::URLRequestSlowDownloadJob::kKnownSizeUrl));
  }

  void CreateDownloadForBrowserAndURL(Browser* browser, GURL url) {
    // Starts a download.
    NotificationAddObserver download_start_notification_observer;
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_TRUE(download_start_notification_observer.Wait());

    // Confirms that a notification is created.
    notification_id_ = download_start_notification_observer.notification_id();
    EXPECT_FALSE(notification_id_.empty());
    ASSERT_TRUE(notification());

    // Confirms that there is only one notification.
    message_center::NotificationList::Notifications
        visible_notifications = GetMessageCenter()->GetVisibleNotifications();
    EXPECT_EQ(1u, visible_notifications.size());
    EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id_));

    // Confirms that a download is also started.
    std::vector<content::DownloadItem*> downloads;
    GetDownloadManager(browser)->GetAllDownloads(&downloads);
    EXPECT_EQ(1u, downloads.size());
    download_item_ = downloads[0];
    ASSERT_TRUE(download_item_);
  }

  content::DownloadItem* download_item() const { return download_item_; }
  std::string notification_id() const { return notification_id_; }
  message_center::Notification* notification() const {
    return GetNotification(notification_id_);
  }
  Browser* incognito_browser() const { return incognito_browser_; }
  base::FilePath GetDownloadPath() {
    return DownloadPrefs::FromDownloadManager(GetDownloadManager(browser()))->
      DownloadPath();
  }

 private:
  content::DownloadItem* download_item_ = nullptr;
  Browser* incognito_browser_ = nullptr;
  std::string notification_id_;
};

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadFile) {
  CreateDownload();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id())->type());

  // Confirms that the download update is delivered to the notification.
  NotificationUpdateObserver download_notification_periodically_update_observer;
  download_item()->UpdateObservers();
  download_notification_periodically_update_observer.Wait();

  // Requests to complete the download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for download completion.
  NotificationUpdateObserver
      download_change_notification_observer(notification_id());
  while (download_item()->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
  }

  // Checks strings.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id())->type());

  // Confirms that there is only one notification.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(1u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id()));

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);

  // Try to open the downloaded item by clicking the notification.
  EXPECT_FALSE(GetDownloadManagerDelegate()->opened());
  GetMessageCenter()->ClickOnNotification(notification_id());
  EXPECT_TRUE(GetDownloadManagerDelegate()->opened());

  EXPECT_FALSE(GetNotification(notification_id()));
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadDangerousFile) {
  GURL download_url(embedded_test_server()->GetURL(
      "/downloads/dangerous/dangerous.swf"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()),
      1u,  /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  base::FilePath filename = download_item()->GetFileNameToReportUser();

  // Checks the download status.
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download_item()->GetDangerType());
  EXPECT_TRUE(download_item()->IsDangerous());

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);

  NotificationRemoveObserver notification_close_observer;
  NotificationAddObserver notification_add_observer;

  // Cicks the "keep" button.
  notification()->ButtonClick(1);  // 2nd button: "Keep"
  // Clicking makes the message center closed.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  // Confirms that the notification is closed and re-shown.
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());
  notification_add_observer.Wait();
  EXPECT_EQ(notification_id(), notification_add_observer.notification_id());
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());

  // Checks the download status.
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
            download_item()->GetDangerType());
  EXPECT_FALSE(download_item()->IsDangerous());

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  // Checks the download status.
  EXPECT_FALSE(download_item()->IsDangerous());
  EXPECT_EQ(content::DownloadItem::COMPLETE, download_item()->GetState());

  // Checks the downloaded file.
  EXPECT_TRUE(base::PathExists(GetDownloadPath().Append(filename.BaseName())));
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DiscardDangerousFile) {
  GURL download_url(embedded_test_server()->GetURL(
      "/downloads/dangerous/dangerous.swf"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()),
      1u,  /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  base::FilePath filename = download_item()->GetFileNameToReportUser();

  // Checks the download status.
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download_item()->GetDangerType());
  EXPECT_TRUE(download_item()->IsDangerous());

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  // Ensures the notification exists.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());

  NotificationRemoveObserver notification_close_observer;

  // Clicks the "Discard" button.
  notification()->ButtonClick(0);  // 1st button: "Discard"
  // Clicking makes the message center closed.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  // Confirms that the notification is closed.
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());

  // Ensures the notification has closed.
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  // Checks there is neither any download nor any notification.
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());

  // Checks the downloaded file doesn't exist.
  EXPECT_FALSE(base::PathExists(GetDownloadPath().Append(filename.BaseName())));
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadImageFile) {
  GURL download_url(embedded_test_server()->GetURL(
      "/downloads/image-octet-stream.png"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()), 1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  // Waits for download completion.
  NotificationUpdateObserver
      download_change_notification_observer(notification_id());
  while (GetNotification(notification_id())->image().IsEmpty()) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
  }
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       CloseNotificationAfterDownload) {
  CreateDownload();

  // Requests to complete the download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for download completion.
  NotificationUpdateObserver
      download_change_notification_observer(notification_id());
  while (download_item()->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
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
  content::DownloadManager* download_manager = GetDownloadManager(browser());
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Installs observers before requesting the completion.
  NotificationAddObserver download_notification_add_observer;
  content::DownloadTestObserverTerminal download_terminal_observer(
      download_manager,
      1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Requests to complete the download and wait for it.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));
  download_terminal_observer.WaitForFinished();

  // Waits that new notification.
  download_notification_add_observer.Wait();

  // Confirms that there is only one notification.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(1u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id()));
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, InterruptDownload) {
  CreateDownload();

  // Installs observers before requesting.
  NotificationUpdateObserver
      download_notification_update_observer(notification_id());
  content::DownloadTestObserverInterrupted download_terminal_observer(
      GetDownloadManager(browser()),
      1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Requests to fail the download and wait for it.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kErrorDownloadUrl));
  download_terminal_observer.WaitForFinished();

  // Waits that new notification.
  download_notification_update_observer.Wait();

  // Confirms that there is only one notification.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(1u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id()));

  // Checks strings.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_NE(GetNotification(notification_id())->message().find(
                l10n_util::GetStringFUTF16(
                    IDS_DOWNLOAD_STATUS_INTERRUPTED,
                    l10n_util::GetStringUTF16(
                        IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NETWORK_ERROR))),
            std::string::npos);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id())->type());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       InterruptDownloadAfterClosingNotification) {
  CreateDownload();

  // Closes the notification.
  NotificationRemoveObserver notification_close_observer;
  GetMessageCenter()->RemoveNotification(notification_id(), true /* by_user */);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());

  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is still in progress.
  std::vector<content::DownloadItem*> downloads;
  content::DownloadManager* download_manager = GetDownloadManager(browser());
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Installs observers before requesting the completion.
  NotificationAddObserver download_notification_add_observer;
  content::DownloadTestObserverInterrupted download_terminal_observer(
      download_manager,
      1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Requests to fail the download and wait for it.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kErrorDownloadUrl));
  download_terminal_observer.WaitForFinished();

  // Waits that new notification.
  download_notification_add_observer.Wait();

  // Confirms that there is only one notification.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(1u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id()));
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
  content::DownloadItem* download1 = downloads[0];

  // Confirms that there is a notifications.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(1u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id1));

  // Confirms that there is a popup notifications.
  message_center::NotificationList::PopupNotifications
      popup_notifications = GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(1u, popup_notifications.size());
  EXPECT_TRUE(IsInNotifications(popup_notifications, notification_id1));

  // Starts the 2nd download and waits for a notification.
  NotificationAddObserver download_start_notification_observer2(2);
  ui_test_utils::NavigateToURL(browser(), url2);
  // 2 notifications should be added. One is for new download (url2), and the
  // other one is for reshowing the existing download (url1) as a low-priority
  // notification.
  EXPECT_TRUE(download_start_notification_observer2.Wait());

  // Confirms that there are 2 downloads.
  downloads.clear();
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(2u, downloads.size());
  content::DownloadItem* download2;
  if (download1 == downloads[0])
    download2 = downloads[1];
  else if (download1 == downloads[1])
    download2 = downloads[0];
  else
    NOTREACHED();
  EXPECT_NE(download1, download2);

  // Confirms that there are 2 notifications.
  visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(2u, visible_notifications.size());

  std::string notification_id2;
  for (auto* notification : visible_notifications) {
    if (notification->id() == notification_id1) {
      continue;
    } else if (notification->type() ==
               message_center::NOTIFICATION_TYPE_PROGRESS) {
      notification_id2 = (notification->id());
    }
  }
  EXPECT_TRUE(!notification_id2.empty());

  // Confirms that the both notifications are visible either as popup or in the
  // message center.
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id1));
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id2));

  // Confirms that the new one is popup, and the old one is not.
  popup_notifications = GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(1u, popup_notifications.size());
  EXPECT_FALSE(IsInNotifications(popup_notifications, notification_id1));
  EXPECT_TRUE(IsInNotifications(popup_notifications, notification_id2));

  // Confirms that the old one is low priority, and the new one is default.
  EXPECT_EQ(message_center::LOW_PRIORITY,
            GetNotification(notification_id1)->priority());
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id2)->priority());

  // Confirms that the updates of both download are delivered to the
  // notifications.
  NotificationUpdateObserver
      notification_periodically_update_observer1(notification_id1);
  download1->UpdateObservers();
  notification_periodically_update_observer1.Wait();
  NotificationUpdateObserver
      notification_periodically_update_observer2(notification_id2);
  download2->UpdateObservers();
  notification_periodically_update_observer2.Wait();

  // Requests to complete the downloads.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for the completion of downloads.
  NotificationUpdateObserver download_change_notification_observer;
  while (download1->GetState() != content::DownloadItem::COMPLETE ||
         download2->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
  }

  // Confirms that the both notifications are visible either as popup or in the
  // message center.
  visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(2u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id1));
  EXPECT_TRUE(IsInNotifications(visible_notifications, notification_id2));

  // Confirms that the both are popup'd.
  popup_notifications = GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(2u, popup_notifications.size());
  EXPECT_TRUE(IsInNotifications(popup_notifications, notification_id1));
  EXPECT_TRUE(IsInNotifications(popup_notifications, notification_id2));

  // Confirms that the both are default priority after downloads finish.
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id1)->priority());
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id2)->priority());

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id2)->type());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DownloadMultipleFilesOneByOne) {
  CreateDownload();
  content::DownloadItem* first_download_item = download_item();
  content::DownloadItem* second_download_item = nullptr;
  std::string first_notification_id = notification_id();
  std::string second_notification_id;

  // Requests to complete the first download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for completion of the first download.
  NotificationUpdateObserver
      download_change_notification_observer1(first_notification_id);
  while (first_download_item->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer1.Wait();
    download_change_notification_observer1.Reset();
  }
  EXPECT_EQ(content::DownloadItem::COMPLETE, first_download_item->GetState());

  // Checks the message center.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());

  // Starts the second download.
  GURL url(net::URLRequestSlowDownloadJob::kKnownSizeUrl);
  NotificationAddObserver download_start_notification_observer;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(download_start_notification_observer.Wait());

  // Confirms that the second notification is created.
  second_notification_id =
      download_start_notification_observer.notification_id();
  EXPECT_FALSE(second_notification_id.empty());
  ASSERT_TRUE(GetNotification(second_notification_id));

  // Confirms that there are two notifications, including the second
  // notification.
  message_center::NotificationList::Notifications
      visible_notifications = GetMessageCenter()->GetVisibleNotifications();
  EXPECT_EQ(2u, visible_notifications.size());
  EXPECT_TRUE(IsInNotifications(visible_notifications, first_notification_id));
  EXPECT_TRUE(IsInNotifications(visible_notifications, second_notification_id));

  // Confirms that the second download is also started.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(2u, downloads.size());
  EXPECT_TRUE(first_download_item == downloads[0] ||
              first_download_item == downloads[1]);
  // Stores the second download.
  if (first_download_item == downloads[0])
    second_download_item = downloads[1];
  else
    second_download_item = downloads[0];

  EXPECT_EQ(content::DownloadItem::IN_PROGRESS,
            second_download_item->GetState());

  // Requests to complete the second download.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for completion of the second download.
  NotificationUpdateObserver
      download_change_notification_observer2(second_notification_id);
  while (second_download_item->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer2.Wait();
    download_change_notification_observer2.Reset();
  }

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  // Checks the message center.
  EXPECT_EQ(2u, GetMessageCenter()->GetVisibleNotifications().size());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, CancelDownload) {
  CreateDownload();

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);

  // Cancels the notification by clicking the "cancel' button.
  NotificationRemoveObserver notification_close_observer;
  notification()->ButtonClick(1);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is also cancelled.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::CANCELLED, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DownloadCancelledByUserExternally) {
  CreateDownload();

  // Cancels the notification by clicking the "cancel' button.
  NotificationRemoveObserver notification_close_observer;
  download_item()->Cancel(true /* by_user */);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is also cancelled.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::CANCELLED, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DownloadCancelledExternally) {
  CreateDownload();

  // Cancels the notification by clicking the "cancel' button.
  NotificationRemoveObserver notification_close_observer;
  download_item()->Cancel(false /* by_user */);
  EXPECT_EQ(notification_id(), notification_close_observer.Wait());
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());

  // Confirms that a download is also cancelled.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(content::DownloadItem::CANCELLED, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, IncognitoDownloadFile) {
  PrepareIncognitoBrowser();

  // Starts an incognito download.
  CreateDownloadForBrowserAndURL(
      incognito_browser(),
      GURL(net::URLRequestSlowDownloadJob::kKnownSizeUrl));

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id())->type());
  EXPECT_TRUE(download_item()->GetBrowserContext()->IsOffTheRecord());

  // Requests to complete the download.
  ui_test_utils::NavigateToURL(
      incognito_browser(),
      GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for download completion.
  NotificationUpdateObserver
      download_change_notification_observer(notification_id());
  while (download_item()->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
  }

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            GetNotification(notification_id())->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id())->type());

  // Opens the message center.
  GetMessageCenter()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);

  // Try to open the downloaded item by clicking the notification.
  EXPECT_FALSE(GetIncognitoDownloadManagerDelegate()->opened());
  GetMessageCenter()->ClickOnNotification(notification_id());
  EXPECT_TRUE(GetIncognitoDownloadManagerDelegate()->opened());
  EXPECT_FALSE(GetDownloadManagerDelegate()->opened());

  EXPECT_FALSE(GetNotification(notification_id()));
  chrome::CloseWindow(incognito_browser());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       SimultaneousIncognitoAndNormalDownloads) {
  PrepareIncognitoBrowser();

  GURL url_incognito(net::URLRequestSlowDownloadJob::kUnknownSizeUrl);
  GURL url_normal(net::URLRequestSlowDownloadJob::kKnownSizeUrl);

  // Starts the incognito download.
  NotificationAddObserver download_start_notification_observer1;
  ui_test_utils::NavigateToURL(incognito_browser(), url_incognito);
  EXPECT_TRUE(download_start_notification_observer1.Wait());
  std::string notification_id1 =
      download_start_notification_observer1.notification_id();
  EXPECT_FALSE(notification_id1.empty());

  // Confirms that there is a download.
  std::vector<content::DownloadItem*> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());
  downloads.clear();
  GetDownloadManager(incognito_browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  content::DownloadItem* download_incognito = downloads[0];

  // Starts the normal download.
  NotificationAddObserver download_start_notification_observer2;
  ui_test_utils::NavigateToURL(browser(), url_normal);
  EXPECT_TRUE(download_start_notification_observer2.Wait());
  std::string notification_id2 =
      download_start_notification_observer2.notification_id();
  EXPECT_FALSE(notification_id2.empty());

  // Confirms that there are 2 downloads.
  downloads.clear();
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  content::DownloadItem* download_normal = downloads[0];
  EXPECT_EQ(1u, downloads.size());
  EXPECT_NE(download_normal, download_incognito);
  downloads.clear();
  GetDownloadManager(incognito_browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download_incognito, downloads[0]);

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(-1, GetNotification(notification_id1)->progress());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id2)->type());
  EXPECT_LE(0, GetNotification(notification_id2)->progress());

  EXPECT_TRUE(download_incognito->GetBrowserContext()->IsOffTheRecord());
  EXPECT_FALSE(download_normal->GetBrowserContext()->IsOffTheRecord());

  // Requests to complete the downloads.
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for the completion of downloads.
  NotificationUpdateObserver download_change_notification_observer;
  while (download_normal->GetState() != content::DownloadItem::COMPLETE ||
         download_incognito->GetState() != content::DownloadItem::COMPLETE) {
    download_change_notification_observer.Wait();
    download_change_notification_observer.Reset();
  }

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id2)->type());

  chrome::CloseWindow(incognito_browser());
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
    if (log_in) {
      session_manager::SessionManager::Get()->CreateSession(
          AccountId::FromUserEmailGaiaId(info.email, info.gaia_id), info.hash);
    }
    user_manager::UserManager::Get()->SaveUserDisplayName(
        AccountId::FromUserEmailGaiaId(info.email, info.gaia_id),
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
  std::string notification_id_user1 =
      download_start_notification_observer1.notification_id();
  EXPECT_FALSE(notification_id_user1.empty());

  // Second user starts a download.
  NotificationAddObserver download_start_notification_observer2;
  ui_test_utils::NavigateToURL(browser2, url);
  download_start_notification_observer2.Wait();
  std::string notification_id_user2_1 =
      download_start_notification_observer2.notification_id();
  EXPECT_FALSE(notification_id_user2_1.empty());

  // Confirms that the second user has only 1 download.
  downloads.clear();
  GetDownloadManager(browser2)->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  // Second user starts another download.
  NotificationAddObserver download_start_notification_observer3;
  ui_test_utils::NavigateToURL(browser2, url);
  download_start_notification_observer3.Wait();
  std::string notification_id_user2_2 =
      download_start_notification_observer3.notification_id();
  EXPECT_FALSE(notification_id_user2_2.empty());

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
  // Normal notification for user1.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id_user1)->type());
  EXPECT_EQ(-1, GetNotification(notification_id_user1)->progress());
  // Normal notification for user2.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id_user2_1)->type());
  EXPECT_EQ(-1, GetNotification(notification_id_user2_1)->progress());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id_user2_2)->type());
  EXPECT_EQ(-1, GetNotification(notification_id_user2_2)->progress());

  // Requests to complete the downloads.
  NotificationUpdateObserver download_change_notification_observer;
  ui_test_utils::NavigateToURL(
      browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));

  // Waits for the completion of downloads.
  while (download1->GetState() != content::DownloadItem::COMPLETE ||
         download2->GetState() != content::DownloadItem::COMPLETE ||
         download3->GetState() != content::DownloadItem::COMPLETE) {
    // Requests again, since sometimes the request may fail.
    ui_test_utils::NavigateToURL(
        browser(), GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));
    download_change_notification_observer.Wait();
  }

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id_user1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id_user2_1)->type());
  // Normal notifications for user2.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id_user2_1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            GetNotification(notification_id_user2_2)->type());
}
