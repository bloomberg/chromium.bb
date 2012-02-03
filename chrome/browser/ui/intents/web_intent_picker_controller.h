// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class Browser;
class GURL;
class TabContents;
class TabContentsWrapper;
class WebIntentPicker;
class WebIntentPickerModel;

namespace content {
class WebContents;
class WebIntentsDispatcher;
}

namespace gfx {
class Image;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

// Controls the creation of the WebIntentPicker UI and forwards the user's
// intent handler choice back to the TabContents object.
class WebIntentPickerController : public content::NotificationObserver,
                                  public WebIntentPickerDelegate {
 public:
  // Takes ownership of |factory|.
  explicit WebIntentPickerController(TabContentsWrapper* wrapper);
  virtual ~WebIntentPickerController();

  // Sets the intent data and return pathway handler object for which
  // this picker was created. The picker takes ownership of
  // |intents_dispatcher|. |intents_dispatcher| must not be NULL.
  void SetIntentsDispatcher(content::WebIntentsDispatcher* intents_dispatcher);

  // Shows the web intent picker for |browser|, given the intent
  // |action| and MIME-type |type|.
  void ShowDialog(Browser* browser,
                  const string16& action,
                  const string16& type);

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebIntentPickerDelegate implementation.
  virtual void OnServiceChosen(size_t index, Disposition disposition) OVERRIDE;
  virtual void OnInlineDispositionWebContentsCreated(
      content::WebContents* web_contents) OVERRIDE;
  virtual void OnCancelled() OVERRIDE;
  virtual void OnClosing() OVERRIDE;

 private:
  friend class WebIntentPickerControllerTest;
  friend class WebIntentPickerControllerBrowserTest;
  friend class InvokingTabObserver;
  class WebIntentDataFetcher;
  class FaviconFetcher;

  // Gets a notification when the return message is sent to the source tab,
  // so we can close the picker dialog or service tab.
  void OnSendReturnMessage(webkit_glue::WebIntentReplyType reply_type);

  // Exposed for tests only.
  void set_picker(WebIntentPicker* picker) { picker_ = picker; }

  // Exposed for tests only.
  void set_model_observer(WebIntentPickerModelObserver* observer) {
    picker_model_->set_observer(observer);
  }

  // Called from the WebIntentDataFetcher when intent data is available.
  void OnWebIntentDataAvailable(
      const std::vector<webkit_glue::WebIntentServiceData>& services);

  // Called from the FaviconDataFetcher when a favicon is available.
  void OnFaviconDataAvailable(size_t index, const gfx::Image& icon);

  // Called from the FaviconDataFetcher when a favicon is not available.
  void OnFaviconDataUnavailable(size_t index);

  // Decrements the |pending_async_count_| and notifies the picker if it
  // reaches zero.
  void AsyncOperationFinished();

  // Closes the currently active picker.
  void ClosePicker();

  // A weak pointer to the tab contents that the picker is displayed on.
  TabContentsWrapper* wrapper_;

  // A notification registrar, listening for notifications when the tab closes
  // to close the picker ui.
  content::NotificationRegistrar registrar_;

  // A helper class to fetch web intent data asynchronously.
  scoped_ptr<WebIntentDataFetcher> web_intent_data_fetcher_;

  // A helper class to fetch favicon data asynchronously.
  scoped_ptr<FaviconFetcher> favicon_fetcher_;

  // A weak pointer to the picker this controller controls.
  WebIntentPicker* picker_;

  // The model for the picker. Owned by this controller. It should not be NULL
  // while this controller exists, even if the picker is not shown.
  scoped_ptr<WebIntentPickerModel> picker_model_;

  // A count of the outstanding asynchronous calls.
  int pending_async_count_;

  // Is true if the picker is currently visible.
  // This bool is not equivalent to picker != NULL in a unit test. In that
  // case, a picker may be non-NULL before it is shown.
  bool picker_shown_;

  // Weak pointer to the routing object for the renderer which launched the
  // intent. Contains the intent data and a way to signal back to the
  // client page.
  content::WebIntentsDispatcher* intents_dispatcher_;

  // Weak pointer to the tab servicing the intent. Remembered in order to
  // close it when a reply is sent.
  content::WebContents* service_tab_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
