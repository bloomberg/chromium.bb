// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_controller.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/cws_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/intents/intent_injector.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "webkit/glue/web_intent_service_data.h"

namespace {

// Gets the favicon service for the profile in |tab_contents|.
FaviconService* GetFaviconService(TabContentsWrapper* wrapper) {
  return wrapper->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);
}

// Gets the web intents registry for the profile in |tab_contents|.
WebIntentsRegistry* GetWebIntentsRegistry(TabContentsWrapper* wrapper) {
  return WebIntentsRegistryFactory::GetForProfile(wrapper->profile());
}

// Gets the Chrome web store intents registry for the profile in |tab_contents|.
CWSIntentsRegistry* GetCWSIntentsRegistry(TabContentsWrapper* wrapper) {
  return CWSIntentsRegistryFactory::GetForProfile(wrapper->profile());
}

WebIntentPickerModel::Disposition ConvertDisposition(
    webkit_glue::WebIntentServiceData::Disposition disposition) {
  switch (disposition) {
    case webkit_glue::WebIntentServiceData::DISPOSITION_INLINE:
      return WebIntentPickerModel::DISPOSITION_INLINE;
    case webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW:
      return WebIntentPickerModel::DISPOSITION_WINDOW;
    default:
      NOTREACHED();
      return WebIntentPickerModel::DISPOSITION_WINDOW;
  }
}

}  // namespace

WebIntentPickerController::WebIntentPickerController(
    TabContentsWrapper* wrapper)
    : wrapper_(wrapper),
      picker_(NULL),
      picker_model_(new WebIntentPickerModel()),
      pending_async_count_(0),
      picker_shown_(false),
      intents_dispatcher_(NULL),
      service_tab_(NULL),
      weak_ptr_factory_(this) {
  content::NavigationController* controller =
      &wrapper->web_contents()->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_TAB_CLOSING,
                 content::Source<content::NavigationController>(controller));
}

WebIntentPickerController::~WebIntentPickerController() {
}

void WebIntentPickerController::SetIntentsDispatcher(
    content::WebIntentsDispatcher* intents_dispatcher) {
  intents_dispatcher_ = intents_dispatcher;
  intents_dispatcher_->RegisterReplyNotification(
      base::Bind(&WebIntentPickerController::OnSendReturnMessage,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerController::ShowDialog(Browser* browser,
                                           const string16& action,
                                           const string16& type) {
  // Only show a picker once.
  if (picker_shown_)
    return;

  picker_model_->Clear();

  // If picker is non-NULL, it was set by a test.
  if (picker_ == NULL) {
    picker_ = WebIntentPicker::Create(browser, wrapper_, this,
                                      picker_model_.get());
  }

  picker_shown_ = true;
  pending_async_count_+= 2;
  GetWebIntentsRegistry(wrapper_)->GetIntentServices(action, type, this);
  GetCWSIntentsRegistry(wrapper_)->GetIntentServices(action, type,
      base::Bind(&WebIntentPickerController::OnCWSIntentServicesAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_START ||
         type == content::NOTIFICATION_TAB_CLOSING);
  ClosePicker();
}

void WebIntentPickerController::OnServiceChosen(size_t index,
                                                Disposition disposition) {
  switch (disposition) {
    case WebIntentPickerModel::DISPOSITION_INLINE:
      // Set the model to inline disposition. It will notify the picker which
      // will respond (via OnInlineDispositionWebContentsCreated) with the
      // WebContents to dispatch the intent to.
      picker_model_->SetInlineDisposition(index);
      break;

    case WebIntentPickerModel::DISPOSITION_WINDOW: {
      // TODO(gbillock): This really only handles the 'window' disposition in a
      // quite prototype way. We need to flesh out what happens to the picker
      // during the lifetime of the service url context, and that may mean we
      // need to pass more information into the injector to find the picker
      // again and close it.
      const WebIntentPickerModel::InstalledService& installed_service =
          picker_model_->GetInstalledServiceAt(index);

      int index = TabStripModel::kNoTab;
      Browser* browser = Browser::GetBrowserForController(
          &wrapper_->web_contents()->GetController(), &index);
      TabContentsWrapper* contents = Browser::TabContentsFactory(
          browser->profile(),
          tab_util::GetSiteInstanceForNewTab(
              NULL, browser->profile(), installed_service.url),
          MSG_ROUTING_NONE, NULL, NULL);

      intents_dispatcher_->DispatchIntent(contents->web_contents());
      service_tab_ = contents->web_contents();

      // This call performs all the tab strip manipulation, notifications, etc.
      // Since we're passing in a target_contents, it assumes that we will
      // navigate the page ourselves, though.
      browser::NavigateParams params(browser,
                                     installed_service.url,
                                     content::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.target_contents = contents;
      params.disposition = NEW_FOREGROUND_TAB;
      params.profile = wrapper_->profile();
      browser::Navigate(&params);

      service_tab_->GetController().LoadURL(
          installed_service.url, content::Referrer(),
          content::PAGE_TRANSITION_AUTO_BOOKMARK, std::string());

      ClosePicker();
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void WebIntentPickerController::OnInlineDispositionWebContentsCreated(
    content::WebContents* web_contents) {
  if (web_contents) {
    intents_dispatcher_->DispatchIntent(web_contents);
  } else {
    // TODO(binji): web_contents should never be NULL; it is in this case
    // because views doesn't have an implementation for inline disposition yet.
    // Remove this else when it does. In the meantime, just reforward as if it
    // were the window disposition.
    OnServiceChosen(picker_model_->inline_disposition_index(),
                    WebIntentPickerModel::DISPOSITION_WINDOW);
  }
}

void WebIntentPickerController::OnCancelled() {
  if (!intents_dispatcher_)
    return;

  if (service_tab_) {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED, string16());
  } else {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_PICKER_CANCELLED, string16());
  }

  ClosePicker();
}

void WebIntentPickerController::OnClosing() {
  picker_shown_ = false;
  picker_ = NULL;
}

void WebIntentPickerController::OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
  ClosePicker();

  if (service_tab_ &&
      reply_type != webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED) {
    int index = TabStripModel::kNoTab;
    Browser* browser = Browser::GetBrowserForController(
        &service_tab_->GetController(), &index);
    if (browser) {
      browser->tabstrip_model()->CloseTabContentsAt(
          index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

      // Activate source tab.
      Browser* source_browser =
          BrowserList::FindBrowserWithWebContents(wrapper_->web_contents());
      if (source_browser) {
        int source_index =
            source_browser->tabstrip_model()->GetIndexOfTabContents(wrapper_);
        source_browser->ActivateTabAt(source_index, false);
      }
    }
    service_tab_ = NULL;
  }

  intents_dispatcher_ = NULL;
}

void WebIntentPickerController::OnIntentsQueryDone(
    WebIntentsRegistry::QueryID,
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  FaviconService* favicon_service = GetFaviconService(wrapper_);
  for (size_t i = 0; i < services.size(); ++i) {
    picker_model_->AddInstalledService(
        services[i].title,
        services[i].service_url,
        ConvertDisposition(services[i].disposition));

    pending_async_count_++;
    FaviconService::Handle handle = favicon_service->GetFaviconForURL(
        services[i].service_url,
        history::FAVICON,
        &favicon_consumer_,
        base::Bind(
            &WebIntentPickerController::OnFaviconDataAvailable,
            weak_ptr_factory_.GetWeakPtr()));
    favicon_consumer_.SetClientData(favicon_service, handle, i);
  }

  AsyncOperationFinished();
}

void WebIntentPickerController::OnFaviconDataAvailable(
    FaviconService::Handle handle, history::FaviconData favicon_data) {
  size_t index = favicon_consumer_.GetClientDataForCurrentRequest();
  if (favicon_data.is_valid()) {
    SkBitmap icon_bitmap;

    if (gfx::PNGCodec::Decode(favicon_data.image_data->front(),
                              favicon_data.image_data->size(),
                              &icon_bitmap)) {
      gfx::Image icon_image(new SkBitmap(icon_bitmap));
      picker_model_->UpdateFaviconAt(index, icon_image);
      return;
    }
  }

  AsyncOperationFinished();
}

void WebIntentPickerController::OnCWSIntentServicesAvailable(
    const CWSIntentsRegistry::IntentExtensionList& extensions) {
  for (size_t i = 0; i < extensions.size(); ++i) {
    const CWSIntentsRegistry::IntentExtensionInfo& info = extensions[i];
    picker_model_->AddSuggestedExtension(
        info.name,
        info.id,
        info.average_rating);

    // TODO(binji): Fetch extension icon.
  }

  AsyncOperationFinished();
}

void WebIntentPickerController::AsyncOperationFinished() {
  if (--pending_async_count_ == 0) {
    picker_->OnPendingAsyncCompleted();
  }
}

void WebIntentPickerController::ClosePicker() {
  if (picker_) {
    picker_->Close();
  }
}
