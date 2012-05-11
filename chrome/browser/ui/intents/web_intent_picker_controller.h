// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/cws_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class Browser;
struct DefaultWebIntentService;
class GURL;
class TabContentsWrapper;
class WebIntentPicker;
class WebIntentPickerModel;

namespace content {
class WebContents;
class WebIntentsDispatcher;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

// Controls the creation of the WebIntentPicker UI and forwards the user's
// intent handler choice back to the WebContents object.
class WebIntentPickerController : public content::NotificationObserver,
                                  public WebIntentPickerDelegate,
                                  public WebstoreInstaller::Delegate {
 public:
  // Takes ownership of |factory|.
  explicit WebIntentPickerController(TabContentsWrapper* wrapper);
  virtual ~WebIntentPickerController();

  // Sets the intent data and return pathway handler object for which
  // this picker was created. The picker takes ownership of
  // |intents_dispatcher|. |intents_dispatcher| must not be NULL.
  void SetIntentsDispatcher(content::WebIntentsDispatcher* intents_dispatcher);

  // Shows the web intent picker given the intent |action| and MIME-type |type|.
  void ShowDialog(const string16& action,
                  const string16& type);

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebIntentPickerDelegate implementation.
  virtual void OnServiceChosen(const GURL& url,
                               Disposition disposition) OVERRIDE;
  virtual void OnInlineDispositionWebContentsCreated(
      content::WebContents* web_contents) OVERRIDE;
  virtual void OnExtensionInstallRequested(const std::string& id) OVERRIDE;
  virtual void OnExtensionLinkClicked(const std::string& id) OVERRIDE;
  virtual void OnSuggestionsLinkClicked() OVERRIDE;
  virtual void OnPickerClosed() OVERRIDE;
  virtual void OnChooseAnotherService() OVERRIDE;
  virtual void OnClosing() OVERRIDE;

  // WebstoreInstaller::Delegate implementation.
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id,
                                         const std::string& error) OVERRIDE;

 private:
  friend class WebIntentPickerControllerTest;
  friend class WebIntentPickerControllerBrowserTest;
  friend class WebIntentPickerControllerIncognitoBrowserTest;
  friend class InvokingTabObserver;

  // Gets a notification when the return message is sent to the source tab,
  // so we can close the picker dialog or service tab.
  void OnSendReturnMessage(webkit_glue::WebIntentReplyType reply_type);

  // Exposed for tests only.
  void set_picker(WebIntentPicker* picker) { picker_ = picker; }

  // Exposed for tests only.
  void set_model_observer(WebIntentPickerModelObserver* observer) {
    picker_model_->set_observer(observer);
  }

  // Called by the WebIntentsRegistry, returning |services|, which is
  // a list of WebIntentServiceData matching the query.
  void OnWebIntentServicesAvailable(
      const std::vector<webkit_glue::WebIntentServiceData>& services);

  // Called when a default service is returned from the WebIntentsRegistry.
  // (Still called with default_service.service_url empty if there are no
  // defaults.)
  void OnWebIntentDefaultsAvailable(
      const DefaultWebIntentService& default_service);

  // Coordination method which is delegated to by the registry calls to get
  // services and defaults. Checks whether the picker should be shown or if
  // default choices allow it to be skipped.
  void RegistryCallsCompleted();

  // Called when WebIntentServiceData is ready for checking extensions
  // when dispatching explicit intents. Gets |services|
  // from the WebIntentsRegistry to check for known urls/extensions and find
  // disposition data.
  void WebIntentServicesForExplicitIntent(
      const std::vector<webkit_glue::WebIntentServiceData>& services);

  // Called when FaviconData is returned from the FaviconService.
  void OnFaviconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon_data);

  // Called when IntentExtensionInfo is returned from the CWSIntentsRegistry.
  void OnCWSIntentServicesAvailable(
      const CWSIntentsRegistry::IntentExtensionList& extensions);

  // Called when a suggested extension's icon is fetched.
  void OnExtensionIconURLFetchComplete(const string16& extension_id,
                                       const net::URLFetcher* source);

  typedef base::Callback<void(const gfx::Image&)>
      ExtensionIconAvailableCallback;
  // Called on a worker thread to decode and resize the extension's icon.
  static void DecodeExtensionIconAndResize(
      scoped_ptr<std::string> icon_response,
      const ExtensionIconAvailableCallback& callback,
      const base::Closure& unavailable_callback);

  // Called when an extension's icon is successfully decoded and resized.
  void OnExtensionIconAvailable(const string16& extension_id,
                                const gfx::Image& icon_image);

  // Called when an extension's icon failed to be decoded or resized.
  void OnExtensionIconUnavailable(const string16& extension_id);

  // When an extension is installed, all that is known is the extension id.
  // This callback receives the intent service data for that extension.
  // |services| must be a non-empty list.
  void OnExtensionInstallServiceAvailable(
      const std::vector<webkit_glue::WebIntentServiceData>& services);

  // Decrements the |pending_async_count_| and notifies the picker if it
  // reaches zero.
  void AsyncOperationFinished();

  // Helper to create picker dialog UI.
  void CreatePicker();

  // Closes the currently active picker.
  void ClosePicker();

  // A weak pointer to the tab contents that the picker is displayed on.
  TabContentsWrapper* wrapper_;

  // A notification registrar, listening for notifications when the tab closes
  // to close the picker ui.
  content::NotificationRegistrar registrar_;

  // A weak pointer to the picker this controller controls.
  WebIntentPicker* picker_;

  // The model for the picker. Owned by this controller. It should not be NULL
  // while this controller exists, even if the picker is not shown.
  scoped_ptr<WebIntentPickerModel> picker_model_;

  // A count of the outstanding asynchronous calls.
  int pending_async_count_;

  // A count of outstanding WebIntentsRegistry calls.
  int pending_registry_calls_count_;

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

  // Request consumer used when asynchronously loading favicons.
  CancelableRequestConsumerTSimple<size_t> favicon_consumer_;

  base::WeakPtrFactory<WebIntentPickerController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
