// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_

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
#include "chrome/browser/intents/web_intents_reporting.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"
#include "webkit/glue/web_intent_service_data.h"

class Browser;
struct DefaultWebIntentService;
class GURL;
class TabContents;
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
class WebIntentPickerController
    : public content::NotificationObserver,
      public WebIntentPickerDelegate,
      public extensions::WebstoreInstaller::Delegate {
 public:

  // The various states that the UI may be in. Public for testing.
  enum WebIntentPickerState {
    kPickerHidden,  // Picker not displayed at all.
    kPickerSetup,  // Initial setup. Acquires data, keeps picker hidden.
    kPickerWaiting, // Displaying "waiting for CWS".
    kPickerWaitLong,  // "waiting" has displayed for longer than min. time.
    kPickerMain,  // Displaying main picker dialog.
  };

  // Events that happen during picker life time. Drive state machine.
  enum WebIntentPickerEvent {
    kPickerEventHiddenSetupTimeout,  // Time for hidden setup exired.
    kPickerEventMaxWaitTimeExceeded,  // Exceeded max wait time for CWS results.
    kPickerEventRegistryDataComplete,  // Data from the registry has arrived.
    kPickerEventAsyncDataComplete,  // Data from registry and CWS has arrived.
  };

  explicit WebIntentPickerController(TabContents* tab_contents);
  virtual ~WebIntentPickerController();

  // Sets the intent data and return pathway handler object for which
  // this picker was created. The picker takes ownership of
  // |intents_dispatcher|. |intents_dispatcher| must not be NULL.
  void SetIntentsDispatcher(content::WebIntentsDispatcher* intents_dispatcher);

  // Shows the web intent picker given the intent |action| and MIME-type |type|.
  void ShowDialog(const string16& action,
                  const string16& type);

  // Called by the location bar to see whether the web intents picker button
  // should be shown.
  bool ShowLocationBarPickerTool();

  // Called by the location bar to notify picker that the button was clicked.
  // Called in the controller of the tab which is displaying the service.
  void LocationBarPickerToolClicked();

  // Called to notify a controller for a page hosting a web intents service
  // that the source WebContents has been destroyed.
  void SourceWebContentsDestroyed(content::WebContents* source);

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebIntentPickerDelegate implementation.
  virtual void OnServiceChosen(
      const GURL& url,
      webkit_glue::WebIntentServiceData::Disposition disposition) OVERRIDE;
  virtual void OnInlineDispositionWebContentsCreated(
      content::WebContents* web_contents) OVERRIDE;
  virtual void OnExtensionInstallRequested(const std::string& id) OVERRIDE;
  virtual void OnExtensionLinkClicked(
      const std::string& id,
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnSuggestionsLinkClicked(
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnPickerClosed() OVERRIDE;
  virtual void OnChooseAnotherService() OVERRIDE;
  virtual void OnClosing() OVERRIDE;

  // extensions::WebstoreInstaller::Delegate implementation.
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id,
                                         const std::string& error) OVERRIDE;

 private:
  friend class WebIntentPickerControllerTest;
  friend class WebIntentPickerControllerBrowserTest;
  friend class WebIntentPickerControllerIncognitoBrowserTest;
  friend class WebIntentsButtonDecorationTest;

  // Dispatches intent to a just-installed extension with ID |extension_id|.
  void DispatchToInstalledExtension(const std::string& extension_id);

  // Adds a service to the data model.
  void AddServiceToModel(const webkit_glue::WebIntentServiceData& service);

  // Register the user-selected service (indicated by the passed |url|) as
  // the default for the combination of action/type/options in the picker.
  void SetDefaultServiceForSelection(const GURL& url);

  // Calculate a digest value for the services in the picker.
  int64 DigestServices();

  // Gets a notification when the return message is sent to the source tab,
  // so we can close the picker dialog or service tab.
  void OnSendReturnMessage(webkit_glue::WebIntentReplyType reply_type);

  // Exposed for tests only.
  void set_picker(WebIntentPicker* picker) { picker_ = picker; }

  // Exposed for tests only.
  void set_model_observer(WebIntentPickerModelObserver* observer) {
    picker_model_->set_observer(observer);
  }

  // Notify the controller that its TabContents is hosting a web intents
  // service. Sets the source and dispatcher for the invoking client.
  void SetWindowDispositionSource(content::WebContents* source,
                                  content::WebIntentsDispatcher* dispatcher);

  // Called to notify a controller for a page hosting a web intents service
  // that the source dispatcher has been replied on.
  void SourceDispatcherReplied(webkit_glue::WebIntentReplyType reply_type);

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
  void OnWebIntentServicesAvailableForExplicitIntent(
      const std::vector<webkit_glue::WebIntentServiceData>& services);

  // Called when a favicon is returned from the FaviconService.
  void OnFaviconDataAvailable(
      FaviconService::Handle handle,
      const history::FaviconImageResult& image_result);

  // Called when IntentExtensionInfo is returned from the CWSIntentsRegistry.
  void OnCWSIntentServicesAvailable(
      const CWSIntentsRegistry::IntentExtensionList& extensions);

  // Called when a suggested extension's icon is fetched.
  void OnExtensionIconURLFetchComplete(const string16& extension_id,
                                       const net::URLFetcher* source);

  // Called whenever intent data (both from registry and CWS) arrives.
  void OnIntentDataArrived();

  // Reset internal state to default values.
  void Reset();

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

  // Signals that a picker event has occurred.
  void OnPickerEvent(WebIntentPickerEvent event);

  // Decrements the |pending_async_count_| and notifies the picker if it
  // reaches zero.
  void AsyncOperationFinished();

  // Invoke the specified service at |service_url| with chosen |disposition|.
  void InvokeService(const WebIntentPickerModel::InstalledService& service);

  // Sets current dialog state.
  void SetDialogState(WebIntentPickerState state);

  // Helper to create picker dialog UI.
  void CreatePicker();

  // Closes the currently active picker.
  void ClosePicker();

  // Re-starts the process of showing the dialog, suppressing any default
  // queries. Called on the user clicking Use-Another-Service.
  void ReshowDialog();

  // Delegate for ShowDialog and ReshowDialog. Starts all the data queries for
  // loading the picker model and showing the dialog.
  void ShowDialog(bool suppress_defaults);

  WebIntentPickerState dialog_state_;  // Current state of the dialog.

  // A weak pointer to the tab contents that the picker is displayed on.
  TabContents* tab_contents_;

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

  // Indicator that there is a pending request for cws data.
  bool pending_cws_request_;

  // Is true if the picker is currently visible.
  // This bool is not equivalent to picker != NULL in a unit test. In that
  // case, a picker may be non-NULL before it is shown.
  bool picker_shown_;

  // Weak pointer to the source WebContents for the intent if the TabContents
  // with which this controller is associated is hosting a web intents window
  // disposition service.
  content::WebContents* window_disposition_source_;

  // If this tab is hosting a web intents service, a weak pointer to dispatcher
  // that invoked us. Weak pointer.
  content::WebIntentsDispatcher* source_intents_dispatcher_;

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

  // Timer factory for minimum display time of "waiting" dialog.
  base::WeakPtrFactory<WebIntentPickerController> timer_factory_;

  // Bucket identifier for UMA reporting. Saved off in a field
  // to avoid repeated calculation of the bucket across
  // multiple UMA calls. Should be recalculated each time
  // |intents_dispatcher_| is set.
  web_intents::UMABucket uma_bucket_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
