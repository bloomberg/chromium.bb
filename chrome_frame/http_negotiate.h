// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HTTP_NEGOTIATE_H_
#define CHROME_FRAME_HTTP_NEGOTIATE_H_

#include <shdeprecated.h>
#include <urlmon.h>

#include <string>

#include "base/basictypes.h"
#include "base/win/scoped_comptr.h"

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

  // Enables and disables the User Agent header modification. It is enabled
  // by default when the patch is installed.
  static void set_modify_user_agent(bool value) { modify_user_agent_ = value; }

  // IHttpNegotiate patch methods
  static STDMETHODIMP BeginningTransaction(
      IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
      LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers);

 protected:
  static HRESULT PatchHttpNegotiate(IUnknown* to_patch);

 private:
  static bool modify_user_agent_;

  DISALLOW_COPY_AND_ASSIGN(HttpNegotiatePatch);
};

// From the latest urlmon.h. Symbol name prepended with LOCAL_ to
// avoid conflict (and therefore build errors) for those building with
// a newer Windows SDK.
// TODO(robertshield): Remove this once we update our SDK version.
extern const int LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE;

// Scans |additional_headers| and |headers| for User-Agent header or grabs
// the default User-Agent if none is found. Append "chromeframe" at the end
// of the string. Returns the original content of |additional_headers| but
// with the new User-Agent header. Somewhat unusual interface is dictated
// because we use it in IHttpNegotiate::BeginningTransaction.
// The function is a public since we want to use it from
// UrlmonUrlRequest::BeginningTransaction for the unusual but yet possible case
// when |headers| contains an User-Agent string.
std::string AppendCFUserAgentString(LPCWSTR headers,
                                    LPCWSTR additional_headers);

// Adds or replaces the User-Agent header in a set of HTTP headers.
// Arguments are the same as with AppendCFUserAgentString.
std::string ReplaceOrAddUserAgent(LPCWSTR headers,
                                  const std::string& user_agent_value);

#endif  // CHROME_FRAME_HTTP_NEGOTIATE_H_
