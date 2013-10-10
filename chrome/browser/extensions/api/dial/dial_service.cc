// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_service.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"
#if defined(OS_CHROMEOS)
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using net::HttpResponseHeaders;
using net::HttpUtil;
using net::IOBufferWithSize;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::NetworkInterface;
using net::NetworkInterfaceList;
using net::StringIOBuffer;
using net::UDPSocket;

namespace extensions {

namespace {

// The total number of requests to make per discovery cycle.
const int kDialMaxRequests = 4;

// The interval to wait between successive requests.
const int kDialRequestIntervalMillis = 1000;

// The timeout (MX) set for responses.
const int kDialResponseTimeoutSecs = 2;

// The multicast IP address for discovery.
const char kDialRequestAddress[] = "239.255.255.250";

// The UDP port number for discovery.
const int kDialRequestPort = 1900;

// The DIAL service type as part of the search request.
const char kDialSearchType[] = "urn:dial-multiscreen-org:service:dial:1";

// SSDP headers parsed from the response.
const char kSsdpLocationHeader[] = "LOCATION";
const char kSsdpCacheControlHeader[] = "CACHE-CONTROL";
const char kSsdpConfigIdHeader[] = "CONFIGID.UPNP.ORG";
const char kSsdpUsnHeader[] = "USN";

// The receive buffer size, in bytes.
const int kDialRecvBufferSize = 1500;

// Gets a specific header from |headers| and puts it in |value|.
bool GetHeader(HttpResponseHeaders* headers, const char* name,
               std::string* value) {
  return headers->EnumerateHeader(NULL, std::string(name), value);
}

// Returns the request string.
std::string BuildRequest() {
  // Extra line at the end to make UPnP lib happy.
  chrome::VersionInfo version;
  std::string request(base::StringPrintf(
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST:%s:%i\r\n"
      "MAN:\"ssdp:discover\"\r\n"
      "MX:%d\r\n"
      "ST:%s\r\n"
      "USER-AGENT:%s %s\r\n"
      "\r\n",
      kDialRequestAddress,
      kDialRequestPort,
      kDialResponseTimeoutSecs,
      kDialSearchType,
      version.ProductNameAndVersionForUserAgent().c_str(),
      version.OSType().c_str()));
  // 1500 is a good MTU value for most Ethernet LANs.
  DCHECK(request.size() <= 1500);
  return request;
}

}  // namespace

DialServiceImpl::DialServiceImpl(net::NetLog* net_log)
  : is_writing_(false),
    is_reading_(false),
    discovery_active_(false),
    num_requests_sent_(0),
    max_requests_(kDialMaxRequests),
    finish_delay_(TimeDelta::FromMilliseconds((kDialMaxRequests - 1) *
                                              kDialRequestIntervalMillis) +
                  TimeDelta::FromSeconds(kDialResponseTimeoutSecs)),
    request_interval_(TimeDelta::FromMilliseconds(kDialRequestIntervalMillis)) {
  IPAddressNumber address;
  bool result = net::ParseIPLiteralToNumber(kDialRequestAddress, &address);
  DCHECK(result);
  send_address_ = IPEndPoint(address, kDialRequestPort);
  send_buffer_ = new StringIOBuffer(BuildRequest());
  net_log_ = net_log;
  net_log_source_.type = net::NetLog::SOURCE_UDP_SOCKET;
  net_log_source_.id = net_log_->NextID();
}

DialServiceImpl::~DialServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DialServiceImpl::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void DialServiceImpl::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

bool DialServiceImpl::HasObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return observer_list_.HasObserver(observer);
}

bool DialServiceImpl::Discover() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (discovery_active_)
    return false;
  discovery_active_ = true;

  DVLOG(1) << "Discovery started.";

  StartDiscovery();
  return true;
}

void DialServiceImpl::StartDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(discovery_active_);
  if (socket_.get())
    return;

  BindSocketAndSendRequest();
}

bool DialServiceImpl::BindSocketAndSendRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!socket_.get());

  net::RandIntCallback rand_cb = base::Bind(&base::RandInt);
  socket_.reset(new UDPSocket(net::DatagramSocket::RANDOM_BIND,
                              rand_cb,
                              net_log_,
                              net_log_source_));
  socket_->AllowBroadcast();
  socket_->AllowAddressReuse();

  // Schedule a timer to finish the discovery process (and close the socket).
  if (finish_delay_ > TimeDelta::FromSeconds(0)) {
    finish_timer_.Start(FROM_HERE,
                        finish_delay_,
                        this,
                        &DialServiceImpl::FinishDiscovery);
  }

  // 0 means bind a random port
  IPEndPoint address(IPAddressNumber(net::kIPv4AddressSize), 0);

  if (!CheckResult("Bind", socket_->Bind(address)))
    return false;

  DCHECK(socket_.get());

  recv_buffer_ = new IOBufferWithSize(kDialRecvBufferSize);
  if (!ReadSocket())
    return false;
  SendOneRequest();
  return true;
}

void DialServiceImpl::SendOneRequest() {
  if (num_requests_sent_ == max_requests_) {
    request_timer_.Stop();
    return;
  }
  num_requests_sent_++;
  if (!socket_.get()) {
    DLOG(WARNING) << "Socket not connected.";
    return;
  }

  if (is_writing_) {
    DVLOG(1) << "Already writing.";
    return;
  }
  DVLOG(1) << "Sending request " << num_requests_sent_ << "/"
           << max_requests_;
  is_writing_ = true;
  int result = socket_->SendTo(
      send_buffer_.get(), send_buffer_->size(), send_address_,
      base::Bind(&DialServiceImpl::OnSocketWrite, AsWeakPtr()));
  bool result_ok = CheckResult("SendTo", result);
  if (result_ok && result > 0) {
    // Synchronous write.
    OnSocketWrite(result);
  }
}

void DialServiceImpl::OnSocketWrite(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_writing_ = false;
  if (!CheckResult("OnSocketWrite", result))
    return;

  if (result != send_buffer_->size()) {
    DLOG(ERROR) << "Sent " << result << " chars, expected "
               << send_buffer_->size() << " chars";
  }
  // If discovery is inactive, no reason to notify observers.
  if (!discovery_active_) {
    DVLOG(1) << "Request sent after discovery finished.  Ignoring.";
    return;
  }
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDiscoveryRequest(this));
  // If we need to send additional requests, schedule a timer to do so.
  if (num_requests_sent_ < max_requests_ && num_requests_sent_ == 1) {
    request_timer_.Start(FROM_HERE,
                         request_interval_,
                         this,
                         &DialServiceImpl::SendOneRequest);
  }
}

bool DialServiceImpl::ReadSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!socket_.get()) {
    DLOG(WARNING) << "Socket not connected.";
    return false;
  }

  if (is_reading_) {
    DVLOG(1) << "Already reading.";
    return false;
  }

  int result = net::OK;
  bool result_ok = true;
  do {
    is_reading_ = true;
    result = socket_->RecvFrom(
        recv_buffer_.get(),
        kDialRecvBufferSize, &recv_address_,
        base::Bind(&DialServiceImpl::OnSocketRead, AsWeakPtr()));
    result_ok = CheckResult("RecvFrom", result);
    if (result != net::ERR_IO_PENDING)
      is_reading_ = false;
    if (result_ok && result > 0) {
      // Synchronous read.
      HandleResponse(result);
    }
  } while (result_ok && result != net::OK && result != net::ERR_IO_PENDING);
  return result_ok;
}

void DialServiceImpl::OnSocketRead(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_reading_ = false;
  if (!CheckResult("OnSocketRead", result))
    return;
  if (result > 0)
    HandleResponse(result);

  // Await next response.
  ReadSocket();
}

void DialServiceImpl::HandleResponse(int bytes_read) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(bytes_read, 0);
  if (bytes_read > kDialRecvBufferSize) {
    DLOG(ERROR) << bytes_read << " > " << kDialRecvBufferSize << "!?";
    return;
  }
  DVLOG(1) << "Read " << bytes_read << " bytes from "
           << recv_address_.ToString();

  // If discovery is inactive, no reason to handle response.
  if (!discovery_active_) {
    DVLOG(1) << "Got response after discovery finished.  Ignoring.";
    return;
  }

  std::string response(recv_buffer_->data(), bytes_read);
  Time response_time = Time::Now();

  // Attempt to parse response, notify observers if successful.
  DialDeviceData parsed_device;
  if (ParseResponse(response, response_time, &parsed_device))
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnDeviceDiscovered(this, parsed_device));
}

// static
bool DialServiceImpl::ParseResponse(const std::string& response,
                                    const base::Time& response_time,
                                    DialDeviceData* device) {
  int headers_end = HttpUtil::LocateEndOfHeaders(response.c_str(),
                                                 response.size());
  if (headers_end < 1) {
    DVLOG(1) << "Headers invalid or empty, ignoring: " << response;
    return false;
  }
  std::string raw_headers =
      HttpUtil::AssembleRawHeaders(response.c_str(), headers_end);
  DVLOG(1) << "raw_headers: " << raw_headers << "\n";
  scoped_refptr<HttpResponseHeaders> headers =
      new HttpResponseHeaders(raw_headers);

  std::string device_url_str;
  if (!GetHeader(headers.get(), kSsdpLocationHeader, &device_url_str) ||
      device_url_str.empty()) {
    DVLOG(1) << "No LOCATION header found.";
    return false;
  }

  GURL device_url(device_url_str);
  if (!DialDeviceData::IsDeviceDescriptionUrl(device_url)) {
    DVLOG(1) << "URL " << device_url_str << " not valid.";
    return false;
  }

  std::string device_id;
  if (!GetHeader(headers.get(), kSsdpUsnHeader, &device_id) ||
      device_id.empty()) {
    DVLOG(1) << "No USN header found.";
    return false;
  }

  device->set_device_id(device_id);
  device->set_device_description_url(device_url);
  device->set_response_time(response_time);

  // TODO(mfoltz): Parse the max-age value from the cache control header.
  // http://crbug.com/165289
  std::string cache_control;
  GetHeader(headers.get(), kSsdpCacheControlHeader, &cache_control);

  std::string config_id;
  int config_id_int;
  if (GetHeader(headers.get(), kSsdpConfigIdHeader, &config_id) &&
      base::StringToInt(config_id, &config_id_int)) {
    device->set_config_id(config_id_int);
  } else {
    DVLOG(1) << "Malformed or missing " << kSsdpConfigIdHeader << ": "
             << config_id;
  }

  return true;
}

void DialServiceImpl::FinishDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(discovery_active_);
  DVLOG(1) << "Discovery finished.";
  CloseSocket();
  finish_timer_.Stop();
  request_timer_.Stop();
  discovery_active_ = false;
  num_requests_sent_ = 0;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDiscoveryFinished(this));
}

void DialServiceImpl::CloseSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_reading_ = false;
  is_writing_ = false;
  socket_.reset();
}

bool DialServiceImpl::CheckResult(const char* operation, int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Operation " << operation << " result " << result;
  if (result < net::OK && result != net::ERR_IO_PENDING) {
    CloseSocket();
    std::string error_str(net::ErrorToString(result));
    DVLOG(0) << "dial socket error: " << error_str;
    // TODO(justinlin): More granular socket errors.
    FOR_EACH_OBSERVER(
        Observer, observer_list_, OnError(this, DIAL_SERVICE_SOCKET_ERROR));
    return false;
  }
  return true;
}

}  // namespace extensions
