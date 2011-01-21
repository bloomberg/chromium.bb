// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <htiframe.h>
#include <mshtml.h>

#include "chrome_frame/protocol_sink_wrap.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/function_stub.h"
#include "chrome_frame/policy_settings.h"
#include "chrome_frame/utils.h"

// BINDSTATUS_SERVER_MIMETYPEAVAILABLE == 54. Introduced in IE 8, so
// not in everyone's headers yet. See:
// http://msdn.microsoft.com/en-us/library/ms775133(VS.85,loband).aspx
#ifndef BINDSTATUS_SERVER_MIMETYPEAVAILABLE
#define BINDSTATUS_SERVER_MIMETYPEAVAILABLE 54
#endif

bool ProtocolSinkWrap::ignore_xua_ = false;

static const char kTextHtmlMimeType[] = "text/html";
const wchar_t kUrlMonDllName[] = L"urlmon.dll";

static const int kInternetProtocolStartIndex = 3;
static const int kInternetProtocolReadIndex = 9;
static const int kInternetProtocolStartExIndex = 13;
static const int kInternetProtocolLockRequestIndex = 11;
static const int kInternetProtocolUnlockRequestIndex = 12;


// IInternetProtocol/Ex patches.
STDMETHODIMP Hook_Start(InternetProtocol_Start_Fn orig_start,
                        IInternetProtocol* protocol,
                        LPCWSTR url,
                        IInternetProtocolSink* prot_sink,
                        IInternetBindInfo* bind_info,
                        DWORD flags,
                        HANDLE_PTR reserved);

STDMETHODIMP Hook_StartEx(InternetProtocol_StartEx_Fn orig_start_ex,
                          IInternetProtocolEx* protocol,
                          IUri* uri,
                          IInternetProtocolSink* prot_sink,
                          IInternetBindInfo* bind_info,
                          DWORD flags,
                          HANDLE_PTR reserved);

STDMETHODIMP Hook_Read(InternetProtocol_Read_Fn orig_read,
                       IInternetProtocol* protocol,
                       void* buffer,
                       ULONG size,
                       ULONG* size_read);

STDMETHODIMP Hook_LockRequest(InternetProtocol_LockRequest_Fn orig_req,
                              IInternetProtocol* protocol, DWORD dwOptions);

STDMETHODIMP Hook_UnlockRequest(InternetProtocol_UnlockRequest_Fn orig_req,
                                IInternetProtocol* protocol);

/////////////////////////////////////////////////////////////////////////////
BEGIN_VTABLE_PATCHES(CTransaction)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, Hook_Start)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, Hook_Read)
  VTABLE_PATCH_ENTRY(kInternetProtocolLockRequestIndex, Hook_LockRequest)
  VTABLE_PATCH_ENTRY(kInternetProtocolUnlockRequestIndex, Hook_UnlockRequest)
END_VTABLE_PATCHES()

BEGIN_VTABLE_PATCHES(CTransaction2)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartExIndex, Hook_StartEx)
END_VTABLE_PATCHES()

//
// ProtocolSinkWrap implementation

// Static map initialization
ProtData::ProtocolDataMap ProtData::datamap_;
Lock ProtData::datamap_lock_;

ProtocolSinkWrap::ProtocolSinkWrap() {
  DVLOG(1) << __FUNCTION__ << base::StringPrintf(" 0x%08X", this);
}

ProtocolSinkWrap::~ProtocolSinkWrap() {
  DVLOG(1) << __FUNCTION__ << base::StringPrintf(" 0x%08X", this);
}

ScopedComPtr<IInternetProtocolSink> ProtocolSinkWrap::CreateNewSink(
    IInternetProtocolSink* sink, ProtData* data) {
  DCHECK(sink != NULL);
  DCHECK(data != NULL);
  CComObject<ProtocolSinkWrap>* new_sink = NULL;
  CComObject<ProtocolSinkWrap>::CreateInstance(&new_sink);
  new_sink->delegate_ = sink;
  new_sink->prot_data_ = data;
  return ScopedComPtr<IInternetProtocolSink>(new_sink);
}

// IInternetProtocolSink methods
STDMETHODIMP ProtocolSinkWrap::Switch(PROTOCOLDATA* protocol_data) {
  HRESULT hr = E_FAIL;
  if (delegate_)
    hr = delegate_->Switch(protocol_data);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportProgress(ULONG status_code,
                                              LPCWSTR status_text) {
  DVLOG(1) << "ProtocolSinkWrap::ReportProgress: "
           << BindStatus2Str(status_code)
           << " Status: " << (status_text ? status_text : L"");

  HRESULT hr = prot_data_->ReportProgress(delegate_, status_code, status_text);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportData(DWORD flags, ULONG progress,
    ULONG max_progress) {
  DCHECK(delegate_);
  DVLOG(1) << "ProtocolSinkWrap::ReportData: " << Bscf2Str(flags)
           << " progress: " << progress << " progress_max: " << max_progress;

  HRESULT hr = prot_data_->ReportData(delegate_, flags, progress, max_progress);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportResult(HRESULT result, DWORD error,
    LPCWSTR result_text) {
  DVLOG(1) << "ProtocolSinkWrap::ReportResult: result: " << result
           << " error: " << error
           << " Text: " << (result_text ? result_text : L"");
  ExceptionBarrier barrier;
  HRESULT hr = prot_data_->ReportResult(delegate_, result, error, result_text);
  return hr;
}


// Helpers
ScopedComPtr<IBindCtx> BindCtxFromIBindInfo(IInternetBindInfo* bind_info) {
  LPOLESTR bind_ctx_string = NULL;
  ULONG count;
  ScopedComPtr<IBindCtx> bind_ctx;
  bind_info->GetBindString(BINDSTRING_PTR_BIND_CONTEXT, &bind_ctx_string, 1,
                           &count);
  if (bind_ctx_string) {
    int bind_ctx_int;
    base::StringToInt(bind_ctx_string, &bind_ctx_int);
    IBindCtx* pbc = reinterpret_cast<IBindCtx*>(bind_ctx_int);
    bind_ctx.Attach(pbc);
    CoTaskMemFree(bind_ctx_string);
  }

  return bind_ctx;
}

bool ShouldWrapSink(IInternetProtocolSink* sink, const wchar_t* url) {
  // Ignore everything that does not start with http:// or https://.
  // |url| is already normalized (i.e. no leading spaces, capital letters in
  // protocol etc) and non-null (we check in Hook_Start).
  DCHECK(url != NULL);

  if (ProtocolSinkWrap::ignore_xua())
    return false;  // No need to intercept, we're ignoring X-UA-Compatible tags

  if ((url != StrStrW(url, L"http://")) && (url != StrStrW(url, L"https://")))
    return false;

  ScopedComPtr<IHttpNegotiate> http_negotiate;
  HRESULT hr = DoQueryService(GUID_NULL, sink, http_negotiate.Receive());
  if (http_negotiate && !IsSubFrameRequest(http_negotiate))
    return true;

  return false;
}

// High level helpers
bool IsCFRequest(IBindCtx* pbc) {
  ScopedComPtr<BindContextInfo> info;
  BindContextInfo::FromBindContext(pbc, info.Receive());
  DCHECK(info);
  if (info && info->chrome_request())
    return true;

  return false;
}

bool HasProtData(IBindCtx* pbc) {
  ScopedComPtr<BindContextInfo> info;
  BindContextInfo::FromBindContext(pbc, info.Receive());
  bool result = false;
  if (info)
    result = info->has_prot_data();
  return result;
}

void PutProtData(IBindCtx* pbc, ProtData* data) {
  // AddRef and Release to avoid a potential leak of a ProtData instance if
  // FromBindContext fails.
  data->AddRef();
  ScopedComPtr<BindContextInfo> info;
  BindContextInfo::FromBindContext(pbc, info.Receive());
  if (info)
    info->set_prot_data(data);
  data->Release();
}

bool IsTextHtml(const wchar_t* status_text) {
  const std::wstring str = status_text;
  bool is_text_html = LowerCaseEqualsASCII(str, kTextHtmlMimeType);
  return is_text_html;
}

bool IsAdditionallySupportedContentType(const wchar_t* status_text) {
  static const char* kHeaderContentTypes[] = {
    "application/xhtml+xml",
    "application/xml",
    "image/svg",
    "image/svg+xml",
    "text/xml",
    "video/ogg",
    "video/webm",
    "video/mp4"
  };

  const std::wstring str = status_text;
  for (int i = 0; i < arraysize(kHeaderContentTypes); ++i) {
    if (LowerCaseEqualsASCII(str, kHeaderContentTypes[i]))
      return true;
  }

  if (PolicySettings::GetInstance()->GetRendererForContentType(
      status_text) == PolicySettings::RENDER_IN_CHROME_FRAME) {
    return true;
  }

  return false;
}

// Returns:
// RENDERER_TYPE_OTHER: if suggested mime type is not text/html.
// RENDERER_TYPE_UNDETERMINED: if suggested mime type is text/html.
// RENDERER_TYPE_CHROME_RESPONSE_HEADER: X-UA-Compatible tag is in HTTP headers.
// RENDERER_TYPE_CHROME_DEFAULT_RENDERER: GCF is the default renderer and the
//                                        Url is not listed in the
//                                        RenderInHostUrls registry key.
// RENDERER_TYPE_CHROME_OPT_IN_URL: GCF is not the default renderer and the Url
//                                  is listed in the RenderInGcfUrls registry
//                                  key.
RendererType DetermineRendererTypeFromMetaData(
    const wchar_t* suggested_mime_type,
    const std::wstring& url,
    IWinInetHttpInfo* info) {
  bool is_text_html = IsTextHtml(suggested_mime_type);
  bool is_supported_content_type = is_text_html ||
      IsAdditionallySupportedContentType(suggested_mime_type);

  if (!is_supported_content_type)
    return RENDERER_TYPE_OTHER;

  if (!url.empty()) {
    RendererType renderer_type = RendererTypeForUrl(url);
    if (IsChrome(renderer_type)) {
      return renderer_type;
    }
  }

  if (info) {
    char buffer[512] = "x-ua-compatible";
    DWORD len = sizeof(buffer);
    DWORD flags = 0;
    HRESULT hr = info->QueryInfo(HTTP_QUERY_CUSTOM, buffer, &len, &flags, NULL);

    if (hr == S_OK && len > 0) {
      if (CheckXUaCompatibleDirective(buffer, GetIEMajorVersion())) {
        return RENDERER_TYPE_CHROME_RESPONSE_HEADER;
      }
    }
  }

  // We can (and want) to sniff the content.
  if (is_text_html) {
    return RENDERER_TYPE_UNDETERMINED;
  }

  // We cannot sniff the content.
  return RENDERER_TYPE_OTHER;
}

RendererType DetermineRendererType(void* buffer, DWORD size, bool last_chance) {
  RendererType renderer_type = RENDERER_TYPE_UNDETERMINED;
  if (last_chance)
    renderer_type = RENDERER_TYPE_OTHER;

  std::wstring html_contents;
  // TODO(joshia): detect and handle different content encodings
  UTF8ToWide(reinterpret_cast<char*>(buffer), size, &html_contents);

  // Note that document_contents_ may have NULL characters in it. While
  // browsers may handle this properly, we don't and will stop scanning
  // for the XUACompat content value if we encounter one.
  std::wstring xua_compat_content;
  if (SUCCEEDED(UtilGetXUACompatContentValue(html_contents,
                                             &xua_compat_content))) {
    if (CheckXUaCompatibleDirective(WideToASCII(xua_compat_content),
                                    GetIEMajorVersion())) {
      renderer_type = RENDERER_TYPE_CHROME_HTTP_EQUIV;
    }
  }

  return renderer_type;
}

// ProtData
ProtData::ProtData(IInternetProtocol* protocol,
                   InternetProtocol_Read_Fn read_fun, const wchar_t* url)
    : has_suggested_mime_type_(false), has_server_mime_type_(false),
      buffer_size_(0), buffer_pos_(0),
      renderer_type_(RENDERER_TYPE_UNDETERMINED), protocol_(protocol),
      read_fun_(read_fun), url_(url) {
  memset(buffer_, 0, arraysize(buffer_));
  DVLOG(1) << __FUNCTION__ << " " << this;

  // Add to map.
  base::AutoLock lock(datamap_lock_);
  DCHECK(datamap_.end() == datamap_.find(protocol_));
  datamap_[protocol] = this;
}

ProtData::~ProtData() {
  DVLOG(1) << __FUNCTION__ << " " << this;
  Invalidate();
}

HRESULT ProtData::Read(void* buffer, ULONG size, ULONG* size_read) {
  if (renderer_type_ == RENDERER_TYPE_UNDETERMINED) {
    return E_PENDING;
  }

  const ULONG bytes_available = buffer_size_ - buffer_pos_;
  const ULONG bytes_to_copy = std::min(bytes_available, size);
  if (bytes_to_copy) {
    // Copy from the local buffer.
    memcpy(buffer, buffer_ + buffer_pos_, bytes_to_copy);
    *size_read = bytes_to_copy;
    buffer_pos_ += bytes_to_copy;

    HRESULT hr = S_OK;
    ULONG new_data = 0;
    if (size > bytes_available) {
      // User buffer is greater than what we have.
      buffer = reinterpret_cast<uint8*>(buffer) + bytes_to_copy;
      size -= bytes_to_copy;
      hr = read_fun_(protocol_, buffer, size, &new_data);
    }

    if (size_read)
      *size_read = bytes_to_copy + new_data;
    return hr;
  }

  return read_fun_(protocol_, buffer, size, size_read);
}

// Attempt to detect ChromeFrame from HTTP headers when
// BINDSTATUS_MIMETYPEAVAILABLE is received.
// There are three possible outcomes: CHROME_*/OTHER/UNDETERMINED.
// If RENDERER_TYPE_UNDETERMINED we are going to sniff the content later in
// ReportData().
//
// With not-so-well-written software (mime filters/protocols/protocol patchers)
// BINDSTATUS_MIMETYPEAVAILABLE might be fired multiple times with different
// values for the same client.
// If the renderer_type_ member is:
// RENDERER_TYPE_CHROME_* - 2nd (and any subsequent)
//                          BINDSTATUS_MIMETYPEAVAILABLE is ignored.
// RENDERER_TYPE_OTHER  - 2nd (and any subsequent) BINDSTATUS_MIMETYPEAVAILABLE
//                        is passed through.
// RENDERER_TYPE_UNDETERMINED - Try to detect ChromeFrame from HTTP headers
//                              (acts as if this is the first time
//                              BINDSTATUS_MIMETYPEAVAILABLE is received).
HRESULT ProtData::ReportProgress(IInternetProtocolSink* delegate,
                                 ULONG status_code, LPCWSTR status_text) {
  switch (status_code) {
    case BINDSTATUS_DIRECTBIND:
      renderer_type_ = RENDERER_TYPE_OTHER;
      break;

    case BINDSTATUS_REDIRECTING:
      url_.empty();
      if (status_text)
        url_ = status_text;
      break;

    case BINDSTATUS_SERVER_MIMETYPEAVAILABLE:
      has_server_mime_type_ = true;
      SaveSuggestedMimeType(status_text);
      return S_OK;

    // TODO(stoyan): BINDSTATUS_RAWMIMETYPE
    case BINDSTATUS_MIMETYPEAVAILABLE:
    case BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE:
      SaveSuggestedMimeType(status_text);
      // When Transaction is attached i.e. when existing BTS it terminated
      // and "converted" to BTO, events will be re-fired for the new sink,
      // but we may skip the renderer_type_ determination since it's already
      // done.
      if (renderer_type_ == RENDERER_TYPE_UNDETERMINED) {
        // This may seem awkward. CBinding's implementation of IWinInetHttpInfo
        // will forward to CTransaction that will forward to the real protocol.
        // We may ask CTransaction (our protocol_ member) for IWinInetHttpInfo.
        ScopedComPtr<IWinInetHttpInfo> info;
        info.QueryFrom(delegate);
        renderer_type_ = DetermineRendererTypeFromMetaData(suggested_mime_type_,
                                                           url_, info);
      }

      if (IsChrome(renderer_type_)) {
        // Suggested mime type is "text/html" and we have DEFAULT_RENDERER,
        // OPT_IN_URL, or RESPONSE_HEADER.
        DVLOG(1) << "Forwarding BINDSTATUS_MIMETYPEAVAILABLE "
                 << kChromeMimeType;
        SaveReferrer(delegate);
        delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);
      } else if (renderer_type_ == RENDERER_TYPE_OTHER) {
        // Suggested mime type is not "text/html" - we are not interested in
        // this request anymore.
        FireSuggestedMimeType(delegate);
      } else {
        // Suggested mime type is "text/html"; We will try to sniff the
        // HTML content in ReportData.
        DCHECK_EQ(RENDERER_TYPE_UNDETERMINED, renderer_type_);
      }
      return S_OK;
  }

  // We are just pass through at this point, avoid false positive crash reports.
  ExceptionBarrierReportOnlyModule barrier;
  return delegate->ReportProgress(status_code, status_text);
}

HRESULT ProtData::ReportData(IInternetProtocolSink* delegate,
                              DWORD flags, ULONG progress, ULONG max_progress) {
  if (renderer_type_ != RENDERER_TYPE_UNDETERMINED) {
    // We are just pass through now, avoid false positive crash reports.
    ExceptionBarrierReportOnlyModule barrier;
    return delegate->ReportData(flags, progress, max_progress);
  }

  HRESULT hr = FillBuffer();

  bool last_chance = false;
  if (hr == S_OK || hr == S_FALSE) {
    last_chance = true;
  }

  renderer_type_ = DetermineRendererType(buffer_, buffer_size_, last_chance);

  if (renderer_type_ == RENDERER_TYPE_UNDETERMINED) {
    // do not report anything, we need more data.
    return S_OK;
  }

  if (IsChrome(renderer_type_)) {
    DVLOG(1) << "Forwarding BINDSTATUS_MIMETYPEAVAILABLE " << kChromeMimeType;
    SaveReferrer(delegate);
    delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);
  }

  if (renderer_type_ == RENDERER_TYPE_OTHER) {
    FireSuggestedMimeType(delegate);
  }

  // This is the first data notification we forward, since up to now we hold
  // the content received.
  flags |= BSCF_FIRSTDATANOTIFICATION;

  if (hr == S_FALSE) {
    flags |= (BSCF_LASTDATANOTIFICATION | BSCF_DATAFULLYAVAILABLE);
  }

  return delegate->ReportData(flags, progress, max_progress);
}

HRESULT ProtData::ReportResult(IInternetProtocolSink* delegate, HRESULT result,
                               DWORD error, LPCWSTR result_text) {
  // We may receive ReportResult without ReportData, if the connection fails
  // for example.
  if (renderer_type_ == RENDERER_TYPE_UNDETERMINED) {
    DVLOG(1) << "ReportResult received but renderer type is yet unknown.";
    renderer_type_ = RENDERER_TYPE_OTHER;
    FireSuggestedMimeType(delegate);
  }

  HRESULT hr = S_OK;
  if (delegate)
    hr = delegate->ReportResult(result, error, result_text);
  return hr;
}


void ProtData::UpdateUrl(const wchar_t* url) {
  url_ = url;
}

// S_FALSE   - EOF
// S_OK      - buffer fully filled
// E_PENDING - some data added to buffer, but buffer is not yet full
// E_XXXX    - some other error.
HRESULT ProtData::FillBuffer() {
  HRESULT hr_read = S_OK;

  while ((hr_read == S_OK) && (buffer_size_ < kMaxContentSniffLength)) {
    ULONG size_read = 0;
    hr_read = read_fun_(protocol_, buffer_ + buffer_size_,
                       kMaxContentSniffLength - buffer_size_, &size_read);
    buffer_size_ += size_read;
  }

  return hr_read;
}

void ProtData::SaveSuggestedMimeType(LPCWSTR status_text) {
  has_suggested_mime_type_ = true;
  suggested_mime_type_.Allocate(status_text);
}

void ProtData::FireSuggestedMimeType(IInternetProtocolSink* delegate) {
  if (has_server_mime_type_) {
    DVLOG(1) << "Forwarding BINDSTATUS_SERVER_MIMETYPEAVAILABLE "
             << suggested_mime_type_;
    delegate->ReportProgress(BINDSTATUS_SERVER_MIMETYPEAVAILABLE,
                             suggested_mime_type_);
  }

  if (has_suggested_mime_type_) {
    DVLOG(1) << "Forwarding BINDSTATUS_MIMETYPEAVAILABLE "
             << suggested_mime_type_;
    delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE,
                             suggested_mime_type_);
  }
}

void ProtData::SaveReferrer(IInternetProtocolSink* delegate) {
  DCHECK(IsChrome(renderer_type_));
  ScopedComPtr<IWinInetHttpInfo> info;
  info.QueryFrom(delegate);
  if (info) {
    char buffer[4096] = {0};
    DWORD len = sizeof(buffer);
    DWORD flags = 0;
    HRESULT hr = info->QueryInfo(
        HTTP_QUERY_REFERER | HTTP_QUERY_FLAG_REQUEST_HEADERS,
        buffer, &len, &flags, 0);
    if (hr == S_OK && len > 0)
      referrer_.assign(buffer);
  } else {
    DLOG(WARNING) << "Failed to QI for IWinInetHttpInfo";
  }
}

scoped_refptr<ProtData> ProtData::DataFromProtocol(
    IInternetProtocol* protocol) {
  scoped_refptr<ProtData> instance;
  base::AutoLock lock(datamap_lock_);
  ProtocolDataMap::iterator it = datamap_.find(protocol);
  if (datamap_.end() != it)
    instance = it->second;
  return instance;
}

void ProtData::Invalidate() {
  if (protocol_) {
    // Remove from map.
    base::AutoLock lock(datamap_lock_);
    DCHECK(datamap_.end() != datamap_.find(protocol_));
    datamap_.erase(protocol_);
    protocol_ = NULL;
  }
}

// This function looks for the url pattern indicating that this request needs
// to be forced into chrome frame.
// This hack is required because window.open requests from Chrome don't have
// the URL up front. The URL comes in much later when the renderer initiates a
// top level navigation for the url passed into window.open.
// The new page must be rendered in ChromeFrame to preserve the opener
// relationship with its parent even if the new page does not have the chrome
// meta tag.
bool HandleAttachToExistingExternalTab(LPCWSTR url,
                                       IInternetProtocol* protocol,
                                       IInternetProtocolSink* prot_sink,
                                       IBindCtx* bind_ctx) {
  ChromeFrameUrl cf_url;
  if (cf_url.Parse(url) && cf_url.attach_to_external_tab()) {
    scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
    if (!prot_data) {
      // Pass NULL as the read function which indicates that always return EOF
      // without calling the underlying protocol.
      prot_data = new ProtData(protocol, NULL, url);
      PutProtData(bind_ctx, prot_data);
    }

    prot_sink->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);

    int data_flags = BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION;
    prot_sink->ReportData(data_flags, 0, 0);

    prot_sink->ReportResult(S_OK, 0, NULL);
    return true;
  }
  return false;
}

HRESULT ForwardHookStart(InternetProtocol_Start_Fn orig_start,
    IInternetProtocol* protocol, LPCWSTR url, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  ExceptionBarrierReportOnlyModule barrier;
  return orig_start(protocol, url, prot_sink, bind_info, flags, reserved);
}

HRESULT ForwardWrappedHookStart(InternetProtocol_Start_Fn orig_start,
    IInternetProtocol* protocol, LPCWSTR url, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  ExceptionBarrier barrier;
  return orig_start(protocol, url, prot_sink, bind_info, flags, reserved);
}

// IInternetProtocol/Ex hooks.
STDMETHODIMP Hook_Start(InternetProtocol_Start_Fn orig_start,
    IInternetProtocol* protocol, LPCWSTR url, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start);
  if (!url || !prot_sink || !bind_info)
    return E_INVALIDARG;
  DVLOG_IF(1, url != NULL) << "OnStart: " << url << PiFlags2Str(flags);

  ScopedComPtr<IBindCtx> bind_ctx = BindCtxFromIBindInfo(bind_info);
  if (!bind_ctx) {
    // MSHTML sometimes takes a short path, skips the creation of
    // moniker and binding, by directly grabbing protocol from InternetSession
    DVLOG(1) << "DirectBind for " << url;
    return ForwardHookStart(orig_start, protocol, url, prot_sink, bind_info,
                            flags, reserved);
  }

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data && !HasProtData(bind_ctx)) {
    prot_data->Invalidate();
    prot_data = NULL;
  }

  if (HandleAttachToExistingExternalTab(url, protocol, prot_sink, bind_ctx)) {
    return S_OK;
  }

  if (IsCFRequest(bind_ctx)) {
    ScopedComPtr<BindContextInfo> info;
    BindContextInfo::FromBindContext(bind_ctx, info.Receive());
    DCHECK(info);
    if (info) {
      info->set_protocol(protocol);
    }
    return ForwardHookStart(orig_start, protocol, url, prot_sink, bind_info,
                            flags, reserved);
  }

  if (prot_data) {
    DVLOG(1) << "Found existing ProtData!";
    prot_data->UpdateUrl(url);
    ScopedComPtr<IInternetProtocolSink> new_sink =
        ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
    return ForwardWrappedHookStart(orig_start, protocol, url, new_sink,
                                   bind_info, flags, reserved);
  }

  if (!ShouldWrapSink(prot_sink, url)) {
    return ForwardHookStart(orig_start, protocol, url, prot_sink, bind_info,
                            flags, reserved);
  }

  // Fresh request.
  InternetProtocol_Read_Fn read_fun = reinterpret_cast<InternetProtocol_Read_Fn>
      (CTransaction_PatchInfo[1].stub_->argument());
  prot_data = new ProtData(protocol, read_fun, url);
  PutProtData(bind_ctx, prot_data);

  ScopedComPtr<IInternetProtocolSink> new_sink =
      ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
  return ForwardWrappedHookStart(orig_start, protocol, url, new_sink, bind_info,
                                 flags, reserved);
}

HRESULT ForwardHookStartEx(InternetProtocol_StartEx_Fn orig_start_ex,
    IInternetProtocolEx* protocol, IUri* uri, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  ExceptionBarrierReportOnlyModule barrier;
  return orig_start_ex(protocol, uri, prot_sink, bind_info, flags, reserved);
}

HRESULT ForwardWrappedHookStartEx(InternetProtocol_StartEx_Fn orig_start_ex,
    IInternetProtocolEx* protocol, IUri* uri, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  ExceptionBarrier barrier;
  return orig_start_ex(protocol, uri, prot_sink, bind_info, flags, reserved);
}

STDMETHODIMP Hook_StartEx(InternetProtocol_StartEx_Fn orig_start_ex,
    IInternetProtocolEx* protocol, IUri* uri, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start_ex);
  if (!uri || !prot_sink || !bind_info)
    return E_INVALIDARG;

  base::win::ScopedBstr url;
  uri->GetPropertyBSTR(Uri_PROPERTY_ABSOLUTE_URI, url.Receive(), 0);
  DVLOG_IF(1, url != NULL) << "OnStartEx: " << url << PiFlags2Str(flags);

  ScopedComPtr<IBindCtx> bind_ctx = BindCtxFromIBindInfo(bind_info);
  if (!bind_ctx) {
    // MSHTML sometimes takes a short path, skips the creation of
    // moniker and binding, by directly grabbing protocol from InternetSession.
    DVLOG(1) << "DirectBind for " << url;
    return ForwardHookStartEx(orig_start_ex, protocol, uri, prot_sink,
                              bind_info, flags, reserved);
  }

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data && !HasProtData(bind_ctx)) {
    prot_data->Invalidate();
    prot_data = NULL;
  }

  if (HandleAttachToExistingExternalTab(url, protocol, prot_sink, bind_ctx)) {
    return S_OK;
  }

  if (IsCFRequest(bind_ctx)) {
    ScopedComPtr<BindContextInfo> info;
    BindContextInfo::FromBindContext(bind_ctx, info.Receive());
    DCHECK(info);
    if (info) {
      info->set_protocol(protocol);
    }
    return ForwardHookStartEx(orig_start_ex, protocol, uri, prot_sink,
                              bind_info, flags, reserved);
  }

  if (prot_data) {
    DVLOG(1) << "Found existing ProtData!";
    prot_data->UpdateUrl(url);
    ScopedComPtr<IInternetProtocolSink> new_sink =
        ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
    return ForwardWrappedHookStartEx(orig_start_ex, protocol, uri, new_sink,
                                     bind_info, flags, reserved);
  }

  if (!ShouldWrapSink(prot_sink, url)) {
    return ForwardHookStartEx(orig_start_ex, protocol, uri, prot_sink,
                              bind_info, flags, reserved);
  }

  // Fresh request.
  InternetProtocol_Read_Fn read_fun = reinterpret_cast<InternetProtocol_Read_Fn>
    (CTransaction_PatchInfo[1].stub_->argument());
  prot_data = new ProtData(protocol, read_fun, url);
  PutProtData(bind_ctx, prot_data);

  ScopedComPtr<IInternetProtocolSink> new_sink =
      ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
  return ForwardWrappedHookStartEx(orig_start_ex, protocol, uri, new_sink,
                                   bind_info, flags, reserved);
}

STDMETHODIMP Hook_Read(InternetProtocol_Read_Fn orig_read,
    IInternetProtocol* protocol, void* buffer, ULONG size, ULONG* size_read) {
  DCHECK(orig_read);
  HRESULT hr = E_FAIL;

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (!prot_data) {
    // We are not wrapping this request, avoid false positive crash reports.
    ExceptionBarrierReportOnlyModule barrier;
    hr = orig_read(protocol, buffer, size, size_read);
    return hr;
  }

  if (prot_data->is_attach_external_tab_request()) {
    // return EOF always.
    if (size_read)
      *size_read = 0;
    return S_FALSE;
  }

  hr = prot_data->Read(buffer, size, size_read);
  return hr;
}

STDMETHODIMP Hook_LockRequest(InternetProtocol_LockRequest_Fn orig_req,
                              IInternetProtocol* protocol, DWORD options) {
  DCHECK(orig_req);

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data && prot_data->is_attach_external_tab_request()) {
    prot_data->AddRef();
    return S_OK;
  }

  // We are just pass through at this point, avoid false positive crash
  // reports.
  ExceptionBarrierReportOnlyModule barrier;
  return orig_req(protocol, options);
}

STDMETHODIMP Hook_UnlockRequest(InternetProtocol_UnlockRequest_Fn orig_req,
                                IInternetProtocol* protocol) {
  DCHECK(orig_req);

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data && prot_data->is_attach_external_tab_request()) {
    prot_data->Release();
    return S_OK;
  }

  // We are just pass through at this point, avoid false positive crash
  // reports.
  ExceptionBarrierReportOnlyModule barrier;
  return orig_req(protocol);
}

// Patching / Hooking code.
class FakeProtocol : public CComObjectRootEx<CComSingleThreadModel>,
                     public IInternetProtocol {
 public:
  BEGIN_COM_MAP(FakeProtocol)
    COM_INTERFACE_ENTRY(IInternetProtocol)
    COM_INTERFACE_ENTRY(IInternetProtocolRoot)
  END_COM_MAP()

  STDMETHOD(Start)(LPCWSTR url, IInternetProtocolSink *protocol_sink,
      IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
    transaction_.QueryFrom(protocol_sink);
    // Return some unusual error code.
    return INET_E_INVALID_CERTIFICATE;
  }

  STDMETHOD(Continue)(PROTOCOLDATA* protocol_data) { return S_OK; }
  STDMETHOD(Abort)(HRESULT reason, DWORD options) { return S_OK; }
  STDMETHOD(Terminate)(DWORD options) { return S_OK; }
  STDMETHOD(Suspend)() { return S_OK; }
  STDMETHOD(Resume)() { return S_OK; }
  STDMETHOD(Read)(void *buffer, ULONG size, ULONG* size_read) { return S_OK; }
  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos)
    { return S_OK; }
  STDMETHOD(LockRequest)(DWORD options) { return S_OK; }
  STDMETHOD(UnlockRequest)() { return S_OK; }

  ScopedComPtr<IInternetProtocol> transaction_;
};

struct FakeFactory : public IClassFactory,
                     public CComObjectRootEx<CComSingleThreadModel> {
  BEGIN_COM_MAP(FakeFactory)
    COM_INTERFACE_ENTRY(IClassFactory)
  END_COM_MAP()

  STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) {
    if (pUnkOuter)
      return CLASS_E_NOAGGREGATION;
    HRESULT hr = obj_->QueryInterface(riid, ppvObj);
    return hr;
  }

  STDMETHOD(LockServer)(BOOL fLock) {
    return S_OK;
  }

  IUnknown* obj_;
};

static void HookTransactionVtable(IInternetProtocol* p) {
  ScopedComPtr<IInternetProtocolEx> ex;
  ex.QueryFrom(p);

  HRESULT hr = vtable_patch::PatchInterfaceMethods(p, CTransaction_PatchInfo);
  if (hr == S_OK && ex) {
    vtable_patch::PatchInterfaceMethods(ex.get(), CTransaction2_PatchInfo);
  }
}

void TransactionHooks::InstallHooks() {
  if (IS_PATCHED(CTransaction)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return;
  }

  CComObjectStackEx<FakeProtocol> prot;
  CComObjectStackEx<FakeFactory> factory;
  factory.obj_ = &prot;
  ScopedComPtr<IInternetSession> session;
  HRESULT hr = ::CoInternetGetSession(0, session.Receive(), 0);
  hr = session->RegisterNameSpace(&factory, CLSID_NULL, L"611", 0, 0, 0);
  DLOG_IF(FATAL, FAILED(hr)) << "Failed to register namespace";
  if (hr != S_OK)
    return;

  do {
    ScopedComPtr<IMoniker> mk;
    ScopedComPtr<IBindCtx> bc;
    ScopedComPtr<IStream> stream;
    hr = ::CreateAsyncBindCtxEx(0, 0, 0, 0, bc.Receive(), 0);
    DLOG_IF(FATAL, FAILED(hr)) << "CreateAsyncBindCtxEx failed " << hr;
    if (hr != S_OK)
      break;

    hr = ::CreateURLMoniker(NULL, L"611://512", mk.Receive());
    DLOG_IF(FATAL, FAILED(hr)) << "CreateURLMoniker failed " << hr;
    if (hr != S_OK)
      break;

    hr = mk->BindToStorage(bc, NULL, IID_IStream,
                           reinterpret_cast<void**>(stream.Receive()));
    DLOG_IF(FATAL, hr != INET_E_INVALID_CERTIFICATE) <<
        "BindToStorage failed " << hr;
  } while (0);

  hr = session->UnregisterNameSpace(&factory, L"611");
  if (prot.transaction_) {
    HookTransactionVtable(prot.transaction_);
    // Explicit release, otherwise ~CComObjectStackEx will complain about
    // outstanding reference to us, because it runs before ~FakeProtocol
    prot.transaction_.Release();
  }
}

void TransactionHooks::RevertHooks() {
  vtable_patch::UnpatchInterfaceMethods(CTransaction_PatchInfo);
  vtable_patch::UnpatchInterfaceMethods(CTransaction2_PatchInfo);
}
