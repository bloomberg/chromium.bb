// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_info_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace {

media_router::SinkAppStatus GetAppStatusFromAppInfo(
    const media_router::ParsedDialAppInfo& app_info) {
  if (app_info.state == media_router::DialAppState::kRunning ||
      app_info.state == media_router::DialAppState::kStopped) {
    return media_router::SinkAppStatus::kAvailable;
  }

  return media_router::SinkAppStatus::kUnavailable;
}

std::string GetRequestId(const std::string& sink_id,
                         const std::string& app_name) {
  return sink_id + ':' + app_name;
}

GURL GetAppUrl(const media_router::MediaSinkInternal& sink,
               const std::string& app_name) {
  // The DIAL spec (Section 5.4) implies that the app URL must not have a
  // trailing slash.
  GURL partial_app_url = sink.dial_data().app_url;
  return GURL(partial_app_url.spec() + "/" + app_name);
}

}  // namespace

namespace media_router {

DialAppDiscoveryService::DialAppDiscoveryService(
    service_manager::Connector* connector,
    const DialAppInfoParseCompletedCallback& parse_completed_cb)
    : parse_completed_cb_(parse_completed_cb),
      parser_(std::make_unique<SafeDialAppInfoParser>(connector)) {}

DialAppDiscoveryService::~DialAppDiscoveryService() = default;

// Always query the device to get current app status.
void DialAppDiscoveryService::FetchDialAppInfo(
    const MediaSinkInternal& sink,
    const std::string& app_name,
    net::URLRequestContextGetter* request_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string sink_id = sink.sink().id();
  std::string request_id = GetRequestId(sink_id, app_name);
  GURL app_url = GetAppUrl(sink, app_name);
  DVLOG(2) << "Fetch DIAL app info from: " << app_url.spec();

  std::unique_ptr<DialAppInfoFetcher> fetcher =
      std::make_unique<DialAppInfoFetcher>(
          app_url, request_context,
          base::BindOnce(&DialAppDiscoveryService::OnDialAppInfoFetchComplete,
                         base::Unretained(this), sink_id, app_name),
          base::BindOnce(&DialAppDiscoveryService::OnDialAppInfoFetchError,
                         base::Unretained(this), sink_id, app_name));
  fetcher->Start();
  pending_fetcher_map_.emplace(request_id, std::move(fetcher));
}

void DialAppDiscoveryService::SetParserForTest(
    std::unique_ptr<SafeDialAppInfoParser> parser) {
  parser_ = std::move(parser);
}

void DialAppDiscoveryService::OnDialAppInfoParsed(
    const std::string& sink_id,
    const std::string& app_name,
    std::unique_ptr<ParsedDialAppInfo> parsed_app_info,
    SafeDialAppInfoParser::ParsingResult parsing_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string request_id = GetRequestId(sink_id, app_name);
  pending_fetcher_map_.erase(request_id);

  if (!parsed_app_info) {
    DVLOG(2) << "Failed to parse app info XML in utility process, error: "
             << parsing_result;
    parse_completed_cb_.Run(sink_id, app_name, SinkAppStatus::kUnavailable);
    return;
  }

  SinkAppStatus app_status = GetAppStatusFromAppInfo(*parsed_app_info);
  DVLOG(2) << "Get parsed DIAL app info from utility process, [sink_id]: "
           << sink_id << " [name]: " << app_name << " [status]: " << app_status;
  parse_completed_cb_.Run(sink_id, app_name, app_status);
}

void DialAppDiscoveryService::OnDialAppInfoFetchComplete(
    const std::string& sink_id,
    const std::string& app_name,
    const std::string& app_info_xml) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  parser_->Parse(app_info_xml,
                 base::BindOnce(&DialAppDiscoveryService::OnDialAppInfoParsed,
                                base::Unretained(this), sink_id, app_name));
}

void DialAppDiscoveryService::OnDialAppInfoFetchError(
    const std::string& sink_id,
    const std::string& app_name,
    int response_code,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(2) << "Fail to fetch app info XML for: " << sink_id
           << " due to error: " << error_message
           << " response code: " << response_code;

  if (response_code == net::HTTP_NOT_FOUND ||
      response_code >= net::HTTP_INTERNAL_SERVER_ERROR ||
      response_code == net::HTTP_OK) {
    parse_completed_cb_.Run(sink_id, app_name, SinkAppStatus::kUnavailable);
  }

  std::string request_id = GetRequestId(sink_id, app_name);
  pending_fetcher_map_.erase(request_id);
}

}  // namespace media_router
