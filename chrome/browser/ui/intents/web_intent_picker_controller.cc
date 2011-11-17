// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_controller.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/browser/intents/intent_injector.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "ui/gfx/codec/png_codec.h"
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

}  // namespace

// A class that asynchronously fetches web intent data from the web intents
// registry.
class WebIntentPickerController::WebIntentDataFetcher
    : public WebIntentsRegistry::Consumer {
 public:
  WebIntentDataFetcher(WebIntentPickerController* controller,
                       WebIntentsRegistry* web_intents_registry);
  ~WebIntentDataFetcher();

  // Asynchronously fetches WebIntentServiceData for the given
  // |action|, |type| pair.
  void Fetch(const string16& action, const string16& type);

  // Cancels the WebDataService request.
  void Cancel();

 private:
  // WebIntentsRegistry::Consumer implementation.
  virtual void OnIntentsQueryDone(
      WebIntentsRegistry::QueryID,
      const std::vector<webkit_glue::WebIntentServiceData>& services) OVERRIDE;

  // A weak pointer to the picker controller.
  WebIntentPickerController* controller_;

  // A weak pointer to the web intents registry.
  WebIntentsRegistry* web_intents_registry_;

  // The ID of the current web intents request. The value will be -1 if
  // there is no request in flight.
  WebIntentsRegistry::QueryID query_id_;
};

// A class that asynchronously fetches favicons for a vector of URLs.
class WebIntentPickerController::FaviconFetcher {
 public:
  FaviconFetcher(WebIntentPickerController* controller,
                 FaviconService *favicon_service);
  ~FaviconFetcher();

  // Asynchronously fetches favicons for the each URL in |urls|.
  void Fetch(const std::vector<GURL>& urls);

  // Cancels all favicon requests.
  void Cancel();

 private:
  // Callback called when a favicon is received.
  void OnFaviconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon_data);

  // A weak pointer to the picker controller.
  WebIntentPickerController* controller_;

  // A weak pointer to the favicon service.
  FaviconService* favicon_service_;

  // The Consumer to handle asynchronous favicon requests.
  CancelableRequestConsumerTSimple<size_t> load_consumer_;

  DISALLOW_COPY_AND_ASSIGN(FaviconFetcher);
};

WebIntentPickerController::WebIntentPickerController(
    TabContentsWrapper* wrapper,
    WebIntentPickerFactory* factory)
        : wrapper_(wrapper),
          picker_factory_(factory),
          web_intent_data_fetcher_(
              new WebIntentDataFetcher(this,
                                       GetWebIntentsRegistry(wrapper))),
          favicon_fetcher_(
              new FaviconFetcher(this, GetFaviconService(wrapper))),
          picker_(NULL),
          pending_async_count_(0),
          routing_id_(0),
          intent_id_(0),
          service_tab_(NULL) {
  NavigationController* controller = &wrapper->controller();
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_TAB_CLOSING,
                 content::Source<NavigationController>(controller));
}

WebIntentPickerController::~WebIntentPickerController() {
}

void WebIntentPickerController::SetIntent(
    int routing_id,
    const webkit_glue::WebIntentData& intent,
    int intent_id) {
  routing_id_ = routing_id;
  intent_ = intent;
  intent_id_ = intent_id;
}

void WebIntentPickerController::ShowDialog(Browser* browser,
                                           const string16& action,
                                           const string16& type) {
  if (picker_ != NULL)
    return;

  picker_ = picker_factory_->Create(browser, wrapper_, this);

  // TODO(binji) Remove this check when there are implementations of the picker
  // for windows and mac.
  if (picker_ == NULL)
    return;

  web_intent_data_fetcher_->Fetch(action, type);
}

void WebIntentPickerController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_START ||
         type == content::NOTIFICATION_TAB_CLOSING);
  ClosePicker();
}

// Used to forward messages to the source tab from the service context.
// Also watches the source tab contents, and if it closes, doesn't forward
// messages.
class InvokingTabObserver : public TabContentsObserver {
 public:
  InvokingTabObserver(TabContentsWrapper* wrapper,
                      IntentInjector* injector,
                      int routing_id)
      : TabContentsObserver(wrapper->tab_contents()),
        wrapper_(wrapper),
        intent_injector_(injector),
        routing_id_(routing_id) {}
  virtual ~InvokingTabObserver() {}

  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE {
    if (intent_injector_)
      intent_injector_->SourceTabContentsDestroyed(tab);
    wrapper_ = NULL;
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    // The injector can return exactly one message. After that we don't talk
    // to it again, since it may have deleted itself.
    intent_injector_ = NULL;

    if (!wrapper_)
      return false;

    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&WebIntentPickerController::OnSendReturnMessage,
                   base::Unretained(wrapper_->web_intent_picker_controller())));

    message->set_routing_id(routing_id_);
    return wrapper_->Send(message);
  }

 private:
  // Weak pointer to the source tab invoking the intent.
  TabContentsWrapper* wrapper_;

  // Weak pointer to the intent injector managing delivering the intent data to
  // the service tab.
  IntentInjector* intent_injector_;

  // Renderer-side object invoking the intent.
  int routing_id_;
};

void WebIntentPickerController::OnServiceChosen(size_t index) {
  DCHECK(index < urls_.size());

  bool inline_disposition = service_data_[index].disposition ==
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE;
  TabContents* new_tab_contents = NULL;
  if (inline_disposition)
    new_tab_contents = picker_->SetInlineDisposition(urls_[index]);

  if (new_tab_contents == NULL) {
    // TODO(gbillock): This really only handles the 'window' disposition in a
    // quite prototype way. We need to flesh out what happens to the picker
    // during the lifetime of the service url context, and that may mean we
    // need to pass more information into the injector to find the picker again
    // and close it. Also: the above conditional construction is just because
    // there isn't Mac/Win support yet. When that's there, it'll be an else.
    browser::NavigateParams params(NULL, urls_[index],
                                   content::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = NEW_FOREGROUND_TAB;
    params.profile = wrapper_->profile();
    browser::Navigate(&params);
    new_tab_contents = params.target_contents->tab_contents();
    service_tab_ = new_tab_contents;

    ClosePicker();
  }

  IntentInjector* injector = new IntentInjector(new_tab_contents);
  injector->SetIntent(new InvokingTabObserver(wrapper_, injector, routing_id_),
                      intent_,
                      intent_id_);
}

void WebIntentPickerController::OnCancelled() {
  InvokingTabObserver forwarder(wrapper_, NULL, routing_id_);
  if (service_tab_) {
    forwarder.Send(new IntentsMsg_WebIntentReply(
        0, webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED, string16(), intent_id_));
  } else {
    forwarder.Send(new IntentsMsg_WebIntentReply(
        0, webkit_glue::WEB_INTENT_PICKER_CANCELLED, string16(), intent_id_));
  }

  ClosePicker();
}

void WebIntentPickerController::OnClosing() {
}

void WebIntentPickerController::OnSendReturnMessage() {
  ClosePicker();

  if (service_tab_) {
    int index = TabStripModel::kNoTab;
    Browser* browser = Browser::GetBrowserForController(
        &service_tab_->controller(), &index);
    if (browser) {
      browser->tabstrip_model()->CloseTabContentsAt(
          index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
    }
    service_tab_ = NULL;
  }
}

void WebIntentPickerController::OnWebIntentDataAvailable(
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  urls_.clear();
  for (size_t i = 0; i < services.size(); ++i) {
    urls_.push_back(services[i].service_url);
  }
  service_data_ = services;

  // Tell the picker to initialize N urls to the default favicon
  picker_->SetServiceURLs(urls_);
  favicon_fetcher_->Fetch(urls_);
  pending_async_count_--;
}

void WebIntentPickerController::OnFaviconDataAvailable(
    size_t index,
    const SkBitmap& icon_bitmap) {
  picker_->SetServiceIcon(index, icon_bitmap);
  pending_async_count_--;
}

void WebIntentPickerController::OnFaviconDataUnavailable(size_t index) {
  picker_->SetDefaultServiceIcon(index);
  pending_async_count_--;
}

void WebIntentPickerController::ClosePicker() {
  if (picker_) {
    picker_factory_->ClosePicker(picker_);
    picker_ = NULL;
  }
}

WebIntentPickerController::WebIntentDataFetcher::WebIntentDataFetcher(
    WebIntentPickerController* controller,
    WebIntentsRegistry* web_intents_registry)
        : controller_(controller),
          web_intents_registry_(web_intents_registry),
          query_id_(-1) {
}

WebIntentPickerController::WebIntentDataFetcher::~WebIntentDataFetcher() {
}

void WebIntentPickerController::WebIntentDataFetcher::Fetch(
    const string16& action,
    const string16& type) {
  DCHECK(query_id_ == -1) << "Fetch already in process.";
  controller_->pending_async_count_++;
  query_id_ = web_intents_registry_->GetIntentProviders(action, type, this);
}

void WebIntentPickerController::WebIntentDataFetcher::OnIntentsQueryDone(
    WebIntentsRegistry::QueryID,
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  controller_->OnWebIntentDataAvailable(services);
  query_id_ = -1;
}

WebIntentPickerController::FaviconFetcher::FaviconFetcher(
    WebIntentPickerController* controller,
    FaviconService* favicon_service)
        : controller_(controller),
          favicon_service_(favicon_service) {
}

WebIntentPickerController::FaviconFetcher::~FaviconFetcher() {
}

void WebIntentPickerController::FaviconFetcher::Fetch(
    const std::vector<GURL>& urls) {
  if (!favicon_service_)
    return;

  for (size_t index = 0; index < urls.size(); ++index) {
    controller_->pending_async_count_++;
    FaviconService::Handle handle = favicon_service_->GetFaviconForURL(
        urls[index],
        history::FAVICON,
        &load_consumer_,
        base::Bind(
            &WebIntentPickerController::FaviconFetcher::OnFaviconDataAvailable,
            base::Unretained(this)));
    load_consumer_.SetClientData(favicon_service_, handle, index);
  }
}

void WebIntentPickerController::FaviconFetcher::Cancel() {
  load_consumer_.CancelAllRequests();
}

void WebIntentPickerController::FaviconFetcher::OnFaviconDataAvailable(
    FaviconService::Handle handle,
    history::FaviconData favicon_data) {
  size_t index = load_consumer_.GetClientDataForCurrentRequest();
  if (favicon_data.is_valid()) {
    SkBitmap icon_bitmap;

    if (gfx::PNGCodec::Decode(favicon_data.image_data->front(),
                              favicon_data.image_data->size(),
                              &icon_bitmap)) {
      controller_->OnFaviconDataAvailable(index, icon_bitmap);
      return;
    }
  }

  controller_->OnFaviconDataUnavailable(index);
}
