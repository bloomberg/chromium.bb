// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_controller.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/intents/cws_intents_registry_factory.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/intents/intent_service_host.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_reporting.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_icon_loader.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

using extensions::WebstoreInstaller;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(WebIntentPickerController)

namespace {

// Maximum amount of time to delay displaying dialog while waiting for data.
const int kMaxHiddenSetupTimeMs = 200;

// Minimum amount of time to show waiting dialog, if it is shown.
const int kMinThrobberDisplayTimeMs = 800;

// Gets the web intents registry for the specified profile.
WebIntentsRegistry* GetWebIntentsRegistry(Profile* profile) {
  return WebIntentsRegistryFactory::GetForProfile(profile);
}

// Gets the Chrome web store intents registry for the specified profile.
CWSIntentsRegistry* GetCWSIntentsRegistry(Profile* profile) {
  return CWSIntentsRegistryFactory::GetForProfile(profile);
}

class SourceWindowObserver : content::WebContentsObserver {
 public:
  SourceWindowObserver(content::WebContents* web_contents,
                       base::WeakPtr<WebIntentPickerController> controller)
      : content::WebContentsObserver(web_contents),
        controller_(controller) {}
  virtual ~SourceWindowObserver() {}

  // Implement WebContentsObserver
  virtual void WebContentsDestroyed(content::WebContents* web_contents) {
    if (controller_)
      controller_->SourceWebContentsDestroyed(web_contents);
    delete this;
  }

 private:
  base::WeakPtr<WebIntentPickerController> controller_;
};

// Deletes |service|, presumably called from a dispatcher callback.
void DeleteIntentService(
    web_intents::IntentServiceHost* service,
    webkit_glue::WebIntentReplyType type) {
  delete service;
}

}  // namespace

// UMAReporter handles reporting Web Intents events to UMA.
class WebIntentPickerController::UMAReporter {
 public:

  // Resets the service active duration timer to "now".
  void ResetServiceActiveTimer();

  // Records the duration of time spent using the service. Uses |reply_type|
  // to distinguish between successful and failed service calls.
  void RecordServiceActiveDuration(webkit_glue::WebIntentReplyType reply_type);

 private:

  // The time when the user began using the service.
  base::TimeTicks service_start_time_;
};

void WebIntentPickerController::UMAReporter::ResetServiceActiveTimer() {
  service_start_time_ = base::TimeTicks::Now();
}

void WebIntentPickerController::UMAReporter::RecordServiceActiveDuration(
    webkit_glue::WebIntentReplyType reply_type) {
  if (!service_start_time_.is_null()) {
    web_intents::RecordServiceActiveDuration(reply_type,
        base::TimeTicks::Now() - service_start_time_);
  }
}

WebIntentPickerController::WebIntentPickerController(
    content::WebContents* web_contents)
    : dialog_state_(kPickerHidden),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      picker_(NULL),
      picker_model_(new WebIntentPickerModel()),
      uma_reporter_(new UMAReporter()),
      pending_async_count_(0),
      pending_registry_calls_count_(0),
      pending_cws_request_(false),
      picker_shown_(false),
      window_disposition_source_(NULL),
      source_intents_dispatcher_(NULL),
      intents_dispatcher_(NULL),
      location_bar_button_indicated_(true),
      service_tab_(NULL),
      icon_loader_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(dispatcher_factory_(this)) {
  native_services_.reset(new web_intents::NativeServiceFactory());
#if defined(TOOLKIT_VIEWS)
  cancelled_ = true;
#endif
  icon_loader_.reset(
      new web_intents::IconLoader(profile_, picker_model_.get()));
}

WebIntentPickerController::~WebIntentPickerController() {
  if (picker_)
    picker_->InvalidateDelegate();
  if (webstore_installer_.get())
    webstore_installer_->InvalidateDelegate();
}

// TODO(gbillock): combine this with ShowDialog.
void WebIntentPickerController::SetIntentsDispatcher(
    content::WebIntentsDispatcher* intents_dispatcher) {
  // TODO(gbillock): This is to account for multiple dispatches in the same tab.
  // That is currently not a well-handled case, and this is a band-aid.
  dispatcher_factory_.InvalidateWeakPtrs();
  intents_dispatcher_ = intents_dispatcher;
  intents_dispatcher_->RegisterReplyNotification(
      base::Bind(&WebIntentPickerController::OnSendReturnMessage,
                 dispatcher_factory_.GetWeakPtr()));

  // Initialize the reporting bucket.
  const webkit_glue::WebIntentData& intent = intents_dispatcher_->GetIntent();
  uma_bucket_ = web_intents::ToUMABucket(intent.action, intent.type);
}

// TODO(smckay): rename this "StartActivity".
void WebIntentPickerController::ShowDialog(const string16& action,
                                           const string16& type) {
  ShowDialog(kEnableDefaults);
}

void WebIntentPickerController::ReshowDialog() {
  ShowDialog(kSuppressDefaults);
}

void WebIntentPickerController::ShowDialog(DefaultsUsage suppress_defaults) {
  web_intents::RecordIntentDispatched(uma_bucket_);

  DCHECK(intents_dispatcher_);

#if defined(TOOLKIT_VIEWS)
  cancelled_ = true;
#endif

  // Only show a picker once.
  // TODO(gbillock): There's a hole potentially admitting multiple
  // in-flight dispatches since we don't create the picker
  // in this method, but only after calling the registry.
  if (picker_shown_) {
    intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
        webkit_glue::WEB_INTENT_REPLY_FAILURE,
        ASCIIToUTF16("Simultaneous intent invocation.")));
    return;
  }

  // TODO(binji): Figure out what to do when intents are invoked from incognito
  // mode.
  if (profile_->IsOffTheRecord()) {
    intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
        webkit_glue::WEB_INTENT_REPLY_FAILURE, string16()));
    return;
  }

  picker_model_->Clear();
  picker_model_->set_action(intents_dispatcher_->GetIntent().action);
  picker_model_->set_type(intents_dispatcher_->GetIntent().type);

  // If the intent is explicit, skip showing the picker.
  const GURL& service = intents_dispatcher_->GetIntent().service;
  // TODO(gbillock): Decide whether to honor the default suppression flag
  // here or suppress the control for explicit intents.
  if (service.is_valid() && !suppress_defaults) {
    // Get services from the registry to verify a registered extension
    // page for this action/type if it is permitted to be dispatched. (Also
    // required to find disposition set by service.)
    pending_async_count_++;
    GetWebIntentsRegistry(profile_)->GetIntentServices(
        picker_model_->action(), picker_model_->type(),
        base::Bind(
            &WebIntentPickerController::
            OnWebIntentServicesAvailableForExplicitIntent,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // As soon as the dialog is requested, block all input events
  // on the original tab.
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents_);
  constrained_window_tab_helper->BlockWebContentsInteraction(true);
  SetDialogState(kPickerSetup);

  pending_async_count_++;
  pending_registry_calls_count_++;
  GetWebIntentsRegistry(profile_)->GetIntentServices(
      picker_model_->action(), picker_model_->type(),
          base::Bind(&WebIntentPickerController::OnWebIntentServicesAvailable,
              weak_ptr_factory_.GetWeakPtr()));

  GURL invoking_url = web_contents_->GetURL();
  if (invoking_url.is_valid() && !suppress_defaults) {
    pending_async_count_++;
    pending_registry_calls_count_++;
    GetWebIntentsRegistry(profile_)->GetDefaultIntentService(
        picker_model_->action(), picker_model_->type(), invoking_url,
        base::Bind(&WebIntentPickerController::OnWebIntentDefaultsAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  pending_cws_request_ = true;
  pending_async_count_++;
  GetCWSIntentsRegistry(profile_)->GetIntentServices(
      picker_model_->action(), picker_model_->type(),
      base::Bind(&WebIntentPickerController::OnCWSIntentServicesAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerController::OnServiceChosen(
    const GURL& url,
    webkit_glue::WebIntentServiceData::Disposition disposition,
    DefaultsUsage suppress_defaults) {
  web_intents::RecordServiceInvoke(uma_bucket_);
  uma_reporter_->ResetServiceActiveTimer();
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extension_service);

#if defined(TOOLKIT_VIEWS)
  cancelled_ = false;
#endif

  // Set the default here. Activating the intent resets the picker model.
  // TODO(gbillock): we should perhaps couple the model to the dispatcher so
  // we can re-activate the model on use-another-service.
  if (!suppress_defaults)
    SetDefaultServiceForSelection(url);


  // TODO(smckay): this basically smells like another disposition.
  const extensions::Extension* extension =
      extension_service->GetInstalledApp(url);
  if (extension && extension->is_platform_app()) {
    extensions::LaunchPlatformAppWithWebIntent(profile_,
        extension, intents_dispatcher_, web_contents_);
    ClosePicker();
    return;
  }

  // TODO(smckay): This entire method shold basically be pulled out
  // into a separate class dedicated to the execution of intents.
  // The tricky part is with the "INLINE" disposition where we
  // want to (re)use the picker to handle the intent. A bit of
  // artful composition + lazy instantiation should make that possible.
  switch (disposition) {
    case webkit_glue::WebIntentServiceData::DISPOSITION_NATIVE: {
      web_intents::IntentServiceHost* service =
          native_services_->CreateServiceInstance(
              url, intents_dispatcher_->GetIntent(), web_contents_);
      DCHECK(service);

      intents_dispatcher_->RegisterReplyNotification(
          base::Bind(&DeleteIntentService, base::Unretained(service)));

      service->HandleIntent(intents_dispatcher_);
      break;
    }

    case webkit_glue::WebIntentServiceData::DISPOSITION_INLINE: {
      // Set the model to inline disposition. It will notify the picker which
      // will respond (via OnInlineDispositionWebContentsCreated) with the
      // WebContents to dispatch the intent to.
      picker_model_->SetInlineDisposition(url);
      break;
    }

    case webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW: {
      content::WebContents* contents = content::WebContents::Create(
          content::WebContents::CreateParams(
              profile_, tab_util::GetSiteInstanceForNewTab(profile_, url)));
      WebIntentPickerController::CreateForWebContents(contents);

      // Let the controller for the target WebContents know that it is hosting a
      // web intents service. Suppress if we're not showing the
      // use-another-service button.
      if (picker_model_->show_use_another_service()) {
        WebIntentPickerController::FromWebContents(contents)->
            SetWindowDispositionSource(web_contents_, intents_dispatcher_);
      }

      intents_dispatcher_->DispatchIntent(contents);
      service_tab_ = contents;

      // This call performs all the tab strip manipulation, notifications, etc.
      // Since we're passing in a target_contents, it assumes that we will
      // navigate the page ourselves, though.
      chrome::NavigateParams params(profile_, url,
                                    content::PAGE_TRANSITION_LINK);
      params.target_contents = contents;
      params.disposition = NEW_FOREGROUND_TAB;
      params.tabstrip_add_types = TabStripModel::ADD_INHERIT_GROUP;
      chrome::Navigate(&params);

      service_tab_->GetController().LoadURL(
          url, content::Referrer(),
          content::PAGE_TRANSITION_AUTO_BOOKMARK, std::string());

      ClosePicker();
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

content::WebContents*
WebIntentPickerController::CreateWebContentsForInlineDisposition(
    Profile* profile, const GURL& url) {
  content::WebContents::CreateParams create_params(
      profile, tab_util::GetSiteInstanceForNewTab(profile, url));
  content::WebContents* web_contents = content::WebContents::Create(
      create_params);
  intents_dispatcher_->DispatchIntent(web_contents);
  return web_contents;
}

void WebIntentPickerController::SetDefaultServiceForSelection(const GURL& url) {
  DCHECK(picker_model_.get());
  if (url == picker_model_->default_service_url())
    return;

  DefaultWebIntentService record;
  record.action = picker_model_->action();
  record.type = picker_model_->type();
  record.service_url = url.spec();
  record.user_date = static_cast<int>(floor(base::Time::Now().ToDoubleT()));
  GetWebIntentsRegistry(profile_)->RegisterDefaultIntentService(record);
}

void WebIntentPickerController::OnExtensionInstallRequested(
    const std::string& id) {
  // Create a local copy of |id| since it is a reference to a member on a UI
  // object, and SetPendingExtensionInstallId triggers an OnModelChanged and
  // a subsequent rebuild of UI objects.
  std::string extension_id(id);

  picker_model_->SetPendingExtensionInstallId(extension_id);

  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithInstallPrompt(profile_));
  // Don't show a bubble pointing to the extension or any other post
  // installation UI.
  approval->skip_post_install_ui = true;
  approval->show_dialog_callback = base::Bind(
      &WebIntentPickerController::OnShowExtensionInstallDialog,
      weak_ptr_factory_.GetWeakPtr());

  webstore_installer_ = new WebstoreInstaller(profile_, this,
      &web_contents_->GetController(), extension_id,
      approval.Pass(), WebstoreInstaller::FLAG_INLINE_INSTALL);

  pending_async_count_++;
  webstore_installer_->Start();
}

void WebIntentPickerController::OnExtensionLinkClicked(
    const std::string& id,
    WindowOpenDisposition disposition) {
  // Navigate from source tab.
  GURL extension_url(extension_urls::GetWebstoreItemDetailURLPrefix() + id);
  chrome::NavigateParams params(profile_, extension_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition =
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition;
  chrome::Navigate(&params);
}

void WebIntentPickerController::OnSuggestionsLinkClicked(
    WindowOpenDisposition disposition) {
  // Navigate from source tab.
  GURL query_url = extension_urls::GetWebstoreIntentQueryURL(
      UTF16ToUTF8(picker_model_->action()),
      UTF16ToUTF8(picker_model_->type()));
  chrome::NavigateParams params(profile_, query_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition =
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition;
  chrome::Navigate(&params);
}

void WebIntentPickerController::OnUserCancelledPickerDialog() {
  if (!intents_dispatcher_)
    return;

  intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
      webkit_glue::WEB_INTENT_PICKER_CANCELLED, string16()));
  web_intents::RecordPickerCancel(uma_bucket_);

  ClosePicker();
}

void WebIntentPickerController::OnChooseAnotherService() {
  DCHECK(intents_dispatcher_);
  web_intents::RecordChooseAnotherService(uma_bucket_);
  intents_dispatcher_->ResetDispatch();
  picker_model_->SetInlineDisposition(GURL::EmptyGURL());
}

void WebIntentPickerController::OnClosing() {
  SetDialogState(kPickerHidden);
  picker_ = NULL;
  picker_model_->ClearPendingExtensionInstall();
  CancelDownload();
#if defined(TOOLKIT_VIEWS)
  if (cancelled_)
    OnUserCancelledPickerDialog();
#endif
}

void WebIntentPickerController::OnExtensionDownloadStarted(
    const std::string& id,
    content::DownloadItem* item) {
  DownloadItemModel(item).SetShouldShowInShelf(false);
  download_id_ = item->GetGlobalId();
  picker_model_->UpdateExtensionDownloadState(item);
}

void WebIntentPickerController::OnExtensionDownloadProgress(
    const std::string& id,
    content::DownloadItem* item) {
  picker_model_->UpdateExtensionDownloadState(item);
}

void WebIntentPickerController::OnExtensionInstallSuccess(
    const std::string& extension_id) {
  webstore_installer_ = NULL;  // Release reference.

  // OnExtensionInstallSuccess is called via NotificationService::Notify before
  // the extension is added to the ExtensionService. Dispatch via PostTask to
  // allow ExtensionService to update.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &WebIntentPickerController::DispatchToInstalledExtension,
          base::Unretained(this),
          extension_id));
}

void WebIntentPickerController::DispatchToInstalledExtension(
    const std::string& extension_id) {
  web_intents::RecordCWSExtensionInstalled(uma_bucket_);

  download_id_ = content::DownloadId();
  picker_model_->ClearPendingExtensionInstall();
  if (picker_)
    picker_->OnExtensionInstallSuccess(extension_id);

  WebIntentsRegistry::IntentServiceList services;
  GetWebIntentsRegistry(profile_)->GetIntentServicesForExtensionFilter(
      picker_model_->action(), picker_model_->type(),
      extension_id,
      &services);

  // Extension must be registered with registry by now.
  DCHECK(services.size() > 0);

  // TODO(binji): We're going to need to disambiguate if there are multiple
  // services. For now, just choose the first.
  const webkit_glue::WebIntentServiceData& service_data = services[0];

  picker_model_->RemoveSuggestedExtension(extension_id);
  AddServiceToModel(service_data);
  OnServiceChosen(service_data.service_url, service_data.disposition,
                  kEnableDefaults);
  AsyncOperationFinished();
}

void WebIntentPickerController::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  webstore_installer_ = NULL;  // Release reference.

  // If the user cancelled the install then don't show an error message.
  if (reason == WebstoreInstaller::FAILURE_REASON_CANCELLED)
    picker_model_->ClearPendingExtensionInstall();
  else
    picker_model_->SetPendingExtensionInstallStatusString(UTF8ToUTF16(error));

  if (picker_)
    picker_->OnExtensionInstallFailure(id);
  AsyncOperationFinished();
}

void WebIntentPickerController::OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
  ClosePicker();
  uma_reporter_->RecordServiceActiveDuration(reply_type);

  if (service_tab_ &&
      reply_type != webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED) {
    Browser* browser = chrome::FindBrowserWithWebContents(service_tab_);
    if (browser) {
      int index = browser->tab_strip_model()->GetIndexOfWebContents(
          service_tab_);
      browser->tab_strip_model()->CloseWebContentsAt(
          index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

      // Activate source tab.
      Browser* source_browser =
          chrome::FindBrowserWithWebContents(web_contents_);
      if (source_browser) {
        int source_index = source_browser->tab_strip_model()->
            GetIndexOfWebContents(web_contents_);
        source_browser->tab_strip_model()->ActivateTabAt(source_index, false);
      }
    }
    service_tab_ = NULL;
  }

  intents_dispatcher_ = NULL;
}

void WebIntentPickerController::AddServiceToModel(
    const webkit_glue::WebIntentServiceData& service) {

  picker_model_->AddInstalledService(
      service.title,
      service.service_url,
      service.disposition);

  icon_loader_->LoadFavicon(service.service_url);
}

void WebIntentPickerController::OnWebIntentServicesAvailable(
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  for (size_t i = 0; i < services.size(); ++i)
    AddServiceToModel(services[i]);

  RegistryCallsCompleted();
  AsyncOperationFinished();
}

void WebIntentPickerController::OnWebIntentServicesAvailableForExplicitIntent(
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  DCHECK(intents_dispatcher_);
  DCHECK(intents_dispatcher_->GetIntent().service.is_valid());
  for (size_t i = 0; i < services.size(); ++i) {
    if (services[i].service_url != intents_dispatcher_->GetIntent().service)
      continue;

    InvokeServiceWithSelection(services[i]);
    AsyncOperationFinished();
    return;
  }

  // No acceptable extension. The intent cannot be dispatched.
  intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
      webkit_glue::WEB_INTENT_REPLY_FAILURE,  ASCIIToUTF16(
          "Explicit extension URL is not available.")));

  AsyncOperationFinished();
}

void WebIntentPickerController::OnWebIntentDefaultsAvailable(
    const DefaultWebIntentService& default_service) {
  if (!default_service.service_url.empty()) {
    // TODO(gbillock): this doesn't belong in the model, but keep it there
    // for now.
    picker_model_->set_default_service_url(GURL(default_service.service_url));
  }

  RegistryCallsCompleted();
  AsyncOperationFinished();
}

void WebIntentPickerController::RegistryCallsCompleted() {
  pending_registry_calls_count_--;
  if (pending_registry_calls_count_ != 0) return;

  if (picker_model_->default_service_url().is_valid()) {
    // If there's a default service, dispatch to it immediately
    // without showing the picker.
    const WebIntentPickerModel::InstalledService* default_service =
        picker_model_->GetInstalledServiceWithURL(
            GURL(picker_model_->default_service_url()));
    if (default_service != NULL) {
      InvokeService(*default_service);
      return;
    }
  }

  OnPickerEvent(kPickerEventRegistryDataComplete);
  OnIntentDataArrived();
}

void WebIntentPickerController::OnCWSIntentServicesAvailable(
    const CWSIntentsRegistry::IntentExtensionList& extensions) {
  ExtensionServiceInterface* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  for (size_t i = 0; i < extensions.size(); ++i) {
    const CWSIntentsRegistry::IntentExtensionInfo& info = extensions[i];

    // Do not include suggestions for already installed extensions.
    if (extension_service->GetExtensionById(info.id, true))
      continue;

    suggestions.push_back(WebIntentPickerModel::SuggestedExtension(
        info.name, info.id, info.average_rating));

    icon_loader_->LoadExtensionIcon(info.icon_url, info.id);
  }

  picker_model_->AddSuggestedExtensions(suggestions);

  AsyncOperationFinished();
  pending_cws_request_ = false;
  OnIntentDataArrived();
}


void WebIntentPickerController::OnIntentDataArrived() {
  DCHECK(picker_model_.get());

  if (!pending_cws_request_ &&
      pending_registry_calls_count_ == 0)
    OnPickerEvent(kPickerEventAsyncDataComplete);
}

void WebIntentPickerController::Reset() {
  // Abandon all callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  timer_factory_.InvalidateWeakPtrs();

  // Reset state associated with callbacks.
  pending_async_count_ = 0;
  pending_registry_calls_count_ = 0;
  pending_cws_request_ = false;

  // Reset picker.
  icon_loader_.reset();
  picker_model_.reset(new WebIntentPickerModel());
  icon_loader_.reset(
      new web_intents::IconLoader(profile_, picker_model_.get()));

  picker_shown_ = false;

  DCHECK(web_contents_);
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents_);
  constrained_window_tab_helper->BlockWebContentsInteraction(false);
}

void WebIntentPickerController::OnShowExtensionInstallDialog(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  picker_model_->SetPendingExtensionInstallDelegate(delegate);
  picker_model_->SetPendingExtensionInstallPrompt(prompt);
  if (picker_)
    picker_->OnShowExtensionInstallDialog(show_params, delegate, prompt);
}

void WebIntentPickerController::SetWindowDispositionSource(
    content::WebContents* source,
    content::WebIntentsDispatcher* dispatcher) {
  DCHECK(source);
  DCHECK(dispatcher);
  location_bar_button_indicated_ = false;
  window_disposition_source_ = source;
  if (window_disposition_source_) {
    // This object is self-deleting when the source WebContents is destroyed.
    new SourceWindowObserver(window_disposition_source_,
                             weak_ptr_factory_.GetWeakPtr());
  }

  if (dispatcher) {
    dispatcher->RegisterReplyNotification(
      base::Bind(&WebIntentPickerController::SourceDispatcherReplied,
                 weak_ptr_factory_.GetWeakPtr()));
  }
  source_intents_dispatcher_ = dispatcher;
}

void WebIntentPickerController::SourceWebContentsDestroyed(
    content::WebContents* source) {
  window_disposition_source_ = NULL;
  // TODO(gbillock): redraw location bar to kill button
}

void WebIntentPickerController::SourceDispatcherReplied(
    webkit_glue::WebIntentReplyType reply_type) {
  source_intents_dispatcher_ = NULL;
  // TODO(gbillock): redraw location bar to kill button
}

bool WebIntentPickerController::ShowLocationBarPickerButton() {
  return window_disposition_source_ || source_intents_dispatcher_;
}

void WebIntentPickerController::OnPickerEvent(WebIntentPickerEvent event) {
  switch (event) {
    case kPickerEventHiddenSetupTimeout:
      DCHECK(dialog_state_ == kPickerSetup);
      SetDialogState(kPickerWaiting);
      break;

    case kPickerEventMaxWaitTimeExceeded:
      DCHECK(dialog_state_ == kPickerWaiting);

      // If registry data is complete, go to main dialog. Otherwise, wait.
      if (pending_registry_calls_count_ == 0)
        SetDialogState(kPickerMain);
      else
        SetDialogState(kPickerWaitLong);
      break;

    case kPickerEventRegistryDataComplete:
      DCHECK(dialog_state_ == kPickerSetup ||
             dialog_state_ == kPickerWaiting ||
             dialog_state_ == kPickerWaitLong);

      // If minimum wait dialog time is exceeded, display main dialog.
      // Either way, we don't do a thing.
      break;

    case kPickerEventAsyncDataComplete:
      DCHECK(dialog_state_ == kPickerSetup ||
             dialog_state_ == kPickerWaiting ||
             dialog_state_ == kPickerWaitLong ||
             dialog_state_ == kPickerInline);

      // In setup state, transition to main dialog. In waiting state, let
      // timer expire.
      if (dialog_state_ == kPickerSetup)
        SetDialogState(kPickerMain);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void WebIntentPickerController::LocationBarPickerButtonClicked() {
  DCHECK(web_contents_);
  if (window_disposition_source_ && source_intents_dispatcher_) {
    Browser* service_browser =
        chrome::FindBrowserWithWebContents(web_contents_);
    if (!service_browser) return;

    Browser* client_browser =
        chrome::FindBrowserWithWebContents(window_disposition_source_);
    if (!client_browser)
      return;
    int client_index = client_browser->tab_strip_model()->GetIndexOfWebContents(
        window_disposition_source_);
    DCHECK(client_index != TabStripModel::kNoTab);

    source_intents_dispatcher_->ResetDispatch();

    WebIntentPickerController* client_controller =
        WebIntentPickerController::FromWebContents(window_disposition_source_);
    DCHECK(client_controller);

    // This call deletes this object, so anything below here needs to
    // use stack variables.
    chrome::CloseWebContents(service_browser, web_contents_, true);

    // Re-open the other tab and activate the picker.
    client_browser->window()->Activate();
    client_browser->tab_strip_model()->ActivateTabAt(client_index, true);
    // The picker has been Reset() when the new tab is created; need to fully
    // reload.
    client_controller->ReshowDialog();
  }
  // TODO(gbillock): figure out what we ought to do in this case. Probably
  // nothing? Refresh the location bar?
}

void WebIntentPickerController::AsyncOperationFinished() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (--pending_async_count_ == 0) {
    if (picker_)
      picker_->OnPendingAsyncCompleted();
  }
}

void WebIntentPickerController::InvokeServiceWithSelection(
    const webkit_glue::WebIntentServiceData& service) {
  if (picker_shown_) {
    intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
        webkit_glue::WEB_INTENT_REPLY_FAILURE,
        ASCIIToUTF16("Simultaneous intent invocation.")));
    return;
  }

  // TODO(gbillock): this is a bit hacky and exists because in the inline case,
  // the picker currently assumes it exists.
  AddServiceToModel(service);
  picker_model_->set_show_use_another_service(false);

  if (service.disposition ==
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE) {
    picker_model_->SetInlineDisposition(service.service_url);
    SetDialogState(kPickerInline);
    return;
  }

  OnServiceChosen(service.service_url, service.disposition, kSuppressDefaults);
}

void WebIntentPickerController::InvokeService(
    const WebIntentPickerModel::InstalledService& service) {
  if (service.disposition ==
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE) {
    // This call will ensure the picker dialog is created and initialized.
    picker_model_->SetInlineDisposition(service.url);
    SetDialogState(kPickerInline);
    return;
  }
  OnServiceChosen(service.url, service.disposition, kEnableDefaults);
}

void WebIntentPickerController::SetDialogState(WebIntentPickerState state) {
  // Ignore events that don't change state.
  if (state == dialog_state_)
    return;

  // Any pending timers are abandoned on state changes.
  timer_factory_.InvalidateWeakPtrs();

  switch (state) {
    case kPickerSetup:
      DCHECK_EQ(dialog_state_, kPickerHidden);
      // Post timer CWS pending
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          base::Bind(&WebIntentPickerController::OnPickerEvent,
                     timer_factory_.GetWeakPtr(),
                     kPickerEventHiddenSetupTimeout),
          base::TimeDelta::FromMilliseconds(kMaxHiddenSetupTimeMs));
      break;

    case kPickerWaiting:
      DCHECK_EQ(dialog_state_, kPickerSetup);
      // Waiting dialog can be dismissed after minimum wait time.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          base::Bind(&WebIntentPickerController::OnPickerEvent,
                     timer_factory_.GetWeakPtr(),
                     kPickerEventMaxWaitTimeExceeded),
          base::TimeDelta::FromMilliseconds(kMinThrobberDisplayTimeMs));
      break;

    case kPickerWaitLong:
      DCHECK_EQ(dialog_state_, kPickerWaiting);
      break;

    case kPickerInline:
      // Intentional fall-through.
    case kPickerMain:
      // No DCHECK - main state can be reached from any state.
      // Ready to display data.
      picker_model_->SetWaitingForSuggestions(false);
      break;

    case kPickerHidden:
      Reset();
      break;

    default:
      NOTREACHED();
      break;

  }

  dialog_state_ = state;

  // Create picker dialog when changing away from hidden state.
  if (dialog_state_ != kPickerHidden && dialog_state_ != kPickerSetup)
    CreatePicker();
}

void WebIntentPickerController::CreatePicker() {
  // If picker is non-NULL, it was set by a test.
  if (picker_ == NULL)
    picker_ = WebIntentPicker::Create(web_contents_, this, picker_model_.get());
  picker_->SetActionString(WebIntentPicker::GetDisplayStringForIntentAction(
      picker_model_->action()));
  web_intents::RecordPickerShow(
      uma_bucket_, picker_model_->GetInstalledServiceCount());
  picker_shown_ = true;
}

void WebIntentPickerController::ClosePicker() {
  SetDialogState(kPickerHidden);
  if (picker_)
    picker_->Close();
}

void WebIntentPickerController::CancelDownload() {
  if (!download_id_.IsValid())
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(profile);
  if (!download_manager)
    return;
  content::DownloadItem* item =
      download_manager->GetDownload(download_id_.local());
  if (item)
    item->Cancel(true);
  download_id_ = content::DownloadId();
}
