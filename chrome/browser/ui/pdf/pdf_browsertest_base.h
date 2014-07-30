// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_PDF_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_PDF_PDF_BROWSERTEST_BASE_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace base {
class CommandLine;
}

class SkBitmap;

// This class is in its own separate file because it is used in both
// browser_tests and interactive_ui_tests.
class PDFBrowserTest : public InProcessBrowserTest,
                       public testing::WithParamInterface<int>,
                       public content::NotificationObserver {
 public:
  PDFBrowserTest();
  virtual ~PDFBrowserTest();

 protected:
  // Use our own TestServer so that we can serve files from the pdf directory.
  net::test_server::EmbeddedTestServer* pdf_test_server() {
    return &pdf_test_server_;
  }

  int load_stop_notification_count() const {
    return load_stop_notification_count_;
  }

  void Load();
  void WaitForResponse();
  bool VerifySnapshot(const std::string& expected_filename);

 private:
  void CopyFromBackingStoreCallback(bool success, const SkBitmap& bitmap);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // InProcessBrowserTest
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

  // True if the snapshot differed from the expected value.
  bool snapshot_different_;
  // Internal variable used to synchronize to the renderer.
  int next_dummy_search_value_;
  // The filename of the bitmap to compare the snapshot to.
  std::string expected_filename_;
  // If the snapshot is different, holds the location where it's saved.
  base::FilePath snapshot_filename_;
  // How many times we've seen chrome::LOAD_STOP.
  int load_stop_notification_count_;

  net::test_server::EmbeddedTestServer pdf_test_server_;

  DISALLOW_COPY_AND_ASSIGN(PDFBrowserTest);
};

#endif  // CHROME_BROWSER_UI_PDF_PDF_BROWSERTEST_BASE_H_
