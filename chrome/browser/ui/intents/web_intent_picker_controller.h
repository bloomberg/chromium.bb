// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/web_intent_data.h"

class FaviconService;
class GURL;
class SkBitmap;
class TabContentsWrapper;
class WebDataService;
class WebIntentPicker;
class WebIntentPickerFactory;
struct WebIntentServiceData;

// Controls the creation of the WebIntentPicker UI and forwards the user's
// intent handler choice back to the TabContents object.
class WebIntentPickerController : public NotificationObserver,
                                  public WebIntentPickerDelegate {
 public:
  // Takes ownership of |factory|.
  WebIntentPickerController(TabContentsWrapper* wrapper,
                            WebIntentPickerFactory* factory);
  virtual ~WebIntentPickerController();

  // Sets the intent data for which this picker was created. The picker will
  // copy and hold this data until the user has made a service selection.
  // |routing_id| is the IPC routing ID of the source renderer.
  // |intent| is the intent data as created by the client content.
  // |intent_id| is the ID assigned by the source renderer.
  void SetIntent(int routing_id,
                 const webkit_glue::WebIntentData& intent,
                 int intent_id);

  // Shows the web intent picker for the window |parent|, given the intent
  // |action| and MIME-type |type|.
  void ShowDialog(gfx::NativeWindow parent,
                  const string16& action,
                  const string16& type);

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
  friend class WebIntentPickerControllerBrowserTest;
  class WebIntentDataFetcher;
  class FaviconFetcher;

  int pending_async_count() const { return pending_async_count_; }

  // Called from the WebIntentDataFetcher when intent data is available.
  void OnWebIntentDataAvailable(
      const std::vector<WebIntentServiceData>& intent_data);

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

  // The routing id of the renderer which launched the intent. Should be the
  // renderer associated with the TabContents which owns this object.
  int routing_id_;

  // The intent data from the client.
  webkit_glue::WebIntentData intent_;

  // The intent ID assigned to this intent by the renderer.
  int intent_id_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
