// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
#define CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class WebUI;

// For browser_tests, which run on the UI thread, run a second message
// MessageLoop to detect HtmlDialog creation and quit when the constructed
// WebUI instance is captured and ready.
class TestHtmlDialogObserver : public NotificationObserver {
 public:
  // Create and register a new TestHtmlDialogObserver.
  TestHtmlDialogObserver();
  virtual ~TestHtmlDialogObserver();

  // Waits for an HtmlDialog to be created. The WebUI instance is captured
  // and the method returns it when the navigation on the dialog is complete.
  WebUI* GetWebUI();

 private:
  // NotificationObserver:
  virtual void Observe(int type, const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  WebUI* web_ui_;
  bool done_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(TestHtmlDialogObserver);
};

#endif  // CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
