// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class FaviconService;
class GURL;
class SkBitmap;
class TabContentsWrapper;
class WebDataService;
class WebIntentPicker;
class WebIntentPickerFactory;
struct WebIntentData;

// Controls the creation of the WebIntentPicker UI and forwards the user's
// intent handler choice back to the TabContents object.
class WebIntentPickerController : public NotificationObserver,
                                  public WebIntentPickerDelegate {
 public:
  // Takes ownership of |factory|.
  WebIntentPickerController(TabContentsWrapper* wrapper,
                            WebIntentPickerFactory* factory);
  virtual ~WebIntentPickerController();

  // Shows the web intent picker, given the intent |action| and mime-type
  // |type|.
  void ShowDialog(const string16& action, const string16& type);

 protected:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // WebIntentPickerDelegate implementation.
  virtual void OnServiceChosen(size_t index) OVERRIDE;
  virtual void OnCancelled() OVERRIDE;

 private:
  friend class WebIntentPickerControllerTest;
  class WebIntentDataFetcher;
  class FaviconFetcher;

  int pending_async_count() const { return pending_async_count_; }

  // Called from the WebIntentDataFetcher when intent data is available.
  void OnWebIntentDataAvailable(const std::vector<WebIntentData>& intent_data);

  // Called from the FaviconDataFetcher when a favicon is available.
  void OnFaviconDataAvailable(size_t index, const SkBitmap& icon_bitmap);

  // Called from the FaviconDataFetcher when a favicon is not available.
  void OnFaviconDataUnavailable(size_t index);

  // Closes the currently active picker.
  void ClosePicker();

  // A weak pointer to the tab contents that the picker is displayed on.
  TabContentsWrapper* wrapper_;

  // A notification registrar, listening for notifications when the tab closes
  // to close the picker ui.
  NotificationRegistrar registrar_;

  // A factory to create a new picker.
  scoped_ptr<WebIntentPickerFactory> picker_factory_;

  // A helper class to fetch web intent data asynchronously.
  scoped_ptr<WebIntentDataFetcher> web_intent_data_fetcher_;

  // A helper class to fetch favicon data asynchronously.
  scoped_ptr<FaviconFetcher> favicon_fetcher_;

  // A weak pointer to the picker this controller controls.
  WebIntentPicker* picker_;

  // A list of URLs to display in the UI.
  std::vector<GURL> urls_;

  // A count of the outstanding asynchronous calls.
  int pending_async_count_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
