// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"

#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {
// Time interval when media sink service sends sinks to MRP.
const int kFetchCompleteTimeoutSecs = 3;
}

namespace media_router {

DialMediaSinkService::DialMediaSinkService(
    const OnSinksDiscoveredCallback& callback,
    net::URLRequestContextGetter* request_context)
    : MediaSinkService(callback), request_context_(request_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_);
}

DialMediaSinkService::~DialMediaSinkService() {}

void DialMediaSinkService::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  dial_registry()->RegisterObserver(this);
  dial_registry()->StartPeriodicDiscovery();
}

void DialMediaSinkService::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  dial_registry()->UnregisterObserver(this);
}

DialRegistry* DialMediaSinkService::dial_registry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return DialRegistry::GetInstance();
}

DeviceDescriptionService* DialMediaSinkService::GetDescriptionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!description_service_.get()) {
    description_service_.reset(new DeviceDescriptionService(
        base::Bind(&DialMediaSinkService::OnDeviceDescriptionAvailable,
                   base::Unretained(this)),
        base::Bind(&DialMediaSinkService::OnDeviceDescriptionError,
                   base::Unretained(this))));
  }
  return description_service_.get();
}

void DialMediaSinkService::OnDialDeviceEvent(
    const DialRegistry::DeviceList& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "DialMediaSinkService::OnDialDeviceEvent found " << devices.size()
           << " devices";

  // Add a finish timer.
  finish_timer_.reset(new base::OneShotTimer());
  base::TimeDelta finish_delay =
      base::TimeDelta::FromSeconds(kFetchCompleteTimeoutSecs);
  finish_timer_->Start(FROM_HERE, finish_delay, this,
                       &DialMediaSinkService::OnFetchCompleted);

  current_sinks_.clear();
  current_devices_ = devices;

  GetDescriptionService()->GetDeviceDescriptions(devices,
                                                 request_context_.get());
}

void DialMediaSinkService::OnDialError(DialRegistry::DialErrorCode type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "OnDialError [DialErrorCode]: " << static_cast<int>(type);
}

void DialMediaSinkService::OnDeviceDescriptionAvailable(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& description_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::ContainsValue(current_devices_, device_data)) {
    DVLOG(2) << "Device data not found in current device data list...";
    return;
  }

  // When use this "sink" within browser, please note it will have a different
  // ID when it is sent to the extension, because it derives a different sink ID
  // using the given sink ID.
  MediaSink sink(description_data.unique_id, description_data.friendly_name,
                 MediaSink::IconType::GENERIC);
  DialSinkExtraData extra_data;
  extra_data.app_url = description_data.app_url;
  extra_data.model_name = description_data.model_name;
  std::string ip_address = device_data.device_description_url().host();
  if (!extra_data.ip_address.AssignFromIPLiteral(ip_address)) {
    DVLOG(1) << "Invalid ip_address: " << ip_address;
    return;
  }

  current_sinks_.insert(MediaSinkInternal(sink, extra_data));

  if (finish_timer_)
    return;

  // Start fetch timer again if device description comes back after
  // |finish_timer_| fires.
  base::TimeDelta finish_delay =
      base::TimeDelta::FromSeconds(kFetchCompleteTimeoutSecs);
  finish_timer_.reset(new base::OneShotTimer());
  finish_timer_->Start(FROM_HERE, finish_delay, this,
                       &DialMediaSinkService::OnFetchCompleted);
}

void DialMediaSinkService::OnDeviceDescriptionError(
    const DialDeviceData& device,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "OnDescriptionFetchesError [message]: " << error_message;
}

void DialMediaSinkService::OnFetchCompleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!sink_discovery_callback_.is_null());

  finish_timer_.reset();

  auto sinks = current_sinks_;
  if (sinks == mrp_sinks_) {
    DVLOG(2) << "No update to sink list.";
    return;
  }

  DVLOG(2) << "Send sinks to media router, [size]: " << sinks.size();
  sink_discovery_callback_.Run(
      std::vector<MediaSinkInternal>(sinks.begin(), sinks.end()));
  mrp_sinks_ = std::move(sinks);
}

}  // namespace media_router
