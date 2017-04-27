// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/updater_state.h"
#include "components/update_client/utils.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    default:
      return "unknown";
  }
}

// Returns a string representing a sequence of download complete events
// corresponding to each download metrics in |item|.
std::string BuildDownloadCompleteEventElements(const Component& component) {
  using base::StringAppendF;
  std::string download_events;
  for (const auto& metrics : component.download_metrics()) {
    std::string event("<event eventtype=\"14\"");
    StringAppendF(&event, " eventresult=\"%d\"", metrics.error == 0);
    StringAppendF(&event, " downloader=\"%s\"",
                  DownloaderToString(metrics.downloader));
    if (metrics.error) {
      StringAppendF(&event, " errorcode=\"%d\"", metrics.error);
    }
    StringAppendF(&event, " url=\"%s\"", metrics.url.spec().c_str());

    // -1 means that the  byte counts are not known.
    if (metrics.downloaded_bytes != -1) {
      StringAppendF(&event, " downloaded=\"%s\"",
                    base::Int64ToString(metrics.downloaded_bytes).c_str());
    }
    if (metrics.total_bytes != -1) {
      StringAppendF(&event, " total=\"%s\"",
                    base::Int64ToString(metrics.total_bytes).c_str());
    }

    if (metrics.download_time_ms) {
      StringAppendF(&event, " download_time_ms=\"%s\"",
                    base::Uint64ToString(metrics.download_time_ms).c_str());
    }
    StringAppendF(&event, "/>");

    download_events += event;
  }
  return download_events;
}

// Returns a string representing one ping event for the update of a component.
// The event type for this ping event is 3.
std::string BuildUpdateCompleteEventElement(const Component& component) {
  DCHECK(component.state() == ComponentState::kUpdateError ||
         component.state() == ComponentState::kUpdated);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"3\"");
  const int event_result = component.state() == ComponentState::kUpdated;
  StringAppendF(&ping_event, " eventresult=\"%d\"", event_result);
  if (component.error_category())
    StringAppendF(&ping_event, " errorcat=\"%d\"", component.error_category());
  if (component.error_code())
    StringAppendF(&ping_event, " errorcode=\"%d\"", component.error_code());
  if (component.extra_code1())
    StringAppendF(&ping_event, " extracode1=\"%d\"", component.extra_code1());
  if (HasDiffUpdate(component))
    StringAppendF(&ping_event, " diffresult=\"%d\"",
                  !component.diff_update_failed());
  if (component.diff_error_category()) {
    StringAppendF(&ping_event, " differrorcat=\"%d\"",
                  component.diff_error_category());
  }
  if (component.diff_error_code())
    StringAppendF(&ping_event, " differrorcode=\"%d\"",
                  component.diff_error_code());
  if (component.diff_extra_code1()) {
    StringAppendF(&ping_event, " diffextracode1=\"%d\"",
                  component.diff_extra_code1());
  }
  if (!component.previous_fp().empty())
    StringAppendF(&ping_event, " previousfp=\"%s\"",
                  component.previous_fp().c_str());
  if (!component.next_fp().empty())
    StringAppendF(&ping_event, " nextfp=\"%s\"", component.next_fp().c_str());
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

// Returns a string representing one ping event for the uninstall of a
// component. The event type for this ping event is 4.
std::string BuildUninstalledEventElement(const Component& component) {
  DCHECK(component.state() == ComponentState::kUninstalled);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"4\" eventresult=\"1\"");
  if (component.extra_code1())
    StringAppendF(&ping_event, " extracode1=\"%d\"", component.extra_code1());
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

// Builds a ping message for the specified component.
std::string BuildPing(const Configurator& config, const Component& component) {
  const char app_element_format[] =
      "<app appid=\"%s\" version=\"%s\" nextversion=\"%s\">"
      "%s"
      "%s"
      "</app>";

  std::string ping_event;
  switch (component.state()) {
    case ComponentState::kUpdateError:  // Fall through.
    case ComponentState::kUpdated:
      ping_event = BuildUpdateCompleteEventElement(component);
      break;
    case ComponentState::kUninstalled:
      ping_event = BuildUninstalledEventElement(component);
      break;
    default:
      NOTREACHED();
      break;
  }

  const std::string app_element(base::StringPrintf(
      app_element_format,
      component.id().c_str(),                            // "appid"
      component.previous_version().GetString().c_str(),  // "version"
      component.next_version().GetString().c_str(),      // "nextversion"
      ping_event.c_str(),                                // ping event
      BuildDownloadCompleteEventElements(component)
          .c_str()));  // download events

  // The ping request does not include any updater state.
  return BuildProtocolRequest(
      config.GetProdId(), config.GetBrowserVersion().GetString(),
      config.GetChannel(), config.GetLang(), config.GetOSLongName(),
      config.GetDownloadPreference(), app_element, "", nullptr);
}

// Sends a fire and forget ping. The instances of this class have no
// ownership and they self-delete upon completion. One instance of this class
// can send only one ping.
class PingSender {
 public:
  explicit PingSender(const scoped_refptr<Configurator>& config);
  ~PingSender();

  bool SendPing(const Component& component);

 private:
  void OnRequestSenderComplete(int error,
                               const std::string& response,
                               int retry_after_sec);

  const scoped_refptr<Configurator> config_;
  std::unique_ptr<RequestSender> request_sender_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PingSender);
};

PingSender::PingSender(const scoped_refptr<Configurator>& config)
    : config_(config) {}

PingSender::~PingSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PingSender::OnRequestSenderComplete(int error,
                                         const std::string& response,
                                         int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

bool PingSender::SendPing(const Component& component) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto urls(config_->PingUrl());
  if (component.crx_component().requires_network_encryption)
    RemoveUnsecureUrls(&urls);

  if (urls.empty())
    return false;

  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send(
      false, BuildPing(*config_, component), urls,
      base::Bind(&PingSender::OnRequestSenderComplete, base::Unretained(this)));
  return true;
}

}  // namespace

PingManager::PingManager(const scoped_refptr<Configurator>& config)
    : config_(config) {}

PingManager::~PingManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool PingManager::SendPing(const Component& component) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto ping_sender = base::MakeUnique<PingSender>(config_);
  if (!ping_sender->SendPing(component))
    return false;

  // The ping sender object self-deletes after sending the ping asynchrously.
  ping_sender.release();
  return true;
}

}  // namespace update_client
