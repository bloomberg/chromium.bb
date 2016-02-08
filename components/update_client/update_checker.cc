// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/utils.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace update_client {

namespace {

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
    base::StringAppendF(&app, "appid=\"%s\" version=\"%s\"", item->id.c_str(),
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

  return BuildProtocolRequest(
      config.GetBrowserVersion().GetString(), config.GetChannel(),
      config.GetLang(), config.GetOSLongName(), config.GetDownloadPreference(),
      app_elements, additional_attributes);
}

class UpdateCheckerImpl : public UpdateChecker {
 public:
  explicit UpdateCheckerImpl(const scoped_refptr<Configurator>& config);
  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  bool CheckForUpdates(
      const std::vector<CrxUpdateItem*>& items_to_check,
      const std::string& additional_attributes,
      const UpdateCheckCallback& update_check_callback) override;

 private:
  void OnRequestSenderComplete(const net::URLFetcher* source);

  const scoped_refptr<Configurator> config_;
  UpdateCheckCallback update_check_callback_;
  scoped_ptr<RequestSender> request_sender_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerImpl);
};

UpdateCheckerImpl::UpdateCheckerImpl(const scoped_refptr<Configurator>& config)
    : config_(config) {}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool UpdateCheckerImpl::CheckForUpdates(
    const std::vector<CrxUpdateItem*>& items_to_check,
    const std::string& additional_attributes,
    const UpdateCheckCallback& update_check_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (request_sender_.get()) {
    NOTREACHED();
    return false;  // Another update check is in progress.
  }

  update_check_callback_ = update_check_callback;

  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send(
      BuildUpdateCheckRequest(*config_, items_to_check, additional_attributes),
      config_->UpdateUrl(),
      base::Bind(&UpdateCheckerImpl::OnRequestSenderComplete,
                 base::Unretained(this)));
  return true;
}

void UpdateCheckerImpl::OnRequestSenderComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL original_url;
  int error = 0;
  std::string error_message;
  UpdateResponse update_response;

  if (source) {
    original_url = source->GetOriginalURL();
    VLOG(1) << "Update check request went to: " << original_url.spec();
    if (FetchSuccess(*source)) {
      std::string xml;
      source->GetResponseAsString(&xml);
      if (!update_response.Parse(xml)) {
        error = -1;
        error_message = update_response.errors();
      }
    } else {
      error = GetFetchError(*source);
      error_message.assign("network error");
    }
  } else {
    error = -1;
    error_message = "no fetcher";
  }

  if (error) {
    VLOG(1) << "Update request failed: " << error_message;
  }

  request_sender_.reset();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(update_check_callback_, original_url, error,
                            error_message, update_response.results()));
}

}  // namespace

scoped_ptr<UpdateChecker> UpdateChecker::Create(
    const scoped_refptr<Configurator>& config) {
  return scoped_ptr<UpdateChecker>(new UpdateCheckerImpl(config));
}

}  // namespace update_client
