// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/test_helper.h"

#include "base/base64.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/media_router/media_source.h"
#include "url/gurl.h"

namespace media_router {

std::string PresentationConnectionMessageToString(
    const content::PresentationConnectionMessage& message) {
  if (!message.message && !message.data)
    return "null";
  std::string result;
  if (message.message) {
    result = "text=";
    base::EscapeJSONString(*message.message, true, &result);
  } else {
    const base::StringPiece src(
        reinterpret_cast<const char*>(message.data->data()),
        message.data->size());
    base::Base64Encode(src, &result);
    result = "binary=" + result;
  }
  return result;
}

MockIssuesObserver::MockIssuesObserver(IssueManager* issue_manager)
    : IssuesObserver(issue_manager) {}
MockIssuesObserver::~MockIssuesObserver() {}

MockMediaSinksObserver::MockMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}
MockMediaSinksObserver::~MockMediaSinksObserver() {}

MockMediaRoutesObserver::MockMediaRoutesObserver(
    MediaRouter* router,
    const MediaSource::Id source_id)
    : MediaRoutesObserver(router, source_id) {}
MockMediaRoutesObserver::~MockMediaRoutesObserver() {}

MockPresentationConnectionProxy::MockPresentationConnectionProxy() {}
MockPresentationConnectionProxy::~MockPresentationConnectionProxy() {}

#if !defined(OS_ANDROID)
MockDialMediaSinkService::MockDialMediaSinkService(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : DialMediaSinkService(request_context) {}
MockDialMediaSinkService::~MockDialMediaSinkService() = default;

MockCastMediaSinkService::MockCastMediaSinkService(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : CastMediaSinkService(request_context) {}
MockCastMediaSinkService::~MockCastMediaSinkService() = default;
#endif  // !defined(OS_ANDROID)

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.10%d", num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

MediaSinkInternal CreateDialSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("id %d", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::GENERIC);
  media_router::DialSinkExtraData extra_data;
  extra_data.ip_address = ip_endpoint.address();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.app_url =
      GURL(base::StringPrintf("http://192.168.0.10%d/apps", num));
  return media_router::MediaSinkInternal(sink, extra_data);
}

}  // namespace media_router
