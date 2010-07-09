// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <htiframe.h>
#include <mshtml.h>

#include "chrome_frame/protocol_sink_wrap.h"

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_bstr_win.h"
#include "base/singleton.h"
#include "base/string_util.h"

#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/function_stub.h"
#include "chrome_frame/utils.h"

// BINDSTATUS_SERVER_MIMETYPEAVAILABLE == 54. Introduced in IE 8, so
// not in everyone's headers yet. See:
// http://msdn.microsoft.com/en-us/library/ms775133(VS.85,loband).aspx
#ifndef BINDSTATUS_SERVER_MIMETYPEAVAILABLE
#define BINDSTATUS_SERVER_MIMETYPEAVAILABLE 54
#endif

static const char kTextHtmlMimeType[] = "text/html";
const wchar_t kUrlMonDllName[] = L"urlmon.dll";

static const int kInternetProtocolStartIndex = 3;
static const int kInternetProtocolReadIndex = 9;
static const int kInternetProtocolStartExIndex = 13;


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

/////////////////////////////////////////////////////////////////////////////
BEGIN_VTABLE_PATCHES(CTransaction)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, Hook_Start)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, Hook_Read)
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
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" 0x%08X", this);
}

ProtocolSinkWrap::~ProtocolSinkWrap() {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" 0x%08X", this);
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
  DLOG(INFO) << "ProtocolSinkWrap::ReportProgress: "
      <<  BindStatus2Str(status_code)
      << " Status: " << (status_text ? status_text : L"");

  HRESULT hr = prot_data_->ReportProgress(delegate_, status_code, status_text);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportData(DWORD flags, ULONG progress,
    ULONG max_progress) {
  DCHECK(delegate_);
  DLOG(INFO) << "ProtocolSinkWrap::ReportData: " << Bscf2Str(flags) <<
      " progress: " << progress << " progress_max: " << max_progress;

  HRESULT hr = prot_data_->ReportData(delegate_, flags, progress, max_progress);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportResult(HRESULT result, DWORD error,
    LPCWSTR result_text) {
  DLOG(INFO) << "ProtocolSinkWrap::ReportResult: result: " << result <<
      " error: " << error << " Text: " << (result_text ? result_text : L"");
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
    IBindCtx* pbc = reinterpret_cast<IBindCtx*>(StringToInt(bind_ctx_string));
    bind_ctx.Attach(pbc);
    CoTaskMemFree(bind_ctx_string);
  }

  return bind_ctx;
}

bool ShouldWrapSink(IInternetProtocolSink* sink, const wchar_t* url) {
  // TODO(stoyan): check the url scheme for http/https.
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

void PutProtData(IBindCtx* pbc, ProtData* data) {
  ScopedComPtr<BindContextInfo> info;
  BindContextInfo::FromBindContext(pbc, info.Receive());
  if (info)
    info->set_prot_data(data);
}

bool IsTextHtml(const wchar_t* status_text) {
  if (!status_text)
    return false;
  size_t status_text_length = lstrlenW(status_text);
  const wchar_t* status_text_end = status_text +
      std::min(status_text_length, arraysize(kTextHtmlMimeType) - 1);
  bool is_text_html = LowerCaseEqualsASCII(status_text, status_text_end,
                                           kTextHtmlMimeType);
  return is_text_html;
}

RendererType DetermineRendererType(void* buffer, DWORD size, bool last_chance) {
  RendererType type = UNDETERMINED;
  if (last_chance)
    type = OTHER;

  std::wstring html_contents;
  // TODO(joshia): detect and handle different content encodings
  UTF8ToWide(reinterpret_cast<char*>(buffer), size, &html_contents);

  // Note that document_contents_ may have NULL characters in it. While
  // browsers may handle this properly, we don't and will stop scanning
  // for the XUACompat content value if we encounter one.
  std::wstring xua_compat_content;
  UtilGetXUACompatContentValue(html_contents, &xua_compat_content);
  if (StrStrI(xua_compat_content.c_str(), kChromeContentPrefix)) {
    type = CHROME;
  }

  return type;
}

// ProtData
ProtData::ProtData(IInternetProtocol* protocol,
                   InternetProtocol_Read_Fn read_fun, const wchar_t* url)
    : has_suggested_mime_type_(false), has_server_mime_type_(false),
      report_data_received_(false), buffer_size_(0), buffer_pos_(0),
      renderer_type_(UNDETERMINED), protocol_(protocol), read_fun_(read_fun),
      url_(url) {
  memset(buffer_, 0, arraysize(buffer_));
  DLOG(INFO) << __FUNCTION__ << " " << this;

  // Add to map.
  AutoLock lock(datamap_lock_);
  DCHECK(datamap_.end() == datamap_.find(protocol_));
  datamap_[protocol] = this;
}

ProtData::~ProtData() {
  DLOG(INFO) << __FUNCTION__ << " " << this;

  // Remove from map.
  AutoLock lock(datamap_lock_);
  DCHECK(datamap_.end() != datamap_.find(protocol_));
  datamap_.erase(protocol_);
}

HRESULT ProtData::Read(void* buffer, ULONG size, ULONG* size_read) {
  if (renderer_type_ == UNDETERMINED) {
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


HRESULT ProtData::ReportProgress(IInternetProtocolSink* delegate,
                                 ULONG status_code, LPCWSTR status_text) {
  switch (status_code) {
    case BINDSTATUS_DIRECTBIND:
      renderer_type_ = OTHER;
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
      return S_OK;
  }

  return delegate->ReportProgress(status_code, status_text);
}

HRESULT ProtData::ReportData(IInternetProtocolSink* delegate,
                              DWORD flags, ULONG progress, ULONG max_progress) {
  if (renderer_type_ != UNDETERMINED) {
    return delegate->ReportData(flags, progress, max_progress);
  }

  // Do these checks only once.
  if (!report_data_received_) {
    report_data_received_ = true;

    DLOG_IF(INFO, (flags & BSCF_FIRSTDATANOTIFICATION) == 0) <<
      "BUGBUG: BSCF_FIRSTDATANOTIFICATION is not set properly!";


    // We check here, instead in ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE)
    // to be safe when following multiple redirects.?
    if (!IsTextHtml(suggested_mime_type_)) {
      renderer_type_ = OTHER;
      FireSugestedMimeType(delegate);
      return delegate->ReportData(flags, progress, max_progress);
    }

    if (!url_.empty() && IsOptInUrl(url_.c_str())) {
      // TODO(stoyan): We may attempt to remove ourselves from the bind context.
      renderer_type_ = CHROME;
      delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);
      return delegate->ReportData(flags, progress, max_progress);
    }
  }

  HRESULT hr = FillBuffer();

  bool last_chance = false;
  if (hr == S_OK || hr == S_FALSE) {
    last_chance = true;
  }

  renderer_type_ = DetermineRendererType(buffer_, buffer_size_, last_chance);

  if (renderer_type_ == UNDETERMINED) {
    // do not report anything, we need more data.
    return S_OK;
  }

  if (renderer_type_ == CHROME) {
    DLOG(INFO) << "Forwarding BINDSTATUS_MIMETYPEAVAILABLE "
        << kChromeMimeType;
    delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);
  }

  if (renderer_type_ == OTHER) {
    FireSugestedMimeType(delegate);
  }

  // This is the first data notification we forward.
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
  if (renderer_type_ == UNDETERMINED) {
    DLOG(INFO) << "ReportResult received but renderer type is yet unknown.";
    renderer_type_ = OTHER;
    FireSugestedMimeType(delegate);
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

void ProtData::FireSugestedMimeType(IInternetProtocolSink* delegate) {
  if (has_server_mime_type_) {
    DLOG(INFO) << "Forwarding BINDSTATUS_SERVER_MIMETYPEAVAILABLE "
        << suggested_mime_type_;
    delegate->ReportProgress(BINDSTATUS_SERVER_MIMETYPEAVAILABLE,
                             suggested_mime_type_);
  }

  if (has_suggested_mime_type_) {
    DLOG(INFO) << "Forwarding BINDSTATUS_MIMETYPEAVAILABLE "
        << suggested_mime_type_;
    delegate->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE,
                             suggested_mime_type_);
  }
}

scoped_refptr<ProtData> ProtData::DataFromProtocol(
    IInternetProtocol* protocol) {
  scoped_refptr<ProtData> instance;
  AutoLock lock(datamap_lock_);
  ProtocolDataMap::iterator it = datamap_.find(protocol);
  if (datamap_.end() != it)
    instance = it->second;
  return instance;
}

// IInternetProtocol/Ex hooks.
STDMETHODIMP Hook_Start(InternetProtocol_Start_Fn orig_start,
    IInternetProtocol* protocol, LPCWSTR url, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start);
  if (!url || !prot_sink || !bind_info)
    return E_INVALIDARG;
  DLOG_IF(INFO, url != NULL) << "OnStart: " << url << PiFlags2Str(flags);

  ScopedComPtr<IBindCtx> bind_ctx = BindCtxFromIBindInfo(bind_info);
  if (!bind_ctx) {
    // MSHTML sometimes takes a short path, skips the creation of
    // moniker and binding, by directly grabbing protocol from InternetSession
    DLOG(INFO) << "DirectBind for " << url;
    return orig_start(protocol, url, prot_sink, bind_info, flags, reserved);
  }

  if (IsCFRequest(bind_ctx)) {
    return orig_start(protocol, url, prot_sink, bind_info, flags, reserved);
  }

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data) {
    DLOG(INFO) << "Found existing ProtData!";
    prot_data->UpdateUrl(url);
    ScopedComPtr<IInternetProtocolSink> new_sink =
        ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
    return orig_start(protocol, url, new_sink, bind_info, flags, reserved);
  }

  if (!ShouldWrapSink(prot_sink, url)) {
    return orig_start(protocol, url, prot_sink, bind_info, flags, reserved);
  }

  // Fresh request.
  InternetProtocol_Read_Fn read_fun = reinterpret_cast<InternetProtocol_Read_Fn>
      (CTransaction_PatchInfo[1].stub_->argument());
  prot_data = new ProtData(protocol, read_fun, url);
  PutProtData(bind_ctx, prot_data);

  ScopedComPtr<IInternetProtocolSink> new_sink =
      ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
  return orig_start(protocol, url, new_sink, bind_info, flags, reserved);
}

STDMETHODIMP Hook_StartEx(InternetProtocol_StartEx_Fn orig_start_ex,
    IInternetProtocolEx* protocol, IUri* uri, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start_ex);
  if (!uri || !prot_sink || !bind_info)
    return E_INVALIDARG;

  ScopedBstr url;
  uri->GetPropertyBSTR(Uri_PROPERTY_ABSOLUTE_URI, url.Receive(), 0);
  DLOG_IF(INFO, url != NULL) << "OnStartEx: " << url << PiFlags2Str(flags);

  ScopedComPtr<IBindCtx> bind_ctx = BindCtxFromIBindInfo(bind_info);
  if (!bind_ctx) {
    // MSHTML sometimes takes a short path, skips the creation of
    // moniker and binding, by directly grabbing protocol from InternetSession.
    DLOG(INFO) << "DirectBind for " << url;
    return orig_start_ex(protocol, uri, prot_sink, bind_info, flags, reserved);
  }

  if (IsCFRequest(bind_ctx)) {
    return orig_start_ex(protocol, uri, prot_sink, bind_info, flags, reserved);
  }

  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (prot_data) {
    DLOG(INFO) << "Found existing ProtData!";
    prot_data->UpdateUrl(url);
    ScopedComPtr<IInternetProtocolSink> new_sink =
        ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
    return orig_start_ex(protocol, uri, new_sink, bind_info, flags, reserved);
  }

  if (!ShouldWrapSink(prot_sink, url)) {
    return orig_start_ex(protocol, uri, prot_sink, bind_info, flags, reserved);
  }

  // Fresh request.
  InternetProtocol_Read_Fn read_fun = reinterpret_cast<InternetProtocol_Read_Fn>
    (CTransaction_PatchInfo[1].stub_->argument());
  prot_data = new ProtData(protocol, read_fun, url);
  PutProtData(bind_ctx, prot_data);

  ScopedComPtr<IInternetProtocolSink> new_sink =
      ProtocolSinkWrap::CreateNewSink(prot_sink, prot_data);
  return orig_start_ex(protocol, uri, new_sink, bind_info, flags, reserved);
}

STDMETHODIMP Hook_Read(InternetProtocol_Read_Fn orig_read,
    IInternetProtocol* protocol, void* buffer, ULONG size, ULONG* size_read) {
  DCHECK(orig_read);
  scoped_refptr<ProtData> prot_data = ProtData::DataFromProtocol(protocol);
  if (!prot_data) {
    return orig_read(protocol, buffer, size, size_read);
  }

  HRESULT hr = prot_data->Read(buffer, size, size_read);
  return hr;
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
