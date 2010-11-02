// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEEE_IE_PLUGIN_BHO_HTTP_NEGOTIATE_H_
#define CEEE_IE_PLUGIN_BHO_HTTP_NEGOTIATE_H_

#include <atlbase.h>
#include <shdeprecated.h>

#include "base/basictypes.h"

// Typedefs for IHttpNegotiate methods.
typedef HRESULT (STDMETHODCALLTYPE* IHttpNegotiate_BeginningTransaction_Fn)(
    IHttpNegotiate* me, LPCWSTR url, LPCWSTR headers, DWORD reserved,
    LPWSTR* additional_headers);
typedef HRESULT (STDMETHODCALLTYPE* IHttpNegotiate_OnResponse_Fn)(
    IHttpNegotiate* me, DWORD response_code, LPCWSTR response_header,
    LPCWSTR request_header, LPWSTR* additional_request_headers);

// Typedefs for IBindStatusCallback methods.
typedef HRESULT (STDMETHODCALLTYPE* IBindStatusCallback_StartBinding_Fn)(
    IBindStatusCallback* me, DWORD reserved, IBinding *binding);
typedef HRESULT (STDMETHODCALLTYPE* IBindStatusCallback_OnProgress_Fn)(
    IBindStatusCallback* me, ULONG progress, ULONG progress_max,
    ULONG status_code, LPCWSTR status_text);

// Typedefs for IInternetProtocolSink methods.
typedef HRESULT (STDMETHODCALLTYPE* IInternetProtocolSink_ReportProgress_Fn)(
    IInternetProtocolSink* me, ULONG status_code, LPCWSTR status_text);

// Patches methods of urlmon's IHttpNegotiate implementation for the purposes
// of adding to the http user agent header.

class HttpNegotiatePatch {
 private:
  // Class is not to be instantiated at the moment.
  HttpNegotiatePatch();
  ~HttpNegotiatePatch();

 public:
  static bool Initialize();
  static void Uninitialize();

  // IHttpNegotiate patch methods
  static STDMETHODIMP BeginningTransaction(
      IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
      LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers);
  static STDMETHODIMP OnResponse(IHttpNegotiate_OnResponse_Fn original,
      IHttpNegotiate* me, DWORD response_code, LPCWSTR response_headers,
      LPCWSTR request_headers, LPWSTR* additional_request_headers);

 protected:
  static HRESULT PatchHttpNegotiate(IUnknown* to_patch);

 private:
  // Count number of BHOs depending on this patch.
  // Unhook when the last one goes away.
  static CComAutoCriticalSection bho_instance_count_crit_;
  static int bho_instance_count_;

  DISALLOW_COPY_AND_ASSIGN(HttpNegotiatePatch);
};

// Attempts to get to the associated browser service for an active request.
HRESULT GetBrowserServiceFromProtocolSink(IInternetProtocolSink* sink,
                                          IBrowserService** browser_service);

#endif  // CEEE_IE_PLUGIN_BHO_HTTP_NEGOTIATE_H_
