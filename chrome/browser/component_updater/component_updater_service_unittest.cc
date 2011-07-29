// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include "base/at_exit.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/component_updater/component_updater_interceptor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/test_notification_tracker.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_service.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Overrides some of the component updater behaviors so it is easier
// to test and loops faster.
class TestConfigurator : public ComponentUpdateService::Configurator {
 public:
  virtual int InitialDelay() OVERRIDE { return 0; }

  virtual int NextCheckDelay() OVERRIDE {
    // Returning 0 makes the component updater to quit the message loop
    // if it does not have pending update task.
    return 0;
  }

  virtual int StepDelay() OVERRIDE { return 0; }

  virtual GURL UpdateUrl() OVERRIDE { return GURL("http://localhost/upd"); }

  virtual size_t UrlSizeLimit() OVERRIDE { return 256; }

  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE {
    return new TestURLRequestContextGetter();
  }

  // Don't use the utility process to decode files.
  virtual bool InProcess() OVERRIDE { return true; }
};

class TestInstaller : public ComponentInstaller {
 public :
  explicit TestInstaller(bool wrong_one)
      : error_(0), install_(0), wrong_one_(wrong_one) {
  }

  virtual void OnUpdateError(int error) OVERRIDE {
    EXPECT_NE(0, error);
    error_ = error;
  }

  virtual bool Install(base::DictionaryValue* manifest,
                       const FilePath& unpack_path) OVERRIDE {
    EXPECT_FALSE(wrong_one_);
    ++install_;
    return file_util::Delete(unpack_path, true);
  }

  int error() const { return error_; }

  int install() const { return install_; }

 private:
  int error_;
  int install_;
  bool wrong_one_;
};

}  // namespace

// Common fixture for all the component updater tests.
class ComponentUpdaterTest : public testing::Test {
 public:
  ComponentUpdaterTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("components");

    const int notifications[] = {
      chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED,
      chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING,
      chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND,
      chrome::NOTIFICATION_COMPONENT_UPDATE_READY
    };

    for (int ix = 0; ix != arraysize(notifications); ++ix) {
      notification_tracker_.ListenFor(notifications[ix],
                                      NotificationService::AllSources());
    }
    URLFetcher::enable_interception_for_tests(true);
  }

  ~ComponentUpdaterTest() {
    URLFetcher::enable_interception_for_tests(false);
  }

  // Makes the full path to a component updater test file.
  const FilePath test_file(const char* file) {
    return test_data_dir_.AppendASCII(file);
  }

  TestNotificationTracker& notification_tracker() {
    return notification_tracker_;
  }

 private:
  FilePath test_data_dir_;
  TestNotificationTracker notification_tracker_;
  // Note that the shadow |at_exit_manager_| will cause the destruction
  // of the component updater when this object is destroyed.
  base::ShadowingAtExitManager at_exit_manager_;
};

TEST_F(ComponentUpdaterTest, EmptyTest) {
  EXPECT_EQ(0ul, notification_tracker().size());
}

