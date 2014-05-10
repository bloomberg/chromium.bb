// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class PluginInstallerTest : public ChromeRenderViewHostTestHarness {
 public:
  PluginInstallerTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  PluginInstaller* installer() { return installer_.get(); }

  scoped_ptr<content::MockDownloadItem> CreateMockDownloadItem();

 private:
  scoped_ptr<PluginInstaller> installer_;
};

PluginInstallerTest::PluginInstallerTest() {
}

void PluginInstallerTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  installer_.reset(new PluginInstaller());
}

void PluginInstallerTest::TearDown() {
  installer_.reset();
  content::RenderViewHostTestHarness::TearDown();
}

scoped_ptr<content::MockDownloadItem>
PluginInstallerTest::CreateMockDownloadItem() {
  scoped_ptr<content::MockDownloadItem> mock_download_item(
      new testing::StrictMock<content::MockDownloadItem>());
  ON_CALL(*mock_download_item, GetState())
      .WillByDefault(testing::Return(content::DownloadItem::IN_PROGRESS));
  return mock_download_item.Pass();
}

class TestPluginInstallerObserver : public PluginInstallerObserver {
 public:
  explicit TestPluginInstallerObserver(PluginInstaller* installer)
      : PluginInstallerObserver(installer),
        download_started_(false),
        download_finished_(false),
        download_cancelled_(false) {}

  bool download_started() const { return download_started_; }
  bool download_finished() const { return download_finished_; }
  bool download_cancelled() const { return download_cancelled_; }
  const std::string& download_error() const { return download_error_; }

 private:
  virtual void DownloadStarted() OVERRIDE { download_started_ = true; }
  virtual void DownloadFinished() OVERRIDE { download_finished_ = true; }
  virtual void DownloadError(const std::string& message) OVERRIDE {
    download_error_ = message;
  }
  virtual void DownloadCancelled() OVERRIDE { download_cancelled_ = true; }

  bool download_started_;
  bool download_finished_;
  std::string download_error_;
  bool download_cancelled_;
};

// Action for invoking the OnStartedCallback of DownloadURLParameters object
// which is assumed to be pointed to by arg0.
ACTION_P2(InvokeOnStartedCallback, download_item, interrupt_reason) {
  arg0->callback().Run(download_item, interrupt_reason);
}

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

const char kTestUrl[] = "http://example.com/some-url";

}  // namespace

// Test that DownloadStarted()/DownloadFinished() notifications are sent to
// observers when a download initiated by PluginInstaller completes
// successfully.
TEST_F(PluginInstallerTest, StartInstalling_SuccessfulDownload) {
  content::MockDownloadManager mock_download_manager;
  base::RunLoop run_loop;
  scoped_ptr<content::MockDownloadItem> download_item(CreateMockDownloadItem());

  EXPECT_CALL(mock_download_manager,
              DownloadUrlMock(testing::Property(
                  &content::DownloadUrlParameters::url, GURL(kTestUrl))))
      .WillOnce(testing::DoAll(
          InvokeOnStartedCallback(download_item.get(),
                                  content::DOWNLOAD_INTERRUPT_REASON_NONE),
          InvokeClosure(run_loop.QuitClosure())));
  EXPECT_CALL(*download_item, SetOpenWhenComplete(_));

  TestPluginInstallerObserver installer_observer(installer());
  installer()->StartInstallingWithDownloadManager(
      GURL(kTestUrl), web_contents(), &mock_download_manager);
  run_loop.Run();

  EXPECT_TRUE(installer_observer.download_started());
  EXPECT_FALSE(installer_observer.download_finished());

  EXPECT_CALL(*download_item, GetState())
      .WillOnce(testing::Return(content::DownloadItem::COMPLETE));
  download_item->NotifyObserversDownloadUpdated();
  EXPECT_TRUE(installer_observer.download_finished());
}

// Test that DownloadStarted()/DownloadError() notifications are sent to
// observers when a download initiated by PluginInstaller fails to start.
TEST_F(PluginInstallerTest, StartInstalling_FailedStart) {
  content::MockDownloadManager mock_download_manager;
  base::RunLoop run_loop;
  scoped_ptr<content::MockDownloadItem> download_item(CreateMockDownloadItem());

  EXPECT_CALL(mock_download_manager,
              DownloadUrlMock(testing::Property(
                  &content::DownloadUrlParameters::url, GURL(kTestUrl))))
      .WillOnce(
          testing::DoAll(InvokeOnStartedCallback(
                             download_item.get(),
                             content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED),
                         InvokeClosure(run_loop.QuitClosure())));

  TestPluginInstallerObserver installer_observer(installer());
  installer()->StartInstallingWithDownloadManager(
      GURL(kTestUrl), web_contents(), &mock_download_manager);
  run_loop.Run();

  EXPECT_TRUE(installer_observer.download_started());
  EXPECT_FALSE(installer_observer.download_finished());
  EXPECT_EQ("Error 20: NETWORK_FAILED", installer_observer.download_error());
}

// Test that DownloadStarted()/DownloadError() notifications are sent to
// observers when a download initiated by PluginInstaller starts successfully
// but is interrupted later.
TEST_F(PluginInstallerTest, StartInstalling_Interrupted) {
  content::MockDownloadManager mock_download_manager;
  base::RunLoop run_loop;
  scoped_ptr<content::MockDownloadItem> download_item(CreateMockDownloadItem());

  EXPECT_CALL(mock_download_manager,
              DownloadUrlMock(testing::Property(
                  &content::DownloadUrlParameters::url, GURL(kTestUrl))))
      .WillOnce(testing::DoAll(
          InvokeOnStartedCallback(download_item.get(),
                                  content::DOWNLOAD_INTERRUPT_REASON_NONE),
          InvokeClosure(run_loop.QuitClosure())));
  EXPECT_CALL(*download_item, SetOpenWhenComplete(_));

  TestPluginInstallerObserver installer_observer(installer());
  installer()->StartInstallingWithDownloadManager(
      GURL(kTestUrl), web_contents(), &mock_download_manager);
  run_loop.Run();

  EXPECT_TRUE(installer_observer.download_started());
  EXPECT_FALSE(installer_observer.download_finished());

  EXPECT_CALL(*download_item, GetState())
      .WillOnce(testing::Return(content::DownloadItem::INTERRUPTED));
  EXPECT_CALL(*download_item, GetLastReason()).WillOnce(
      testing::Return(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
  download_item->NotifyObserversDownloadUpdated();

  EXPECT_TRUE(installer_observer.download_started());
  EXPECT_FALSE(installer_observer.download_finished());
  EXPECT_EQ("NETWORK_FAILED", installer_observer.download_error());
}
