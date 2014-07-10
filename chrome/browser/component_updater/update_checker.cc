// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/update_checker.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/component_updater/component_updater_configurator.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace component_updater {

// Builds an update check request for |components|. |additional_attributes| is
// serialized as part of the <request> element of the request to customize it
// with data that is not platform or component specific. For each |item|, a
// corresponding <app> element is created and inserted as a child node of
// the <request>.
//
// An app element looks like this:
//    <app appid="hnimpnehoodheedghdeeijklkeaacbdc"
//         version="0.1.2.3" installsource="ondemand">
//      <updatecheck />
//      <packages>
//        <package fp="abcd" />
//      </packages>
//    </app>
std::string BuildUpdateCheckRequest(const Configurator& config,
                                    const std::vector<CrxUpdateItem*>& items,
                                    const std::string& additional_attributes) {
  std::string app_elements;
  for (size_t i = 0; i != items.size(); ++i) {
    const CrxUpdateItem* item = items[i];
    std::string app("<app ");
    base::StringAppendF(&app,
                        "appid=\"%s\" version=\"%s\"",
                        item->id.c_str(),
                        item->component.version.GetString().c_str());
    if (item->on_demand)
      base::StringAppendF(&app, " installsource=\"ondemand\"");
    base::StringAppendF(&app, ">");
    base::StringAppendF(&app, "<updatecheck />");
    if (!item->component.fingerprint.empty()) {
      base::StringAppendF(&app,
                          "<packages>"
                          "<package fp=\"%s\"/>"
                          "</packages>",
                          item->component.fingerprint.c_str());
    }
    base::StringAppendF(&app, "</app>");
    app_elements.append(app);
    VLOG(1) << "Appending to update request: " << app;
  }

  return BuildProtocolRequest(config.GetBrowserVersion().GetString(),
                              config.GetChannel(),
                              config.GetLang(),
                              config.GetOSLongName(),
                              app_elements,
                              additional_attributes);
}

class UpdateCheckerImpl : public UpdateChecker, public net::URLFetcherDelegate {
 public:
  UpdateCheckerImpl(const Configurator& config,
                    const UpdateCheckCallback& update_check_callback);
  virtual ~UpdateCheckerImpl();

  // Overrides for UpdateChecker.
  virtual bool CheckForUpdates(
      const std::vector<CrxUpdateItem*>& items_to_check,
      const std::string& additional_attributes) OVERRIDE;

  // Overrides for UrlFetcher.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  const Configurator& config_;
  const UpdateCheckCallback update_check_callback_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerImpl);
};

scoped_ptr<UpdateChecker> UpdateChecker::Create(
    const Configurator& config,
    const UpdateCheckCallback& update_check_callback) {
  scoped_ptr<UpdateCheckerImpl> update_checker(
      new UpdateCheckerImpl(config, update_check_callback));
  return update_checker.PassAs<UpdateChecker>();
}

UpdateCheckerImpl::UpdateCheckerImpl(
    const Configurator& config,
    const UpdateCheckCallback& update_check_callback)
    : config_(config), update_check_callback_(update_check_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool UpdateCheckerImpl::CheckForUpdates(
    const std::vector<CrxUpdateItem*>& items_to_check,
    const std::string& additional_attributes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (url_fetcher_)
    return false;  // Another fetch is in progress.

  url_fetcher_.reset(SendProtocolRequest(
      config_.UpdateUrl(),
      BuildUpdateCheckRequest(config_, items_to_check, additional_attributes),
      this,
      config_.RequestContext()));

  return true;
}

void UpdateCheckerImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url_fetcher_.get() == source);

  int error = 0;
  std::string error_message;
  UpdateResponse update_response;

  if (FetchSuccess(*source)) {
    std::string xml;
    source->GetResponseAsString(&xml);
    if (!update_response.Parse(xml)) {
      error = -1;
      error_message = update_response.errors();
      VLOG(1) << "Update request failed: " << error_message;
    }
  } else {
    error = GetFetchError(*source);
    error_message.assign("network error");
    VLOG(1) << "Update request failed: network error";
  }

  url_fetcher_.reset();
  update_check_callback_.Run(error, error_message, update_response.results());
}

}  // namespace component_updater
