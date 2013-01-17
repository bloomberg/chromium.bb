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
#include "chrome/browser/intents/cws_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_reporting.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/web_contents_user_data.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"
#include "webkit/glue/web_intent_service_data.h"

class Browser;
struct DefaultWebIntentService;
class GURL;
class Profile;
class WebIntentPicker;
class WebIntentPickerModel;

namespace content {
class WebContents;
class WebIntentsDispatcher;
}

namespace web_intents {
class NativeServiceFactory;
class IconLoader;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

// Controls the creation of the WebIntentPicker UI and forwards the user's
// intent handler choice back to the WebContents object.
class WebIntentPickerController
    : public WebIntentPickerDelegate,
      public extensions::WebstoreInstaller::Delegate,
      public content::WebContentsUserData<WebIntentPickerController> {
 public:

  // The various states that the UI may be in. Public for testing.
  enum WebIntentPickerState {
    kPickerHidden,  // Picker not displayed at all.
    kPickerSetup,  // Initial setup. Acquires data, keeps picker hidden.
    kPickerWaiting, // Displaying "waiting for CWS".
    kPickerWaitLong,  // "waiting" has displayed for longer than min. time.
    kPickerMain,  // Displaying main picker dialog.
    kPickerInline, // Displaying inline intent handler.
  };

  // Events that happen during picker life time. Drive state machine.
  enum WebIntentPickerEvent {
    kPickerEventHiddenSetupTimeout,  // Time for hidden setup exired.
    kPickerEventMaxWaitTimeExceeded,  // Exceeded max wait time for CWS results.
    kPickerEventRegistryDataComplete,  // Data from the registry has arrived.
    kPickerEventAsyncDataComplete,  // Data from registry and CWS has arrived.
  };

  virtual ~WebIntentPickerController();

  // Sets the intent data and return pathway handler object for which
  // this picker was created. The picker takes ownership of
  // |intents_dispatcher|. |intents_dispatcher| must not be NULL.
  void SetIntentsDispatcher(content::WebIntentsDispatcher* intents_dispatcher);

  // Shows the web intent picker given the intent |action| and MIME-type |type|.
  void ShowDialog(const string16& action,
                  const string16& type);

  // Called directly after SetIntentsDispatcher to handle the dispatch of the
  // intent to the indicated service. The picker model may still be unloaded.
  void InvokeServiceWithSelection(
      const webkit_glue::WebIntentServiceData& service);

  // Called by the location bar to see whether the web intents picker button
  // should be shown.
  bool ShowLocationBarPickerButton();

  // Record that the location bar button has been animated.
  void SetLocationBarPickerButtonIndicated() {
    location_bar_button_indicated_ = true;
  }

  // Check whether the location bar button has been animated.
  bool location_bar_picker_button_indicated() const {
    return location_bar_button_indicated_;
  }

  // Called by the location bar to notify picker that the button was clicked.
  // Called in the controller of the tab which is displaying the service.
  void LocationBarPickerButtonClicked();

  // Called to notify a controller for a page hosting a web intents service
  // that the source WebContents has been destroyed.
  void SourceWebContentsDestroyed(content::WebContents* source);

 protected:
  // WebIntentPickerDelegate implementation.
  virtual void OnServiceChosen(
      const GURL& url,
      webkit_glue::WebIntentServiceData::Disposition disposition,
      DefaultsUsage suppress_defaults) OVERRIDE;
  virtual content::WebContents* CreateWebContentsForInlineDisposition(
      Profile* profile, const GURL& url) OVERRIDE;
  virtual void OnExtensionInstallRequested(const std::string& id) OVERRIDE;
  virtual void OnExtensionLinkClicked(
      const std::string& id,
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnSuggestionsLinkClicked(
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnUserCancelledPickerDialog() OVERRIDE;
  virtual void OnChooseAnotherService() OVERRIDE;
  virtual void OnClosing() OVERRIDE;

  // extensions::WebstoreInstaller::Delegate implementation.
  virtual void OnExtensionDownloadStarted(const std::string& id,
                                          content::DownloadItem* item) OVERRIDE;
  virtual void OnExtensionDownloadProgress(
      const std::string& id,
      content::DownloadItem* item) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      extensions::WebstoreInstaller::FailureReason reason) OVERRIDE;

 private:
  explicit WebIntentPickerController(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebIntentPickerController>;

  friend class WebIntentPickerControllerTest;
  friend class WebIntentPickerControllerBrowserTest;
  friend class WebIntentPickerControllerIncognitoBrowserTest;
  friend class WebIntentsButtonDecorationTest;

  // Forward declaraton of the internal implementation class.
  class UMAReporter;

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

  // Notify the controller that its WebContents is hosting a web intents
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

  // Called when IntentExtensionInfo is returned from the CWSIntentsRegistry.
  void OnCWSIntentServicesAvailable(
      const CWSIntentsRegistry::IntentExtensionList& extensions);

  void OnIntentDataArrived();

  // Reset internal state to default values.
  void Reset();

  // Called to show a custom extension install dialog.
  void OnShowExtensionInstallDialog(
      const ExtensionInstallPrompt::ShowParams& show_params,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt);

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
  void ShowDialog(DefaultsUsage suppress_defaults);

  // Cancel a pending download if any.
  void CancelDownload();

  WebIntentPickerState dialog_state_;  // Current state of the dialog.

  // A weak pointer to the web contents that the picker is displayed on.
  content::WebContents* web_contents_;

  // A weak pointer to the profile for the web contents.
  Profile* profile_;

  // A weak pointer to the picker this controller controls.
  WebIntentPicker* picker_;

  // The model for the picker. Owned by this controller. It should not be NULL
  // while this controller exists, even if the picker is not shown.
  scoped_ptr<WebIntentPickerModel> picker_model_;

  // UMA reporting manager.
  scoped_ptr<UMAReporter> uma_reporter_;

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

#if defined(TOOLKIT_VIEWS)
  // Set to true if user cancelled the picker dialog. Set to false if the picker
  // dialog is closing for any other reason.
  // TODO(rouslan): We need to fix DialogDelegate in Views to notify us when the
  // user closes the picker dialog. This boolean is a mediocre workaround for
  // lack of that information.
  bool cancelled_;
#endif

  // Weak pointer to the source WebContents for the intent if it is associated
  // with this controller and hosting a web intents window disposition service.
  content::WebContents* window_disposition_source_;

  // If this tab is hosting a web intents service, a weak pointer to dispatcher
  // that invoked us. Weak pointer.
  content::WebIntentsDispatcher* source_intents_dispatcher_;

  // Weak pointer to the routing object for the renderer which launched the
  // intent. Contains the intent data and a way to signal back to the
  // client page.
  content::WebIntentsDispatcher* intents_dispatcher_;

  // Saves whether the use-another-service button has been
  // animated on the location bar.
  bool location_bar_button_indicated_;

  // Weak pointer to the tab servicing the intent. Remembered in order to
  // close it when a reply is sent.
  content::WebContents* service_tab_;

  // Object managing the details of icon loading.
  scoped_ptr<web_intents::IconLoader> icon_loader_;

  // Factory for weak pointers used in callbacks for async calls to load the
  // picker model.
  base::WeakPtrFactory<WebIntentPickerController> weak_ptr_factory_;

  // Timer factory for minimum display time of "waiting" dialog.
  base::WeakPtrFactory<WebIntentPickerController> timer_factory_;

  // Weak pointers for the dispatcher OnSendReturnMessage will not be
  // cancelled on picker close.
  base::WeakPtrFactory<WebIntentPickerController> dispatcher_factory_;

  // Bucket identifier for UMA reporting. Saved off in a field
  // to avoid repeated calculation of the bucket across
  // multiple UMA calls. Should be recalculated each time
  // |intents_dispatcher_| is set.
  web_intents::UMABucket uma_bucket_;

  // Factory used to obtain instance of native services.
  scoped_ptr<web_intents::NativeServiceFactory> native_services_;

  // The ID of a pending extension download.
  content::DownloadId download_id_;

  // Manager for a pending extension download and installation.
  scoped_refptr<extensions::WebstoreInstaller> webstore_installer_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerController);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_CONTROLLER_H_
