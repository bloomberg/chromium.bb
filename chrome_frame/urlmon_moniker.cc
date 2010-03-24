// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_moniker.h"

#include <shlguid.h>

#include "base/string_util.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/urlmon_bind_status_callback.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "net/http/http_util.h"

static const int kMonikerBindToObject = 8;
static const int kMonikerBindToStorage = kMonikerBindToObject + 1;

base::LazyInstance<base::ThreadLocalPointer<NavigationManager> >
    NavigationManager::thread_singleton_(base::LINKER_INITIALIZED);

BEGIN_VTABLE_PATCHES(IMoniker)
  VTABLE_PATCH_ENTRY(kMonikerBindToObject, MonikerPatch::BindToObject)
  VTABLE_PATCH_ENTRY(kMonikerBindToStorage, MonikerPatch::BindToStorage)
END_VTABLE_PATCHES()

namespace {
std::string FindReferrerFromHeaders(const wchar_t* headers,
                                     const wchar_t* additional_headers) {
  std::string referrer;

  const wchar_t* both_headers[] = { headers, additional_headers };
  for (int i = 0; referrer.empty() && i < arraysize(both_headers); ++i) {
    if (!both_headers[i])
      continue;
    std::string raw_headers_utf8 = WideToUTF8(both_headers[i]);
    net::HttpUtil::HeadersIterator it(raw_headers_utf8.begin(),
                                      raw_headers_utf8.end(), "\r\n");
    while (it.GetNext()) {
      if (LowerCaseEqualsASCII(it.name(), "referer")) {
        referrer = it.values();
        break;
      }
    }
  }

  return referrer;
}

}  // end namespace

void ReadStreamCache::RewindCache() const {
  DCHECK(cache_);
  RewindStream(cache_);
}

HRESULT ReadStreamCache::WriteToCache(const void* data, ULONG size,
                                      ULONG* written) {
  HRESULT hr = S_OK;
  if (!cache_) {
    hr = ::CreateStreamOnHGlobal(NULL, TRUE, cache_.Receive());
    DCHECK(cache_);
  }

  if (SUCCEEDED(hr))
    hr = cache_->Write(data, size, written);

  return hr;
}

size_t ReadStreamCache::GetCacheSize() const {
  size_t ret = 0;
  if (cache_) {
    STATSTG stg = {0};
    cache_->Stat(&stg, STATFLAG_NONAME);
    DCHECK_EQ(stg.cbSize.HighPart, 0);
    ret = stg.cbSize.LowPart;
  }

  return ret;
}

HRESULT ReadStreamCache::Read(void* pv, ULONG cb, ULONG* read) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      ::GetCurrentThreadId());
  DCHECK(delegate_);
  DWORD read_bytes = 0;
  HRESULT hr = delegate_->Read(pv, cb, &read_bytes);
  if (read)
    *read = read_bytes;

  // Note that Read() might return E_PENDING so we just check if anything
  // was read rather than check hr.
  if (read_bytes) {
    DWORD written = 0;
    WriteToCache(pv, read_bytes, &written);
    DCHECK(read_bytes == written);
  }

  return hr;
}

/////////////////////////////

HRESULT SimpleBindingImpl::GetBindResult(CLSID* protocol, DWORD* result_code,
                                         LPOLESTR* result, DWORD* reserved) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      ::GetCurrentThreadId());

  HRESULT hr = S_OK;

  if (protocol)
    *protocol = GUID_NULL;

  if (result)
    *result = NULL;

  if (delegate_)
    hr = delegate_->GetBindResult(protocol, result_code, result, reserved);

  if (SUCCEEDED(hr) && bind_results_ != S_OK)
    *result_code = bind_results_;

  return hr;
}

HRESULT SimpleBindingImpl::DelegateQI(void* obj, REFIID iid, void** ret,
                                      DWORD cookie) {
  SimpleBindingImpl* me = reinterpret_cast<SimpleBindingImpl*>(obj);
  HRESULT hr = E_NOINTERFACE;
  if (me->delegate_)
    hr = me->delegate_.QueryInterface(iid, ret);
  DLOG(INFO) << __FUNCTION__ << " " << GuidToString(iid)
      << StringPrintf(" 0x%08X", hr);
  return hr;
}

///////////////////////////////

void RequestHeaders::OnBeginningTransaction(const wchar_t* url,
                                            const wchar_t* headers,
                                            const wchar_t* additional_headers) {
  if (url)
    request_url_ = url;

  if (headers)
    begin_request_headers_ = headers;

  if (additional_headers)
    additional_request_headers_ = additional_headers;

  DLOG(INFO) << __FUNCTION__ << "\n " << request_url_ << "\n "
      << begin_request_headers_ << "\n "
      << additional_request_headers_;

  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (mgr) {
    DCHECK(mgr->IsTopLevelUrl(request_url_.c_str()));
    mgr->SetActiveRequestHeaders(this);
  }
}

void RequestHeaders::OnResponse(DWORD response_code,
                                const wchar_t* response_headers,
                                const wchar_t* request_headers) {
  response_code_ = response_code;

  if (response_headers)
    response_headers_ = response_headers;

  if (request_headers)
    request_headers_ = request_headers;

#ifndef NDEBUG
  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (mgr) {
    // For some reason we might not have gotten an OnBeginningTransaction call!
    // DCHECK(mgr->IsTopLevelUrl(request_url_.c_str()));
    // DCHECK(mgr->GetActiveRequestHeaders() == this);
    mgr->SetActiveRequestHeaders(this);
  }
#endif
}

std::string RequestHeaders::GetReferrer() {
  std::string referrer = FindReferrerFromHeaders(begin_request_headers_.c_str(),
                                 additional_request_headers_.c_str());
  if (referrer.empty()) {
    DLOG(INFO) << request_url_ << "\nbheaders: " << begin_request_headers_
        << "\nadd:" << additional_request_headers_ << "\nreq: " << request_headers_;
  }
  return referrer;
}

HRESULT RequestHeaders::FireHttpNegotiateEvents(IHttpNegotiate* http) const {
  DCHECK(http);

  LPOLESTR additional_headers = NULL;
  HRESULT hr = http->BeginningTransaction(request_url_.c_str(),
      begin_request_headers_.c_str(), 0, &additional_headers);
  DCHECK(SUCCEEDED(hr));
  ::CoTaskMemFree(additional_headers);

  // Also notify the navigation manager on this thread if one exists.
  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (mgr) {
    mgr->OnBeginningTransaction(true, request_url_.c_str(),
        begin_request_headers_.c_str(), additional_request_headers_.c_str());
  }

  additional_headers = NULL;
  hr = http->OnResponse(response_code_, response_headers_.c_str(),
      request_headers_.c_str(), &additional_headers);
  DCHECK(SUCCEEDED(hr));
  if (additional_headers) {
    ::CoTaskMemFree(additional_headers);
    additional_headers = NULL;
  }

  return hr;
}

///////////////////////////////

RequestData::RequestData() {
  ZeroMemory(&format_, sizeof(format_));
  format_.dwAspect = 1;
  format_.lindex = -1;
  format_.tymed = TYMED_ISTREAM;
  CComObject<ReadStreamCache>* stream = NULL;
  CComObject<ReadStreamCache>::CreateInstance(&stream);
  DCHECK(stream);
  stream_delegate_ = stream;
  DCHECK_EQ(stream->m_dwRef, 1);
}

RequestData::~RequestData() {
}

void RequestData::Initialize(RequestHeaders* headers) {
  if (headers) {
    headers_ = headers;
  } else {
    headers_ = new RequestHeaders();
  }
}

HRESULT RequestData::DelegateDataRead(IBindStatusCallback* callback,
                                      DWORD flags, DWORD size,
                                      FORMATETC* format, STGMEDIUM* storage,
                                      size_t* bytes_read) {
  DCHECK(callback);
  DCHECK(storage);
  if ((flags & BSCF_FIRSTDATANOTIFICATION) && format) {
    format_ = *format;
  }

  IStream* original = storage->pstm;
  DCHECK(original);
  if (!original)
    return E_POINTER;

  size_t size_before = stream_delegate_->GetCacheSize();
  stream_delegate_->SetDelegate(storage->pstm);
  storage->pstm = stream_delegate_;
  HRESULT hr = callback->OnDataAvailable(flags, size, format, storage);
  storage->pstm = original;
  if (bytes_read)
    *bytes_read = stream_delegate_->GetCacheSize() - size_before;
  return hr;
}

void RequestData::CacheAll(IStream* data) {
  DCHECK(data);
  char buffer[4096];
  HRESULT hr = S_OK;
  while (hr == S_OK) {
    DWORD read = 0;
    hr = data->Read(buffer, arraysize(buffer), &read);
    if (read) {
      DWORD written = 0;
      stream_delegate_->WriteToCache(buffer, read, &written);
      DCHECK(read == written);
    } else {
      // Just in case Read() completed with S_OK but zero bytes.
      break;
    }
  }
}

HRESULT RequestData::GetResetCachedContentStream(IStream** clone) {
  DCHECK(clone);
  IStream* cache = stream_delegate_->cache();
  if (!cache)
    return HRESULT_FROM_WIN32(ERROR_EMPTY);
  HRESULT hr = cache->Clone(clone);
  if (SUCCEEDED(hr)) {
    RewindStream(*clone);
  } else {
    NOTREACHED();
  }
  return hr;
}

////////////////////////////

HRESULT NavigationManager::NavigateToCurrentUrlInCF(IBrowserService* browser) {
  DCHECK(browser);
  DLOG(INFO) << __FUNCTION__ << " " << url();

  MarkBrowserOnThreadForCFNavigation(browser);

  ScopedComPtr<IBindCtx> bind_context;
  ScopedComPtr<IMoniker> moniker;
  HRESULT hr = ::CreateBindCtx(0, bind_context.Receive());
  DCHECK(bind_context);
  if (SUCCEEDED(hr) &&
      SUCCEEDED(hr = ::CreateURLMonikerEx(NULL, url_.c_str(), moniker.Receive(),
                                          URL_MK_UNIFORM))) {
    if (SUCCEEDED(hr)) {
      // If there's a referrer, preserve it.
      std::wstring headers;
      if (!referrer_.empty()) {
        headers = StringPrintf(L"Referer: %ls\r\n\r\n",
            ASCIIToWide(referrer_).c_str());
      }

      // Pass in URL fragments if applicable.
      std::wstring fragment;
      GURL parsed_moniker_url(url_);
      if (parsed_moniker_url.has_ref()) {
        fragment = UTF8ToWide(parsed_moniker_url.ref());
        set_original_url_with_fragment(url_.c_str());
      }

      hr = NavigateBrowserToMoniker(browser, moniker, headers.c_str(),
          bind_context, fragment.c_str());
      DLOG(INFO) << StringPrintf("NavigateBrowserToMoniker: 0x%08X", hr);
    }
  }

  return hr;
}

void NavigationManager::SetActiveRequestData(RequestData* request_data) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" 0x%08X", request_data);
  DCHECK(request_data_ == NULL || request_data == NULL);
  DLOG_IF(ERROR, url_.empty() && request_data)
      << "attempt to cache data after url has been cleared";
  // Clear the current request headers as the request data must have the
  // correct set of headers.
  DCHECK(request_data == NULL || request_data->headers() != NULL);
  request_headers_ = NULL;

  if (request_data == NULL || !url_.empty()) {
    request_data_ = request_data;
  }
}

void NavigationManager::OnBeginningTransaction(bool is_top_level,
    const wchar_t* url, const wchar_t* headers,
    const wchar_t* additional_headers) {
  DCHECK(url != NULL);

  // We've seen this happen on the first page load in IE8 (it might also happen
  // in other versions of IE) and for some reason we did not get a call to
  // BeginningTransaction the first time it happened and this time around we're
  // using a cached data object that didn't get the request headers or URL.
  DLOG_IF(ERROR, lstrlenW(url) == 0) << __FUNCTION__ << "Invalid URL.";

  if (!is_top_level)
    return;

  if (url_.compare(url) != 0) {
    DLOG(INFO) << __FUNCTION__ << " not processing headers for url: " << url
        << " current url is: " << url_;
    return;
  }

  // Save away the referrer in case our active document needs it to initiate
  // navigation in chrome.
  referrer_ = FindReferrerFromHeaders(headers, additional_headers);
}

/////////////////////////////////////////

NavigationManager* NavigationManager::GetThreadInstance() {
  return thread_singleton_.Pointer()->Get();
}

void NavigationManager::RegisterThreadInstance() {
  DCHECK(GetThreadInstance() == NULL);
  thread_singleton_.Pointer()->Set(this);
}

void NavigationManager::UnregisterThreadInstance() {
  DCHECK(GetThreadInstance() == this);
  thread_singleton_.Pointer()->Set(NULL);
}

/////////////////////////////////////////

// static
bool MonikerPatch::Initialize() {
  if (IS_PATCHED(IMoniker)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return true;
  }

  ScopedComPtr<IMoniker> moniker;
  HRESULT hr = ::CreateURLMoniker(NULL, L"http://localhost/",
                                  moniker.Receive());
  DCHECK(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    hr = vtable_patch::PatchInterfaceMethods(moniker, IMoniker_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr)) << StringPrintf("patch failed 0x%08X", hr);
  }

  return SUCCEEDED(hr);
}

// static
void MonikerPatch::Uninitialize() {
  vtable_patch::UnpatchInterfaceMethods(IMoniker_PatchInfo);
}

// static
HRESULT MonikerPatch::BindToObject(IMoniker_BindToObject_Fn original,
                                   IMoniker* me, IBindCtx* bind_ctx,
                                   IMoniker* to_left, REFIID iid, void** obj) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(to_left == NULL);
  CComHeapPtr<WCHAR> url;
  HRESULT hr = me->GetDisplayName(bind_ctx, NULL, &url);
  DCHECK(SUCCEEDED(hr));
  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (mgr) {
    bool interest = mgr ? mgr->IsTopLevelUrl(url) : false;
    if (interest) {
      scoped_refptr<RequestData> request_data(mgr->GetActiveRequestData(url));
      if (request_data) {
        DLOG(INFO) << " got cached content";
        mgr->set_referrer(request_data->headers()->GetReferrer());
        DLOG(INFO) << "referrer is: " << mgr->referrer();
        // Create a new CF document object and initialize it with
        // the already cached data.
        ScopedComPtr<IUnknown> cf_doc;
        hr = cf_doc.CreateInstance(CLSID_ChromeActiveDocument);
        DCHECK(SUCCEEDED(hr));
        ScopedComPtr<IPersistMoniker> persist_moniker;
        hr = persist_moniker.QueryFrom(cf_doc);
        DCHECK(SUCCEEDED(hr));
        hr = persist_moniker->Load(TRUE, me, bind_ctx, STGM_READ);
        if (SUCCEEDED(hr)) {
          hr = persist_moniker.QueryInterface(iid, obj);
          DCHECK(SUCCEEDED(hr));
        }
      } else {
        DLOG(INFO) << " creating callback object";
        CComObject<CFUrlmonBindStatusCallback>* callback = NULL;
        hr = CComObject<CFUrlmonBindStatusCallback>::CreateInstance(
            &callback);
        callback->AddRef();
        hr = callback->Initialize(bind_ctx, mgr->GetActiveRequestHeaders());
        DCHECK(SUCCEEDED(hr));
        hr = original(me, bind_ctx, to_left, iid, obj);
        callback->Release();
        if (SUCCEEDED(hr) && (*obj) == NULL) {
          DCHECK(hr == MK_S_ASYNCHRONOUS);
        }
      }
    } else {
      DLOG(INFO) << " -- calling original. (no interest)";
      hr = original(me, bind_ctx, to_left, iid, obj);
    }
  } else {
    // We don't have a NavigationManager instance for cases like View Source
    // etc.
    hr = original(me, bind_ctx, to_left, iid, obj);
  }

  return hr;
}

// static
HRESULT MonikerPatch::BindToStorage(IMoniker_BindToStorage_Fn original,
                                    IMoniker* me, IBindCtx* bind_ctx,
                                    IMoniker* to_left, REFIID iid, void** obj) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(iid == IID_IStream);
  DCHECK(to_left == NULL);

  HRESULT hr = E_UNEXPECTED;
  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (mgr) {
    CComHeapPtr<WCHAR> url;
    hr = me->GetDisplayName(bind_ctx, NULL, &url);
    DCHECK(SUCCEEDED(hr));
    bool interest = mgr->IsTopLevelUrl(url);
    DLOG(INFO) << "interest: " << interest << " url " << url;
    if (interest) {
      scoped_refptr<RequestData> request_data(mgr->GetActiveRequestData(url));
      if (request_data) {
        CComObject<SimpleBindingImpl>* binding = NULL;
        CComObject<SimpleBindingImpl>::CreateInstance(&binding);
        binding->OverrideBindResults(INET_E_TERMINATED_BIND);
        binding->AddRef();
        hr = BindToStorageFromCache(bind_ctx, kChromeMimeType, request_data,
                                    binding, reinterpret_cast<IStream**>(obj));
        binding->Release();
      } else {
        DLOG(INFO) << " creating callback object";
        CComObject<CFUrlmonBindStatusCallback>* callback = NULL;
        hr = CComObject<CFUrlmonBindStatusCallback>::CreateInstance(
            &callback);
        callback->AddRef();
        hr = callback->Initialize(bind_ctx, mgr->GetActiveRequestHeaders());
        DCHECK(SUCCEEDED(hr));
        hr = original(me, bind_ctx, to_left, iid, obj);
        callback->Release();
        if (SUCCEEDED(hr) && (*obj) == NULL) {
          DCHECK(hr == MK_S_ASYNCHRONOUS);
        }
      }
    } else {
      hr = original(me, bind_ctx, to_left, iid, obj);
    }
  } else {
    // We usually only get here when we're issuing requests from our
    // worker thread.  However there are some exceptions such as when
    // windows media player is issuing requests, so we don't DCHECK.
    hr = original(me, bind_ctx, to_left, iid, obj);
  }

  return hr;
}

HRESULT MonikerPatch::BindToStorageFromCache(IBindCtx* bind_ctx,
                                             const wchar_t* mime_type,
                                             RequestData* data,
                                             SimpleBindingImpl* binding,
                                             IStream** cache_out) {
  DCHECK(bind_ctx);
  // mime_type may be NULL
  DCHECK(data);
  DCHECK(binding);
  // cache_out may be NULL

  ScopedComPtr<IUnknown> holder;
  HRESULT hr = bind_ctx->GetObjectParam(L"_BSCB_Holder_", holder.Receive());
  DCHECK(holder);
  ScopedComPtr<IBindStatusCallback> bscb;
  ScopedComPtr<IHttpNegotiate> http_negotiate;
  DoQueryService(IID_IBindStatusCallback, holder, bscb.Receive());
  DoQueryService(IID_IHttpNegotiate, bscb, http_negotiate.Receive());
  if (!bscb) {
    NOTREACHED();
    return E_NOINTERFACE;
  }

  hr = bscb->OnStartBinding(0, binding);
  DCHECK(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    // Fire BeginningTransaction and OnResponse events.
    if (http_negotiate)
      data->headers()->FireHttpNegotiateEvents(http_negotiate);

    FORMATETC format_etc = data->format();
    if (mime_type) {
      // If a non NULL mime type was specified, override the clipboard format.
      format_etc.cfFormat = ::RegisterClipboardFormatW(mime_type);
      DCHECK(format_etc.cfFormat);
    }

    const wchar_t* mime_type_available = mime_type;
    wchar_t format_name[MAX_PATH] = {0};
    if (!mime_type_available) {
      ::GetClipboardFormatName(format_etc.cfFormat, format_name,
                               arraysize(format_name));
      mime_type_available = format_name;
    }
    DCHECK(mime_type_available);
    hr = bscb->OnProgress(0, 0, BINDSTATUS_MIMETYPEAVAILABLE,
                          mime_type_available);

    ScopedComPtr<IStream> cache;
    data->GetResetCachedContentStream(cache.Receive());

    STGMEDIUM medium = {0};
    medium.tymed = TYMED_ISTREAM;
    medium.pstm = cache;
    DLOG(INFO) << __FUNCTION__ << " total bytes available: "
        << data->GetCachedContentSize();
    hr = bscb->OnDataAvailable(
        BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION |
        BSCF_DATAFULLYAVAILABLE, data->GetCachedContentSize(), &format_etc,
        &medium);

    // OnDataAvailable might report a failure such as INET_E_TERMINATED_BIND
    // when mshtml decides not to handle the request.
    // We ignore the return value from OnStopBinding and return whatever
    // OnDataAvailable gave us.
    if (FAILED(hr)) {
      binding->OverrideBindResults(hr);
    } else if (cache_out) {
      *cache_out = cache.Detach();
    }

    bscb->OnStopBinding(hr, NULL);
  }

  return hr;
}

