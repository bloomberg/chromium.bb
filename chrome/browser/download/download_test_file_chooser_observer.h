// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_CHOOSER_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_CHOOSER_OBSERVER_H_

#include "base/memory/ref_counted.h"

class Profile;

namespace internal {
class MockFileChooserDownloadManagerDelegate;
}

// Observes and overrides file chooser activity for a profile. By default, once
// attached to a profile, this class overrides the default file chooser by
// replacing the ChromeDownloadManagerDelegate associated with |profile|.
// NOTE: Again, this overrides the ChromeDownloadManagerDelegate for |profile|.
class DownloadTestFileChooserObserver {
 public:
  // Attaches to |profile|. By default file chooser dialogs will be disabled
  // once attached. Call EnableFileChooser() to re-enable.
  explicit DownloadTestFileChooserObserver(Profile* profile);
  ~DownloadTestFileChooserObserver();

  // Sets whether the file chooser dialog is enabled. If |enable| is false, any
  // attempt to display a file chooser dialog will cause the download to be
  // canceled. Otherwise, attempting to display a file chooser dialog will
  // result in the download continuing with the suggested path.
  void EnableFileChooser(bool enable);

  // Returns true if a file chooser dialog was displayed since the last time
  // this method was called.
  bool TestAndResetDidShowFileChooser();

 private:
  scoped_refptr<internal::MockFileChooserDownloadManagerDelegate>
      test_delegate_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_CHOOSER_OBSERVER_H_
