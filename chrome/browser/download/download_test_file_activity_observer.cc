// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_test_file_activity_observer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace content {
class DownloadItem;
}

// Test ChromeDownloadManagerDelegate that controls whether how file chooser
// dialogs are handled, and how files are opend.
// By default, file chooser dialogs are disabled.
class DownloadTestFileActivityObserver::MockDownloadManagerDelegate
    : public ChromeDownloadManagerDelegate {
 public:
  explicit MockDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile),
        file_chooser_enabled_(false),
        file_chooser_displayed_(false) {}

  void EnableFileChooser(bool enable) {
    file_chooser_enabled_ = enable;
  }

  bool TestAndResetDidShowFileChooser() {
    bool did_show = file_chooser_displayed_;
    file_chooser_displayed_ = false;
    return did_show;
  }

 protected:

  virtual void ChooseDownloadPath(content::DownloadItem* item,
                                  const base::FilePath& suggested_path,
                                  const FileSelectedCallback&
                                      callback) OVERRIDE {
    file_chooser_displayed_ = true;
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, (file_chooser_enabled_ ? suggested_path
                                         : base::FilePath())));
  }

  virtual void OpenDownload(content::DownloadItem* item) OVERRIDE {}

 private:
  virtual ~MockDownloadManagerDelegate() {}

  bool file_chooser_enabled_;
  bool file_chooser_displayed_;
};

DownloadTestFileActivityObserver::DownloadTestFileActivityObserver(
    Profile* profile) {
  test_delegate_ = new MockDownloadManagerDelegate(profile);
  DownloadServiceFactory::GetForProfile(profile)->
      SetDownloadManagerDelegateForTesting(test_delegate_.get());
}

DownloadTestFileActivityObserver::~DownloadTestFileActivityObserver() {
}

void DownloadTestFileActivityObserver::EnableFileChooser(bool enable) {
  test_delegate_->EnableFileChooser(enable);
}

bool DownloadTestFileActivityObserver::TestAndResetDidShowFileChooser() {
  return test_delegate_->TestAndResetDidShowFileChooser();
}
