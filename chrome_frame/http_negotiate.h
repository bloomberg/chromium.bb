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

// Patches methods of urlmon's IHttpNegotiate implementation for the purposes
// of adding to the http user agent header.
class HttpNegotiatePatch {
  // class is not to be instantiated atm.
  HttpNegotiatePatch();
  ~HttpNegotiatePatch();

 public:
  static bool Initialize();
  static void Uninitialize();

  static STDMETHODIMP BeginningTransaction(
      IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
      LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers);

  static STDMETHODIMP OnResponse(
      IHttpNegotiate_OnResponse_Fn original, IHttpNegotiate* me,
      DWORD response_code, LPCWSTR response_header, LPCWSTR request_header,
      LPWSTR* additional_request_headers);

 protected:
  static HRESULT PatchHttpNegotiate(IUnknown* to_patch);

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpNegotiatePatch);
};

#endif  // CHROME_FRAME_HTTP_NEGOTIATE_H_
