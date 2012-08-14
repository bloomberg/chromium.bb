// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_test_file_chooser_observer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace content {
class DownloadItem;
}

namespace internal {

// Test ChromeDownloadManagerDelegate that controls whether how file chooser
// dialogs are handled. By default, file chooser dialogs are disabled.
class MockFileChooserDownloadManagerDelegate
    : public ChromeDownloadManagerDelegate {
 public:
  explicit MockFileChooserDownloadManagerDelegate(Profile* profile)
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
                                  const FilePath& suggested_path,
                                  const FileSelectedCallback&
                                      callback) OVERRIDE {
    file_chooser_displayed_ = true;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   (file_chooser_enabled_ ? suggested_path : FilePath())));
  }

 private:
  virtual ~MockFileChooserDownloadManagerDelegate() {}

  bool file_chooser_enabled_;
  bool file_chooser_displayed_;
};

}  // namespace internal

DownloadTestFileChooserObserver::DownloadTestFileChooserObserver(
    Profile* profile) {
  test_delegate_ =
      new internal::MockFileChooserDownloadManagerDelegate(profile);
  DownloadServiceFactory::GetForProfile(profile)->
      SetDownloadManagerDelegateForTesting(test_delegate_.get());
}

DownloadTestFileChooserObserver::~DownloadTestFileChooserObserver() {
}

void DownloadTestFileChooserObserver::EnableFileChooser(bool enable) {
  test_delegate_->EnableFileChooser(enable);
}

bool DownloadTestFileChooserObserver::TestAndResetDidShowFileChooser() {
  return test_delegate_->TestAndResetDidShowFileChooser();
}

