// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
#define CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class JsInjectionReadyObserver;
class WebUI;

// For browser_tests, which run on the UI thread, run a second message
// MessageLoop to detect HtmlDialog creation and quit when the constructed
// WebUI instance is captured and ready.
class TestHtmlDialogObserver : public content::NotificationObserver {
 public:
  // Create and register a new TestHtmlDialogObserver. If
  // |js_injection_ready_observer| is non-NULL, notify it as soon as the RVH is
  // available.
  explicit TestHtmlDialogObserver(
      JsInjectionReadyObserver* js_injection_ready_observer);
  virtual ~TestHtmlDialogObserver();

  // Waits for an HtmlDialog to be created. The WebUI instance is captured
  // and the method returns it when the navigation on the dialog is complete.
  WebUI* GetWebUI();

 private:
  // content::NotificationObserver:
  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  // Observer to take some action when the dialog is ready for JavaScript
  // injection.
  JsInjectionReadyObserver* js_injection_ready_observer_;
  WebUI* web_ui_;
  bool done_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(TestHtmlDialogObserver);
};

#endif  // CHROME_TEST_BASE_TEST_HTML_DIALOG_OBSERVER_H_
