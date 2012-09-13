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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/intents/cws_intents_registry_factory.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_reporting.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

using extensions::WebstoreInstaller;

namespace {

// Maximum amount of time to delay displaying dialog while waiting for data.
const int kMaxHiddenSetupTimeMs = 200;

// Minimum amount of time to show waiting dialog, if it is shown.
const int kMinThrobberDisplayTimeMs = 2000;


// Gets the favicon service for the profile in |tab_contents|.
FaviconService* GetFaviconService(TabContents* tab_contents) {
  return FaviconServiceFactory::GetForProfile(tab_contents->profile(),
                                              Profile::EXPLICIT_ACCESS);
}

// Gets the web intents registry for the profile in |tab_contents|.
WebIntentsRegistry* GetWebIntentsRegistry(TabContents* tab_contents) {
  return WebIntentsRegistryFactory::GetForProfile(tab_contents->profile());
}

// Gets the Chrome web store intents registry for the profile in |tab_contents|.
CWSIntentsRegistry* GetCWSIntentsRegistry(TabContents* tab_contents) {
  return CWSIntentsRegistryFactory::GetForProfile(tab_contents->profile());
}

// Returns the action-specific string for |action|.
string16 GetIntentActionString(const std::string& action) {
  if (!action.compare(web_intents::kActionShare))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SHARE);
  else if (!action.compare(web_intents::kActionEdit))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_EDIT);
  else if (!action.compare(web_intents::kActionView))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_VIEW);
  else if (!action.compare(web_intents::kActionPick))
    // Using generic string per UX suggestions.
    return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE);
  else if (!action.compare(web_intents::kActionSubscribe))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SUBSCRIBE);
  else if (!action.compare(web_intents::kActionSave))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SAVE);
  else
    return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE);
}

// Self-deleting trampoline that forwards A URLFetcher response to a callback.
class URLFetcherTrampoline : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const net::URLFetcher* source)>
      ForwardingCallback;

  explicit URLFetcherTrampoline(const ForwardingCallback& callback);
  ~URLFetcherTrampoline();

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  // Fowarding callback from |OnURLFetchComplete|.
  ForwardingCallback callback_;
};

URLFetcherTrampoline::URLFetcherTrampoline(const ForwardingCallback& callback)
    : callback_(callback) {
}

URLFetcherTrampoline::~URLFetcherTrampoline() {
}

void URLFetcherTrampoline::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(!callback_.is_null());
  callback_.Run(source);
  delete source;
  delete this;
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

}  // namespace

WebIntentPickerController::WebIntentPickerController(
    TabContents* tab_contents)
    : dialog_state_(kPickerHidden),
      tab_contents_(tab_contents),
      picker_(NULL),
      picker_model_(new WebIntentPickerModel()),
      pending_async_count_(0),
      pending_registry_calls_count_(0),
      pending_cws_request_(false),
      picker_shown_(false),
      window_disposition_source_(NULL),
      source_intents_dispatcher_(NULL),
      intents_dispatcher_(NULL),
      service_tab_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)) {
  content::NavigationController* controller =
      &tab_contents->web_contents()->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::Source<content::NavigationController>(controller));
}

WebIntentPickerController::~WebIntentPickerController() {
}

// TODO(gbillock): combine this with ShowDialog.
void WebIntentPickerController::SetIntentsDispatcher(
    content::WebIntentsDispatcher* intents_dispatcher) {
  intents_dispatcher_ = intents_dispatcher;
  intents_dispatcher_->RegisterReplyNotification(
      base::Bind(&WebIntentPickerController::OnSendReturnMessage,
                 weak_ptr_factory_.GetWeakPtr()));

  // Initialize the reporting bucket.
  const webkit_glue::WebIntentData& intent = intents_dispatcher_->GetIntent();
  uma_bucket_ = web_intents::ToUMABucket(intent.action, intent.type);
}

// TODO(smckay): rename this "StartActivity".
void WebIntentPickerController::ShowDialog(const string16& action,
                                           const string16& type) {
  ShowDialog(false);
}

void WebIntentPickerController::ReshowDialog() {
  ShowDialog(true);
}

void WebIntentPickerController::ShowDialog(bool suppress_defaults) {
  web_intents::RecordIntentDispatched(uma_bucket_);

  DCHECK(intents_dispatcher_);

  // Only show a picker once.
  // TODO(gbillock): There's a hole potentially admitting multiple
  // in-flight dispatches since we don't create the picker
  // in this method, but only after calling the registry.
  if (picker_shown_) {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_REPLY_FAILURE,
        ASCIIToUTF16("Simultaneous intent invocation."));
    return;
  }

  // TODO(binji): Figure out what to do when intents are invoked from incognito
  // mode.
  if (tab_contents_->profile()->IsOffTheRecord()) {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_REPLY_FAILURE, string16());
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
    // TODO(gbillock): When we can parse pages for the intent tag,
    // take out this requirement that explicit intents dispatch to
    // extension urls.
    if (!service.SchemeIs(chrome::kExtensionScheme)) {
      intents_dispatcher_->SendReplyMessage(
          webkit_glue::WEB_INTENT_REPLY_FAILURE, ASCIIToUTF16(
              "Only extension urls are supported for explicit invocation"));
      return;
    }

    // Get services from the registry to verify a registered extension
    // page for this action/type if it is permitted to be dispatched. (Also
    // required to find disposition set by service.)
    pending_async_count_++;
    GetWebIntentsRegistry(tab_contents_)->GetIntentServices(
        picker_model_->action(), picker_model_->type(),
        base::Bind(
            &WebIntentPickerController::
            OnWebIntentServicesAvailableForExplicitIntent,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // As soon as the dialog is requested, block all input events
  // on the original tab.
  tab_contents_->constrained_window_tab_helper()->BlockTabContent(true);
  SetDialogState(kPickerSetup);

  pending_async_count_++;
  pending_registry_calls_count_++;
  GetWebIntentsRegistry(tab_contents_)->GetIntentServices(
      picker_model_->action(), picker_model_->type(),
          base::Bind(&WebIntentPickerController::OnWebIntentServicesAvailable,
              weak_ptr_factory_.GetWeakPtr()));

  GURL invoking_url = tab_contents_->web_contents()->GetURL();
  if (invoking_url.is_valid() && !suppress_defaults) {
    pending_async_count_++;
    pending_registry_calls_count_++;
    GetWebIntentsRegistry(tab_contents_)->GetDefaultIntentService(
        picker_model_->action(), picker_model_->type(), invoking_url,
        base::Bind(&WebIntentPickerController::OnWebIntentDefaultsAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  pending_cws_request_ = true;
  pending_async_count_++;
  GetCWSIntentsRegistry(tab_contents_)->GetIntentServices(
      picker_model_->action(), picker_model_->type(),
      base::Bind(&WebIntentPickerController::OnCWSIntentServicesAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_START ||
         type == chrome::NOTIFICATION_TAB_CLOSING);
  ClosePicker();
}

void WebIntentPickerController::OnServiceChosen(
    const GURL& url,
    webkit_glue::WebIntentServiceData::Disposition disposition) {
  web_intents::RecordServiceInvoke(uma_bucket_);
  ExtensionService* service = tab_contents_->profile()->GetExtensionService();
  DCHECK(service);

  // Set the default here. Activating the intent resets the picker model.
  // TODO(gbillock): we should perhaps couple the model to the dispatcher so
  // we can re-activate the model on use-another-service.
  SetDefaultServiceForSelection(url);

  const extensions::Extension* extension = service->GetInstalledApp(url);

  // TODO(smckay): this basically smells like another disposition.
  if (extension && extension->is_platform_app()) {
    extensions::LaunchPlatformAppWithWebIntent(tab_contents_->profile(),
        extension, intents_dispatcher_, tab_contents_->web_contents());
    ClosePicker();
    return;
  }

  switch (disposition) {
    case webkit_glue::WebIntentServiceData::DISPOSITION_INLINE:
      // Set the model to inline disposition. It will notify the picker which
      // will respond (via OnInlineDispositionWebContentsCreated) with the
      // WebContents to dispatch the intent to.
      picker_model_->SetInlineDisposition(url);
      break;

    case webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW: {
      TabContents* contents = chrome::TabContentsFactory(
          tab_contents_->profile(),
          tab_util::GetSiteInstanceForNewTab(
              tab_contents_->profile(), url),
          MSG_ROUTING_NONE, NULL);

      // Let the controller for the target TabContents know that it is hosting a
      // web intents service.
      contents->web_intent_picker_controller()->SetWindowDispositionSource(
          tab_contents_->web_contents(), intents_dispatcher_);

      intents_dispatcher_->DispatchIntent(contents->web_contents());
      service_tab_ = contents->web_contents();

      // This call performs all the tab strip manipulation, notifications, etc.
      // Since we're passing in a target_contents, it assumes that we will
      // navigate the page ourselves, though.
      chrome::NavigateParams params(tab_contents_->profile(), url,
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

// Take the MD5 digest of the origins of picker options
// and record it as the default context string.
int64 WebIntentPickerController::DigestServices() {
  // The context in which the default is registered is all
  // the installed services represented in the picker (represented
  // by their site origins).
  std::vector<std::string> context_urls;
  for (size_t i = 0; i < picker_model_->GetInstalledServiceCount(); ++i) {
    const GURL& url = picker_model_->GetInstalledServiceAt(i).url;
    if (url.SchemeIs(chrome::kExtensionScheme))
      context_urls.push_back(url.host());  // this is the extension ID
    else
      context_urls.push_back(url.GetOrigin().spec());
  }
  std::sort(context_urls.begin(), context_urls.end());

  base::MD5Context md5_context;
  base::MD5Init(&md5_context);
  for (size_t i = 0; i < context_urls.size(); ++i)
    base::MD5Update(&md5_context, context_urls[i]);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context);
  int64 hash = 0;
  COMPILE_ASSERT(sizeof(base::MD5Digest) > sizeof(int64),
                 int64_size_greater_than_md5_buffer);
  memcpy(&hash, &digest, sizeof(int64));

  return hash;
}

void WebIntentPickerController::SetDefaultServiceForSelection(const GURL& url) {
  int64 service_hash = DigestServices();
  DCHECK(picker_model_.get());
  if (url == picker_model_->default_service_url() &&
      service_hash == picker_model_->default_service_hash()) {
    return;
  }

  DefaultWebIntentService record;
  record.action = picker_model_->action();
  record.type = picker_model_->type();
  record.service_url = url.spec();
  record.suppression = service_hash;
  record.user_date = static_cast<int>(floor(base::Time::Now().ToDoubleT()));
  GetWebIntentsRegistry(tab_contents_)->RegisterDefaultIntentService(record);
}

void WebIntentPickerController::OnInlineDispositionWebContentsCreated(
    content::WebContents* web_contents) {
  if (web_contents)
    intents_dispatcher_->DispatchIntent(web_contents);
}

void WebIntentPickerController::OnExtensionInstallRequested(
    const std::string& id) {
  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithInstallPrompt(
          tab_contents_->profile()));

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      tab_contents_->profile(), this,
      &tab_contents_->web_contents()->GetController(), id,
      approval.Pass(), WebstoreInstaller::FLAG_INLINE_INSTALL);

  pending_async_count_++;
  installer->Start();
}

void WebIntentPickerController::OnExtensionLinkClicked(
    const std::string& id,
    WindowOpenDisposition disposition) {
  // Navigate from source tab.
  GURL extension_url(extension_urls::GetWebstoreItemDetailURLPrefix() + id);
  chrome::NavigateParams params(tab_contents_->profile(), extension_url,
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
  chrome::NavigateParams params(tab_contents_->profile(), query_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition =
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition;
  chrome::Navigate(&params);
}

void WebIntentPickerController::OnPickerClosed() {
  if (!intents_dispatcher_)
    return;

  if (service_tab_) {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED, string16());
  } else {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_PICKER_CANCELLED, string16());
    web_intents::RecordPickerCancel(uma_bucket_);
  }

  ClosePicker();
}

void WebIntentPickerController::OnChooseAnotherService() {
  DCHECK(intents_dispatcher_);
  web_intents::RecordChooseAnotherService(uma_bucket_);
  intents_dispatcher_->ResetDispatch();
}

void WebIntentPickerController::OnClosing() {
  SetDialogState(kPickerHidden);
  picker_ = NULL;
}

void WebIntentPickerController::OnExtensionInstallSuccess(
    const std::string& extension_id) {
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
  picker_->OnExtensionInstallSuccess(extension_id);
  WebIntentsRegistry::IntentServiceList services;
  GetWebIntentsRegistry(tab_contents_)->GetIntentServicesForExtensionFilter(
      picker_model_->action(), picker_model_->type(),
      extension_id,
      &services);

  // Extension must be registered with registry by now.
  DCHECK(services.size() > 0);

  // TODO(binji): We're going to need to disambiguate if there are multiple
  // services. For now, just choose the first.
  const webkit_glue::WebIntentServiceData& service_data = services[0];

  picker_model_->AddInstalledService(
      service_data.title, service_data.service_url,
      service_data.disposition);
  OnServiceChosen(service_data.service_url, service_data.disposition);
  AsyncOperationFinished();
}

void WebIntentPickerController::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error) {
  picker_->OnExtensionInstallFailure(id);
  AsyncOperationFinished();
}

void WebIntentPickerController::OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
  ClosePicker();

  if (service_tab_ &&
      reply_type != webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED) {
    Browser* browser = browser::FindBrowserWithWebContents(service_tab_);
    if (browser) {
      int index = browser->tab_strip_model()->GetIndexOfWebContents(
          service_tab_);
      browser->tab_strip_model()->CloseTabContentsAt(
          index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

      // Activate source tab.
      Browser* source_browser =
          browser::FindBrowserWithWebContents(tab_contents_->web_contents());
      if (source_browser) {
        int source_index = source_browser->tab_strip_model()->
            GetIndexOfTabContents(tab_contents_);
        chrome::ActivateTabAt(source_browser, source_index, false);
      }
    }
    service_tab_ = NULL;
  }

  intents_dispatcher_ = NULL;
}

void WebIntentPickerController::AddServiceToModel(
    const webkit_glue::WebIntentServiceData& service) {
  FaviconService* favicon_service = GetFaviconService(tab_contents_);

  picker_model_->AddInstalledService(
      service.title,
      service.service_url,
      service.disposition);

  pending_async_count_++;
  FaviconService::Handle handle = favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(
          tab_contents_->profile(),
          service.service_url,
          history::FAVICON,
          gfx::kFaviconSize,
          &favicon_consumer_),
      base::Bind(
          &WebIntentPickerController::OnFaviconDataAvailable,
          weak_ptr_factory_.GetWeakPtr()));
  favicon_consumer_.SetClientData(
      favicon_service, handle, picker_model_->GetInstalledServiceCount() - 1);
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

    AddServiceToModel(services[i]);

    // BUG? This should a) not use (i)
    InvokeService(picker_model_->GetInstalledServiceAt(i));
    AsyncOperationFinished();
    return;
  }

  // No acceptable extension. The intent cannot be dispatched.
  intents_dispatcher_->SendReplyMessage(
      webkit_glue::WEB_INTENT_REPLY_FAILURE,  ASCIIToUTF16(
          "Explicit extension URL is not available."));

  AsyncOperationFinished();
}

void WebIntentPickerController::OnWebIntentDefaultsAvailable(
    const DefaultWebIntentService& default_service) {
  if (!default_service.service_url.empty()) {
    picker_model_->set_default_service_url(GURL(default_service.service_url));
    picker_model_->set_default_service_hash(default_service.suppression);
  }

  RegistryCallsCompleted();
  AsyncOperationFinished();
}

void WebIntentPickerController::RegistryCallsCompleted() {
  pending_registry_calls_count_--;
  if (pending_registry_calls_count_ != 0) return;

  if (picker_model_->default_service_url().is_valid() &&
      picker_model_->default_service_hash() == DigestServices()) {
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

void WebIntentPickerController::OnFaviconDataAvailable(
    FaviconService::Handle handle,
    const history::FaviconImageResult& image_result) {
  size_t index = favicon_consumer_.GetClientDataForCurrentRequest();
  if (!image_result.image.IsEmpty()) {
    picker_model_->UpdateFaviconAt(index, image_result.image);
    return;
  }

  AsyncOperationFinished();
}

void WebIntentPickerController::OnCWSIntentServicesAvailable(
    const CWSIntentsRegistry::IntentExtensionList& extensions) {
  ExtensionServiceInterface* extension_service =
      tab_contents_->profile()->GetExtensionService();

  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  for (size_t i = 0; i < extensions.size(); ++i) {
    const CWSIntentsRegistry::IntentExtensionInfo& info = extensions[i];

    // Do not include suggestions for already installed extensions.
    if (extension_service->GetExtensionById(UTF16ToUTF8(info.id),
                                            true)) {
      continue;
    }

    suggestions.push_back(WebIntentPickerModel::SuggestedExtension(
        info.name, info.id, info.average_rating));

    pending_async_count_++;
    net::URLFetcher* icon_url_fetcher = net::URLFetcher::Create(
        0,
        info.icon_url,
        net::URLFetcher::GET,
        new URLFetcherTrampoline(
            base::Bind(
                &WebIntentPickerController::OnExtensionIconURLFetchComplete,
                weak_ptr_factory_.GetWeakPtr(), info.id)));

    icon_url_fetcher->SetLoadFlags(
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
    icon_url_fetcher->SetRequestContext(
        tab_contents_->profile()->GetRequestContext());
    icon_url_fetcher->Start();
  }

  picker_model_->AddSuggestedExtensions(suggestions);

  AsyncOperationFinished();
  pending_cws_request_ = false;
  OnIntentDataArrived();
}

void WebIntentPickerController::OnExtensionIconURLFetchComplete(
    const string16& extension_id, const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (source->GetResponseCode() != 200) {
    AsyncOperationFinished();
    return;
  }

  scoped_ptr<std::string> response(new std::string);
  if (!source->GetResponseAsString(response.get())) {
    AsyncOperationFinished();
    return;
  }

  // I'd like to have the worker thread post a task directly to the UI thread
  // to call OnExtensionIcon[Un]Available, but this doesn't work: To do so
  // would require DecodeExtensionIconAndResize to be a member function (so it
  // has access to |this|) but a weak pointer cannot be dereferenced on a
  // thread other than the thread where the WeakPtrFactory was created. Since
  // the stored |this| pointer is weak, DecodeExtensionIconAndResize asserts
  // before it even starts.
  //
  // Instead, I package up the callbacks that I want the worker thread to call,
  // and make DecodeExtensionIconAndResize static. The stored weak |this|
  // pointers are not dereferenced until invocation (on the UI thread).
  ExtensionIconAvailableCallback available_callback =
      base::Bind(
          &WebIntentPickerController::OnExtensionIconAvailable,
          weak_ptr_factory_.GetWeakPtr(),
          extension_id);
  base::Closure unavailable_callback =
      base::Bind(
          &WebIntentPickerController::OnExtensionIconUnavailable,
          weak_ptr_factory_.GetWeakPtr(),
          extension_id);

  // Decode PNG and resize on worker thread.
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DecodeExtensionIconAndResize,
                 base::Passed(&response),
                 available_callback,
                 unavailable_callback));
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
  picker_model_.reset(new WebIntentPickerModel());
  picker_shown_ = false;

  DCHECK(tab_contents_);
  tab_contents_->constrained_window_tab_helper()->BlockTabContent(false);
}

// static
void WebIntentPickerController::DecodeExtensionIconAndResize(
    scoped_ptr<std::string> icon_response,
    const ExtensionIconAvailableCallback& callback,
    const base::Closure& unavailable_callback) {
  SkBitmap icon_bitmap;
  if (gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(icon_response->data()),
        icon_response->length(),
        &icon_bitmap)) {
    SkBitmap resized_icon = skia::ImageOperations::Resize(
        icon_bitmap,
        skia::ImageOperations::RESIZE_BEST,
        gfx::kFaviconSize, gfx::kFaviconSize);
    gfx::Image icon_image(resized_icon);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback, icon_image));
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        unavailable_callback);
  }
}

void WebIntentPickerController::OnExtensionIconAvailable(
    const string16& extension_id,
    const gfx::Image& icon_image) {
  picker_model_->SetSuggestedExtensionIconWithId(extension_id, icon_image);
  AsyncOperationFinished();
}

void WebIntentPickerController::OnExtensionIconUnavailable(
    const string16& extension_id) {
  AsyncOperationFinished();
}

void WebIntentPickerController::SetWindowDispositionSource(
    content::WebContents* source,
    content::WebIntentsDispatcher* dispatcher) {
  DCHECK(source);
  DCHECK(dispatcher);
  window_disposition_source_ = source;
  if (window_disposition_source_) {
    // This object is self-deleting when the source WebContents is destroyed.
    new SourceWindowObserver(window_disposition_source_,
                             weak_ptr_factory_.GetWeakPtr());
  }

  source_intents_dispatcher_ = dispatcher;
  if (dispatcher) {
    dispatcher->RegisterReplyNotification(
      base::Bind(&WebIntentPickerController::SourceDispatcherReplied,
                 weak_ptr_factory_.GetWeakPtr()));
  }
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

bool WebIntentPickerController::ShowLocationBarPickerTool() {
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
             dialog_state_ == kPickerWaitLong);

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

void WebIntentPickerController::LocationBarPickerToolClicked() {
  DCHECK(tab_contents_);
  if (window_disposition_source_ && source_intents_dispatcher_) {
    Browser* service_browser =
        browser::FindBrowserWithWebContents(tab_contents_->web_contents());
    if (!service_browser) return;

    TabContents* client_tab =
        TabContents::FromWebContents(window_disposition_source_);
    Browser* client_browser =
        browser::FindBrowserWithWebContents(window_disposition_source_);
    if (!client_browser || !client_tab) return;
    int client_index =
        client_browser->tab_strip_model()->GetIndexOfTabContents(client_tab);
    DCHECK(client_index != TabStripModel::kNoTab);

    source_intents_dispatcher_->ResetDispatch();

    chrome::CloseWebContents(service_browser, tab_contents_->web_contents());

    // Re-open the other tab and activate the picker.
    client_browser->window()->Activate();
    client_browser->tab_strip_model()->ActivateTabAt(client_index, true);
    // The picker has been Reset() when the new tab is created; need to fully
    // reload.
    client_tab->web_intent_picker_controller()->ReshowDialog();
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

void WebIntentPickerController::InvokeService(
    const WebIntentPickerModel::InstalledService& service) {
  if (service.disposition ==
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE) {
    SetDialogState(kPickerMain);
  }
  OnServiceChosen(service.url, service.disposition);
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
    picker_ = WebIntentPicker::Create(tab_contents_, this, picker_model_.get());
  picker_->SetActionString(GetIntentActionString(
      UTF16ToUTF8(picker_model_->action())));
  web_intents::RecordPickerShow(
      uma_bucket_, picker_model_->GetInstalledServiceCount());
  picker_shown_ = true;
}

void WebIntentPickerController::ClosePicker() {
  SetDialogState(kPickerHidden);
  if (picker_)
    picker_->Close();
}
