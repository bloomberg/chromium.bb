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

#include "chrome_frame/utils.h"

// BINDSTATUS_SERVER_MIMETYPEAVAILABLE == 54. Introduced in IE 8, so
// not in everyone's headers yet. See:
// http://msdn.microsoft.com/en-us/library/ms775133(VS.85,loband).aspx
#ifndef BINDSTATUS_SERVER_MIMETYPEAVAILABLE
#define BINDSTATUS_SERVER_MIMETYPEAVAILABLE 54
#endif

static const wchar_t* kChromeMimeType = L"application/chromepage";
static const char kTextHtmlMimeType[] = "text/html";
const wchar_t kUrlMonDllName[] = L"urlmon.dll";

static const int kInternetProtocolStartIndex = 3;
static const int kInternetProtocolReadIndex = 9;
static const int kInternetProtocolStartExIndex = 13;

// TODO(ananta)
// We should avoid duplicate VTable declarations.
BEGIN_VTABLE_PATCHES(IInternetProtocol)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, ProtocolSinkWrap::OnStart)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, ProtocolSinkWrap::OnRead)
END_VTABLE_PATCHES()

BEGIN_VTABLE_PATCHES(IInternetProtocolSecure)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, ProtocolSinkWrap::OnStart)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, ProtocolSinkWrap::OnRead)
END_VTABLE_PATCHES()

BEGIN_VTABLE_PATCHES(IInternetProtocolEx)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, ProtocolSinkWrap::OnStart)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, ProtocolSinkWrap::OnRead)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartExIndex, ProtocolSinkWrap::OnStartEx)
END_VTABLE_PATCHES()

BEGIN_VTABLE_PATCHES(IInternetProtocolExSecure)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartIndex, ProtocolSinkWrap::OnStart)
  VTABLE_PATCH_ENTRY(kInternetProtocolReadIndex, ProtocolSinkWrap::OnRead)
  VTABLE_PATCH_ENTRY(kInternetProtocolStartExIndex, ProtocolSinkWrap::OnStartEx)
END_VTABLE_PATCHES()

//
// ProtocolSinkWrap implementation
//

// Static map initialization
ProtocolSinkWrap::ProtocolSinkMap ProtocolSinkWrap::sink_map_;
CComAutoCriticalSection ProtocolSinkWrap::sink_map_lock_;

ProtocolSinkWrap::ProtocolSinkWrap()
    : protocol_(NULL), renderer_type_(UNDETERMINED),
      buffer_size_(0), buffer_pos_(0), is_saved_result_(false),
      result_code_(0), result_error_(0), report_data_recursiveness_(0),
      determining_renderer_type_(false) {
  memset(buffer_, 0, arraysize(buffer_));
}

ProtocolSinkWrap::~ProtocolSinkWrap() {
  // This object may be destroyed before Initialize is called.
  if (protocol_ != NULL) {
    CComCritSecLock<CComAutoCriticalSection> lock(sink_map_lock_);
    DCHECK(sink_map_.end() != sink_map_.find(protocol_));
    sink_map_.erase(protocol_);
    protocol_ = NULL;
  }
  DLOG(INFO) << "ProtocolSinkWrap: active sinks: " << sink_map_.size();
}

bool ProtocolSinkWrap::PatchProtocolHandlers() {
  HRESULT hr = PatchProtocolMethods(CLSID_HttpProtocol,
                                    IInternetProtocol_PatchInfo,
                                    IInternetProtocolEx_PatchInfo);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to patch IInternetProtocol interface."
        << " Error: " << hr;
    return false;
  }

  hr = PatchProtocolMethods(CLSID_HttpSProtocol,
                            IInternetProtocolSecure_PatchInfo,
                            IInternetProtocolExSecure_PatchInfo);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to patch IInternetProtocol secure interface."
        << " Error: " << hr;
    return false;
  }

  return true;
}

void ProtocolSinkWrap::UnpatchProtocolHandlers() {
  vtable_patch::UnpatchInterfaceMethods(IInternetProtocol_PatchInfo);
  vtable_patch::UnpatchInterfaceMethods(IInternetProtocolEx_PatchInfo);
  vtable_patch::UnpatchInterfaceMethods(IInternetProtocolSecure_PatchInfo);
  vtable_patch::UnpatchInterfaceMethods(IInternetProtocolExSecure_PatchInfo);
}

HRESULT ProtocolSinkWrap::CreateProtocolHandlerInstance(
    const CLSID& clsid, IInternetProtocol** protocol) {
  if (!protocol) {
    return E_INVALIDARG;
  }

  HMODULE module = ::GetModuleHandle(kUrlMonDllName);
  if (!module) {
    NOTREACHED() << "urlmon is not yet loaded. Error: " << GetLastError();
    return E_FAIL;
  }

  typedef HRESULT (WINAPI* DllGetClassObject_Fn)(REFCLSID, REFIID, LPVOID*);
  DllGetClassObject_Fn fn = reinterpret_cast<DllGetClassObject_Fn>(
      ::GetProcAddress(module, "DllGetClassObject"));
  if (!fn) {
    NOTREACHED() << "DllGetClassObject not found in urlmon.dll";
    return E_FAIL;
  }

  ScopedComPtr<IClassFactory> protocol_class_factory;
  HRESULT hr = fn(clsid, IID_IClassFactory,
      reinterpret_cast<LPVOID*>(protocol_class_factory.Receive()));
  if (FAILED(hr)) {
    NOTREACHED() << "DllGetclassObject failed. Error: " << hr;
    return hr;
  }

  ScopedComPtr<IInternetProtocol> handler_instance;
  hr = protocol_class_factory->CreateInstance(NULL, IID_IInternetProtocol,
      reinterpret_cast<void**>(handler_instance.Receive()));
  if (FAILED(hr)) {
    NOTREACHED() << "ClassFactory::CreateInstance failed for InternetProtocol."
        << " Error: " << hr;
  } else {
    *protocol = handler_instance.Detach();
  }

  return hr;
}

HRESULT ProtocolSinkWrap::PatchProtocolMethods(
    const CLSID& clsid_protocol,
    vtable_patch::MethodPatchInfo* protocol_patch_info,
    vtable_patch::MethodPatchInfo* protocol_ex_patch_info) {
  if (!protocol_patch_info || !protocol_ex_patch_info) {
    return E_INVALIDARG;
  }

  ScopedComPtr<IInternetProtocol> http_protocol;
  HRESULT hr = CreateProtocolHandlerInstance(clsid_protocol,
                                             http_protocol.Receive());
  if (FAILED(hr)) {
    NOTREACHED() << "ClassFactory::CreateInstance failed for InternetProtocol."
        << " Error: " << hr;
    return false;
  }

  ScopedComPtr<IInternetProtocolEx> ipex;
  ipex.QueryFrom(http_protocol);
  if (ipex) {
    hr = vtable_patch::PatchInterfaceMethods(ipex, protocol_ex_patch_info);
  } else {
    hr = vtable_patch::PatchInterfaceMethods(http_protocol,
                                             protocol_patch_info);
  }
  return hr;
}

// IInternetProtocol/Ex method implementation.
HRESULT ProtocolSinkWrap::OnStart(InternetProtocol_Start_Fn orig_start,
    IInternetProtocol* protocol, LPCWSTR url, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start);
  DLOG_IF(INFO, url != NULL) << "OnStart: " << url;

  ScopedComPtr<IInternetProtocolSink> sink_to_use(MaybeWrapSink(protocol,
      prot_sink, url));
  return orig_start(protocol, url, sink_to_use, bind_info, flags, reserved);
}

HRESULT ProtocolSinkWrap::OnStartEx(InternetProtocol_StartEx_Fn orig_start_ex,
    IInternetProtocolEx* protocol, IUri* uri, IInternetProtocolSink* prot_sink,
    IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved) {
  DCHECK(orig_start_ex);

  ScopedBstr url;
  uri->GetPropertyBSTR(Uri_PROPERTY_ABSOLUTE_URI, url.Receive(), 0);
  DLOG_IF(INFO, url != NULL) << "OnStartEx: " << url;

  ScopedComPtr<IInternetProtocolSink> sink_to_use(MaybeWrapSink(protocol,
      prot_sink, url));
  return orig_start_ex(protocol, uri, sink_to_use, bind_info, flags, reserved);
}

HRESULT ProtocolSinkWrap::OnRead(InternetProtocol_Read_Fn orig_read,
    IInternetProtocol* protocol, void* buffer, ULONG size, ULONG* size_read) {
  DCHECK(orig_read);

  scoped_refptr<ProtocolSinkWrap> instance =
      ProtocolSinkWrap::InstanceFromProtocol(protocol);
  HRESULT hr;
  if (instance) {
    DCHECK(instance->protocol_ == protocol);
    hr = instance->OnReadImpl(buffer, size, size_read, orig_read);
  } else {
    hr = orig_read(protocol, buffer, size, size_read);
  }

  return hr;
}

bool ProtocolSinkWrap::Initialize(IInternetProtocol* protocol,
    IInternetProtocolSink* original_sink, const wchar_t* url) {
  DCHECK(original_sink);
  delegate_ = original_sink;
  protocol_ = protocol;
  if (url)
    url_ = url;

  CComCritSecLock<CComAutoCriticalSection> lock(sink_map_lock_);
  DCHECK(sink_map_.end() == sink_map_.find(protocol));
  sink_map_[protocol] = this;
  DLOG(INFO) << "ProtocolSinkWrap: active sinks: " << sink_map_.size();
  return true;
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
  DLOG(INFO) << "ProtocolSinkWrap::ReportProgress: Code:" << status_code <<
      " Text: " << (status_text ? status_text : L"");
  if (!delegate_) {
    return E_FAIL;
  }
  if ((BINDSTATUS_MIMETYPEAVAILABLE == status_code) ||
      (BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE == status_code)) {
    // If we have a MIMETYPE and that MIMETYPE is not "text/html". we don't
    // want to do anything with this.
    if (status_text) {
      size_t status_text_length = lstrlenW(status_text);
      const wchar_t* status_text_end = status_text + std::min(
          status_text_length, arraysize(kTextHtmlMimeType) - 1);
      if (!LowerCaseEqualsASCII(status_text, status_text_end,
                                kTextHtmlMimeType)) {
        renderer_type_ = OTHER;
      }
    }
  }

  HRESULT hr = S_OK;
  if (delegate_ && renderer_type_ != CHROME) {
    hr = delegate_->ReportProgress(status_code, status_text);
  }
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportData(DWORD flags, ULONG progress,
    ULONG max_progress) {
  DCHECK(protocol_);
  DCHECK(delegate_);
  DLOG(INFO) << "ProtocolSinkWrap::ReportData: flags: " << flags <<
    " progress: " << progress << " progress_max: " << max_progress;

  scoped_refptr<ProtocolSinkWrap> self_ref(this);

  // Maintain a stack depth to make a determination. ReportData is called
  // recursively in IE8. If the request can be served in a single Read, the
  // situation ends up like this:
  // orig_prot
  // |--> ProtocolSinkWrap::ReportData (BSCF_FIRSTDATANOTIFICATION)
  //       |--> orig_prot->Read(...) - 1st read - S_OK and data
  //            |--> ProtocolSinkWrap::ReportData (BSCF_LASTDATANOTIFICATION)
  //                 |--> orig_prot->Read(...) - 2nd read S_FALSE, 0 bytes
  //
  // Inner call returns S_FALSE and no data. We try to make a determination
  // of render type then and incorrectly set it to 'OTHER' as we don't have
  // any data yet. However, we can make a determination in the context of
  // outer ReportData since the first read will return S_OK with data. Then
  // the next Read in the loop will return S_FALSE and we will enter the
  // determination logic.

  // NOTE: We use the report_data_recursiveness_ variable to detect situations
  //    in which calls to ReportData are re-entrant (such as when the entire
  //    contents of a page fit inside a single packet). In these cases, we
  //    don't care about re-entrant calls beyond the second, and so we compare
  //    report_data_recursiveness_ inside the while loop, making sure we skip
  //    what would otherwise be spurious calls to ReportProgress().
  report_data_recursiveness_++;

  HRESULT hr = S_OK;
  if (is_undetermined()) {
    CheckAndReportChromeMimeTypeForRequest();
  }

  // we call original only if the renderer type is other
  if (renderer_type() == OTHER) {
    hr = delegate_->ReportData(flags, progress, max_progress);

    if (is_saved_result_) {
      is_saved_result_ = false;
      delegate_->ReportResult(result_code_, result_error_,
          result_text_.c_str());
    }
  }

  report_data_recursiveness_--;
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::ReportResult(HRESULT result, DWORD error,
    LPCWSTR result_text) {
  DLOG(INFO) << "ProtocolSinkWrap::ReportResult: result: " << result <<
    " error: " << error << " Text: " << (result_text ? result_text : L"");

  // If this request failed, we don't want to have anything to do with this.
  if (FAILED(result))
    renderer_type_ = OTHER;

  // if we are still not sure about the renderer type, cache the result,
  // othewise urlmon will get confused about getting reported about a
  // success result for which it never received any data.
  if (is_undetermined()) {
    is_saved_result_ = true;
    result_code_ = result;
    result_error_ = error;
    if (result_text)
      result_text_ = result_text;
    return S_OK;
  }

  HRESULT hr = E_FAIL;
  if (delegate_)
    hr = delegate_->ReportResult(result, error, result_text);

  return hr;
}

// IInternetBindInfoEx
STDMETHODIMP ProtocolSinkWrap::GetBindInfo(DWORD* flags,
    BINDINFO* bind_info_ret) {
  ScopedComPtr<IInternetBindInfo> bind_info;
  HRESULT hr = bind_info.QueryFrom(delegate_);
  if (bind_info)
    hr = bind_info->GetBindInfo(flags, bind_info_ret);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::GetBindString(ULONG string_type,
    LPOLESTR* string_array, ULONG array_size, ULONG* size_returned) {
  ScopedComPtr<IInternetBindInfo> bind_info;
  HRESULT hr = bind_info.QueryFrom(delegate_);
  if (bind_info)
    hr = bind_info->GetBindString(string_type, string_array,
        array_size, size_returned);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::GetBindInfoEx(DWORD* flags, BINDINFO* bind_info,
    DWORD* bindf2, DWORD* reserved) {
  ScopedComPtr<IInternetBindInfoEx> bind_info_ex;
  HRESULT hr = bind_info_ex.QueryFrom(delegate_);
  if (bind_info_ex)
    hr = bind_info_ex->GetBindInfoEx(flags, bind_info, bindf2, reserved);
  return hr;
}

// IServiceProvider
STDMETHODIMP ProtocolSinkWrap::QueryService(REFGUID service_guid,
    REFIID riid, void** service) {
  ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = service_provider.QueryFrom(delegate_);
  if (service_provider)
    hr = service_provider->QueryService(service_guid, riid, service);
  return hr;
}

// IAuthenticate
STDMETHODIMP ProtocolSinkWrap::Authenticate(HWND* window,
    LPWSTR* user_name, LPWSTR* password) {
  ScopedComPtr<IAuthenticate> authenticate;
  HRESULT hr = authenticate.QueryFrom(delegate_);
  if (authenticate)
    hr = authenticate->Authenticate(window, user_name, password);
  return hr;
}

// IInternetProtocolEx
STDMETHODIMP ProtocolSinkWrap::Start(LPCWSTR url,
    IInternetProtocolSink *protocol_sink, IInternetBindInfo* bind_info,
    DWORD flags, HANDLE_PTR reserved) {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Start(url, protocol_sink, bind_info, flags, reserved);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Continue(PROTOCOLDATA* protocol_data) {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Continue(protocol_data);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Abort(HRESULT reason, DWORD options) {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Abort(reason, options);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Terminate(DWORD options) {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Terminate(options);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Suspend() {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Suspend();
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Resume() {
  ScopedComPtr<IInternetProtocolRoot> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Resume();
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Read(void *buffer, ULONG size,
    ULONG* size_read) {
  ScopedComPtr<IInternetProtocol> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Read(buffer, size, size_read);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::Seek(LARGE_INTEGER move, DWORD origin,
    ULARGE_INTEGER* new_pos) {
  ScopedComPtr<IInternetProtocol> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->Seek(move, origin, new_pos);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::LockRequest(DWORD options) {
  ScopedComPtr<IInternetProtocol> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->LockRequest(options);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::UnlockRequest() {
  ScopedComPtr<IInternetProtocol> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->UnlockRequest();
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::StartEx(IUri* uri,
    IInternetProtocolSink* protocol_sink, IInternetBindInfo* bind_info,
    DWORD flags, HANDLE_PTR reserved) {
  ScopedComPtr<IInternetProtocolEx> protocol;
  HRESULT hr = protocol.QueryFrom(delegate_);
  if (protocol)
    hr = protocol->StartEx(uri, protocol_sink, bind_info, flags, reserved);
  return hr;
}

// IInternetPriority
STDMETHODIMP ProtocolSinkWrap::SetPriority(LONG priority) {
  ScopedComPtr<IInternetPriority> internet_priority;
  HRESULT hr = internet_priority.QueryFrom(delegate_);
  if (internet_priority)
    hr = internet_priority->SetPriority(priority);
  return hr;
}

STDMETHODIMP ProtocolSinkWrap::GetPriority(LONG* priority) {
  ScopedComPtr<IInternetPriority> internet_priority;
  HRESULT hr = internet_priority.QueryFrom(delegate_);
  if (internet_priority)
    hr = internet_priority->GetPriority(priority);
  return hr;
}

// IWrappedProtocol
STDMETHODIMP ProtocolSinkWrap::GetWrapperCode(LONG *code, DWORD_PTR reserved) {
  ScopedComPtr<IWrappedProtocol> wrapped_protocol;
  HRESULT hr = wrapped_protocol.QueryFrom(delegate_);
  if (wrapped_protocol)
    hr = wrapped_protocol->GetWrapperCode(code, reserved);
  return hr;
}


// public IUriContainer
STDMETHODIMP ProtocolSinkWrap::GetIUri(IUri** uri) {
  ScopedComPtr<IUriContainer> uri_container;
  HRESULT hr = uri_container.QueryFrom(delegate_);
  if (uri_container)
    hr = uri_container->GetIUri(uri);
  return hr;
}

// Protected helpers

void ProtocolSinkWrap::DetermineRendererType() {
  if (is_undetermined()) {
    if (IsOptInUrl(url_.c_str())) {
      renderer_type_ = CHROME;
    } else {
      std::wstring xua_compat_content;
      // Note that document_contents_ may have NULL characters in it. While
      // browsers may handle this properly, we don't and will stop scanning for
      // the XUACompat content value if we encounter one.
      DCHECK(buffer_size_ < arraysize(buffer_));
      buffer_[buffer_size_] = 0;
      std::wstring html_contents;
      // TODO(joshia): detect and handle different content encodings
      UTF8ToWide(buffer_, buffer_size_, &html_contents);
      UtilGetXUACompatContentValue(html_contents, &xua_compat_content);
      if (StrStrI(xua_compat_content.c_str(), kChromeContentPrefix)) {
        renderer_type_ = CHROME;
      } else {
        renderer_type_ = OTHER;
      }
    }
  }
}

HRESULT ProtocolSinkWrap::CheckAndReportChromeMimeTypeForRequest() {
  if (!is_undetermined())
    return S_OK;

  // This function could get invoked recursively in the context of
  // IInternetProtocol::Read. Check for the same and bail.
  if (determining_renderer_type_)
    return S_OK;

  determining_renderer_type_ = true;

  HRESULT hr_read = S_OK;
  while (hr_read == S_OK) {
    ULONG size_read = 0;
    hr_read = protocol_->Read(buffer_ + buffer_size_,
        kMaxContentSniffLength - buffer_size_, &size_read);
    buffer_size_ += size_read;

    // Attempt to determine the renderer type if we have received
    // sufficient data. Do not attempt this when we are called recursively.
    if (report_data_recursiveness_ < 2 && (S_FALSE == hr_read) ||
       (buffer_size_ >= kMaxContentSniffLength)) {
      DetermineRendererType();
      if (renderer_type() == CHROME) {
        // Workaround for IE 8 and "nosniff". See:
        // http://blogs.msdn.com/ie/archive/2008/09/02/ie8-security-part-vi-beta-2-update.aspx
        delegate_->ReportProgress(
            BINDSTATUS_SERVER_MIMETYPEAVAILABLE, kChromeMimeType);
        // For IE < 8.
        delegate_->ReportProgress(
            BINDSTATUS_MIMETYPEAVAILABLE, kChromeMimeType);

        delegate_->ReportProgress(
            BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE, kChromeMimeType);

        delegate_->ReportData(
            BSCF_FIRSTDATANOTIFICATION, 0, 0);

        delegate_->ReportData(
            BSCF_LASTDATANOTIFICATION | BSCF_DATAFULLYAVAILABLE, 0, 0);
      }
      break;
    }
  }

  determining_renderer_type_ = false;
  return hr_read;
}

HRESULT ProtocolSinkWrap::OnReadImpl(void* buffer, ULONG size, ULONG* size_read,
    InternetProtocol_Read_Fn orig_read) {
  // We want to switch the renderer to chrome, we cannot return any
  // data now.
  if (CHROME == renderer_type())
    return S_FALSE;

  // Serve data from our buffer first.
  if (OTHER == renderer_type()) {
    const ULONG bytes_to_copy = std::min(buffer_size_ - buffer_pos_, size);
    if (bytes_to_copy) {
      memcpy(buffer, buffer_ + buffer_pos_, bytes_to_copy);
      *size_read = bytes_to_copy;
      buffer_pos_ += bytes_to_copy;
      return S_OK;
    }
  }

  return orig_read(protocol_, buffer, size, size_read);
}

scoped_refptr<ProtocolSinkWrap> ProtocolSinkWrap::InstanceFromProtocol(
    IInternetProtocol* protocol) {
  CComCritSecLock<CComAutoCriticalSection> lock(sink_map_lock_);
  scoped_refptr<ProtocolSinkWrap> instance;
  ProtocolSinkMap::iterator it = sink_map_.find(protocol);
  if (sink_map_.end() != it)
    instance = it->second;
  return instance;
}

ScopedComPtr<IInternetProtocolSink> ProtocolSinkWrap::MaybeWrapSink(
    IInternetProtocol* protocol, IInternetProtocolSink* prot_sink,
    const wchar_t* url) {
  ScopedComPtr<IInternetProtocolSink> sink_to_use(prot_sink);

  // FYI: GUID_NULL doesn't work when the URL is being loaded from history.
  // asking for IID_IHttpNegotiate as the service id works, but
  // getting the IWebBrowser2 interface still doesn't work.
  ScopedComPtr<IHttpNegotiate> http_negotiate;
  HRESULT hr = DoQueryService(GUID_NULL, prot_sink, http_negotiate.Receive());

  if (http_negotiate && !IsSubFrameRequest(http_negotiate)) {
    CComObject<ProtocolSinkWrap>* wrap = NULL;
    CComObject<ProtocolSinkWrap>::CreateInstance(&wrap);
    DCHECK(wrap);
    if (wrap) {
      wrap->AddRef();
      if (wrap->Initialize(protocol, prot_sink, url)) {
        sink_to_use = wrap;
      }
      wrap->Release();
    }
  }

  return sink_to_use;
}
