// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Events from whereever through the Broker.

#include "ceee/ie/plugin/bho/webrequest_events_funnel.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"

namespace keys = extension_webrequest_api_constants;

namespace {

double MilliSecondsFromTime(const base::Time& time) {
  return base::Time::kMillisecondsPerSecond * time.ToDoubleT();
}

}  // namespace

HRESULT WebRequestEventsFunnel::OnBeforeRedirect(int request_id,
                                                 const wchar_t* url,
                                                 DWORD status_code,
                                                 const wchar_t* redirect_url,
                                                 const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetString(keys::kRedirectUrlKey, redirect_url);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnBeforeRedirect, args);
}

HRESULT WebRequestEventsFunnel::OnBeforeRequest(int request_id,
                                                const wchar_t* url,
                                                const char* method,
                                                CeeeWindowHandle tab_handle,
                                                const char* type,
                                                const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kMethodKey, method);
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kTypeKey, type);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnBeforeRequest, args);
}

HRESULT WebRequestEventsFunnel::OnCompleted(int request_id,
                                            const wchar_t* url,
                                            DWORD status_code,
                                            const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnCompleted, args);
}

HRESULT WebRequestEventsFunnel::OnErrorOccurred(int request_id,
                                                const wchar_t* url,
                                                const wchar_t* error,
                                                const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kErrorKey, error);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnErrorOccurred, args);
}

HRESULT WebRequestEventsFunnel::OnHeadersReceived(
    int request_id,
    const wchar_t* url,
    DWORD status_code,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnHeadersReceived, args);
}

HRESULT WebRequestEventsFunnel::OnRequestSent(int request_id,
                                              const wchar_t* url,
                                              const char* ip,
                                              const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kIpKey, ip);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnRequestSent, args);
}
