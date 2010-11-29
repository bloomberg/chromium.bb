// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Web Request Events.

#ifndef CEEE_IE_PLUGIN_BHO_WEBREQUEST_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_WEBREQUEST_EVENTS_FUNNEL_H_

#include <atlcomcli.h>

#include "base/time.h"
#include "ceee/ie/plugin/bho/events_funnel.h"

#include "toolband.h"  // NOLINT

// Implements a set of methods to send web request related events to the
// Broker.
class WebRequestEventsFunnel : public EventsFunnel {
 public:
  WebRequestEventsFunnel() {}
  explicit WebRequestEventsFunnel(IEventSender* client)
      : EventsFunnel(client) {}

  // Sends the webRequest.onBeforeRedirect event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the current request.
  // @param status_code Standard HTTP status code returned by the server.
  // @param redirect_url The new URL.
  // @param time_stamp The time when the browser was about to make the redirect.
  virtual HRESULT OnBeforeRedirect(int request_id,
                                   const wchar_t* url,
                                   DWORD status_code,
                                   const wchar_t* redirect_url,
                                   const base::Time& time_stamp);

  // Sends the webRequest.onBeforeRequest event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the request.
  // @param method Standard HTTP method, such as "GET" or "POST".
  // @param tab_handle The window handle of the tab in which the request takes
  //        place. Set to INVALID_HANDLE_VALUE if the request isn't related to a
  //        tab.
  // @param type How the requested resource will be used, such as "main_frame"
  //        or "sub_frame". Please find the complete list and explanation on
  //        http://www.chromium.org/developers/design-documents/extensions/notifications-of-web-request-and-navigation
  // @param time_stamp The time when the browser was about to make the request.
  virtual HRESULT OnBeforeRequest(int request_id,
                                  const wchar_t* url,
                                  const char* method,
                                  CeeeWindowHandle tab_handle,
                                  const char* type,
                                  const base::Time& time_stamp);

  // Sends the webRequest.onCompleted event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the request.
  // @param status_code Standard HTTP status code returned by the server.
  // @param time_stamp The time when the response was received completely.
  virtual HRESULT OnCompleted(int request_id,
                              const wchar_t* url,
                              DWORD status_code,
                              const base::Time& time_stamp);

  // Sends the webRequest.onErrorOccurred event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the request.
  // @param error The error description.
  // @param time_stamp The time when the error occurred.
  virtual HRESULT OnErrorOccurred(int request_id,
                                  const wchar_t* url,
                                  const wchar_t* error,
                                  const base::Time& time_stamp);

  // Sends the webRequest.onHeadersReceived event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the request.
  // @param status_code Standard HTTP status code returned by the server.
  // @param time_stamp The time when the status line and response headers were
  //        received.
  virtual HRESULT OnHeadersReceived(int request_id,
                                    const wchar_t* url,
                                    DWORD status_code,
                                    const base::Time& time_stamp);

  // Sends the webRequest.onRequestSent event to the broker.
  // @param request_id The ID of the request.
  // @param url The URL of the request.
  // @param ip The server IP address that is actually connected to.
  // @param time_stamp The time when the browser finished sending the request.
  virtual HRESULT OnRequestSent(int request_id,
                                const wchar_t* url,
                                const char* ip,
                                const base::Time& time_stamp);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestEventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_WEBREQUEST_EVENTS_FUNNEL_H_
