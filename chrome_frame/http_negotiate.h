// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HTTP_NEGOTIATE_H_
#define CHROME_FRAME_HTTP_NEGOTIATE_H_

#include <urlmon.h>

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

// Typedefs for IInternetProtocolSink methods.
typedef HRESULT (STDMETHODCALLTYPE* IInternetProtocolSink_ReportProgress_Fn)(
    IInternetProtocolSink* me, ULONG status_code, LPCWSTR status_text);

// Patches methods of urlmon's IHttpNegotiate implementation for the purposes
// of adding to the http user agent header.

// Also patches one of the IBindStatusCallback implementations in urlmon to pick
// up an IBinding during the StartBinding call. The IBinding implementor then
// gets a patch applied to its IInternetProtocolSink's ReportProgress method.
// The patched is there so that the reporting of the MIME type to the IBinding
// implementor can be changed if an X-Chrome-Frame HTTP header is present
// in the response headers. If anyone can suggest a more straightforward way of
// doing this, I would be eternally grateful.
class HttpNegotiatePatch {
  // class is not to be instantiated atm.
  HttpNegotiatePatch();
  ~HttpNegotiatePatch();

 public:
  static bool Initialize();
  static void Uninitialize();

  // IHttpNegotiate patch methods
  static STDMETHODIMP BeginningTransaction(
      IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
      LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers);
  static STDMETHODIMP OnResponse(
      IHttpNegotiate_OnResponse_Fn original, IHttpNegotiate* me,
      DWORD response_code, LPCWSTR response_header, LPCWSTR request_header,
      LPWSTR* additional_request_headers);

  // IBindStatusCallback patch methods
  static STDMETHODIMP StartBinding(IBindStatusCallback_StartBinding_Fn original,
      IBindStatusCallback* me, DWORD reserved, IBinding *binding);

  // IInternetProtocolSink patch methods
  static STDMETHODIMP ReportProgress(
      IInternetProtocolSink_ReportProgress_Fn original,
      IInternetProtocolSink* me, ULONG status_code, LPCWSTR status_text);

 protected:
  static HRESULT PatchHttpNegotiate(IUnknown* to_patch);

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpNegotiatePatch);
};

#endif  // CHROME_FRAME_HTTP_NEGOTIATE_H_
