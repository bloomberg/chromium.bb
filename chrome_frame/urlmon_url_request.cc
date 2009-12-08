// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_url_request.h"

#include <wininet.h>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "chrome_frame/utils.h"
#include "net/http/http_util.h"
#include "net/http/http_response_headers.h"

static const LARGE_INTEGER kZero = {0};
static const ULARGE_INTEGER kUnsignedZero = {0};
int UrlmonUrlRequest::instance_count_ = 0;

UrlmonUrlRequest::UrlmonUrlRequest()
    : pending_read_size_(0),
      status_(URLRequestStatus::FAILED, net::ERR_FAILED),
      thread_(PlatformThread::CurrentId()),
      redirect_status_(0),
      parent_window_(NULL),
      worker_thread_(NULL),
      ignore_redirect_stop_binding_error_(false) {
  DLOG(INFO) << StringPrintf("Created request. Obj: %X", this)
      << " Count: " << ++instance_count_;
}

UrlmonUrlRequest::~UrlmonUrlRequest() {
  DLOG(INFO) << StringPrintf("Deleted request. Obj: %X", this)
      << " Count: " << --instance_count_;
}

bool UrlmonUrlRequest::Start() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  if (!worker_thread_) {
    NOTREACHED() << __FUNCTION__ << " Urlmon request thread not initialized";
    return false;
  }

  Create(HWND_MESSAGE);
  if (!IsWindow()) {
    NOTREACHED() << "Failed to create urlmon message window: "
                 << GetLastError();
    return false;
  }

  // Take a self reference to maintain COM lifetime. This will be released
  // in OnFinalMessage
  AddRef();
  request_handler()->AddRequest(this);

  worker_thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &UrlmonUrlRequest::StartAsync));

  return true;
}

void UrlmonUrlRequest::Stop() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  if (!worker_thread_) {
    NOTREACHED() << __FUNCTION__ << " Urlmon request thread not initialized";
    return;
  }

  // We can remove the request from the map safely here if it is still valid.
  // There is an additional reference on the UrlmonUrlRequest instance which
  // is released when the task scheduled by the EndRequest function executes.
  request_handler()->RemoveRequest(this);

  worker_thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &UrlmonUrlRequest::StopAsync));
}

void UrlmonUrlRequest::StartAsync() {
  DCHECK(worker_thread_ != NULL);

  status_.set_status(URLRequestStatus::IO_PENDING);
  HRESULT hr = StartAsyncDownload();
  if (FAILED(hr)) {
    // Do not call EndRequest() here since it will attempt to free references
    // that have not been established.
    status_.set_os_error(HresultToNetError(hr));
    status_.set_status(URLRequestStatus::FAILED);
    DLOG(ERROR) << "StartAsyncDownload failed";
    EndRequest();
    return;
  }
}

void UrlmonUrlRequest::StopAsync() {
  DCHECK(worker_thread_ != NULL);

  if (binding_) {
    binding_->Abort();
  } else {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_os_error(net::ERR_FAILED);
    EndRequest();
  }
}

void UrlmonUrlRequest::OnFinalMessage(HWND window) {
  m_hWnd = NULL;
  // Release the outstanding reference in the context of the UI thread to
  // ensure that our instance gets deleted in the same thread which created it.
  Release();
}

bool UrlmonUrlRequest::Read(int bytes_to_read) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this);

  if (!worker_thread_) {
    NOTREACHED() << __FUNCTION__ << " Urlmon request thread not initialized";
    return false;
  }

  worker_thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &UrlmonUrlRequest::ReadAsync,
                                   bytes_to_read));
  return true;
}

void UrlmonUrlRequest::TransferToHost(IUnknown* host) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  DCHECK(host);

  DCHECK(moniker_);
  if (!moniker_)
    return;

  ScopedComPtr<IMoniker> moniker;
  moniker.Attach(moniker_.Detach());

  // Create a new bind context that's not associated with our callback.
  // Calling RevokeBindStatusCallback doesn't disassociate the callback with
  // the bind context in IE7.  The returned bind context has the same
  // implementation of GetRunningObjectTable as the bind context we held which
  // basically delegates to ole32's GetRunningObjectTable.  The object table
  // is then used to determine if the moniker is already running and via
  // that mechanism is associated with the same internet request as has already
  // been issued.
  ScopedComPtr<IBindCtx> bind_context;
  CreateBindCtx(0, bind_context.Receive());
  DCHECK(bind_context);

  LPOLESTR url = NULL;
  if (SUCCEEDED(moniker->GetDisplayName(bind_context, NULL, &url))) {
    DLOG(INFO) << __FUNCTION__ << " " << url;

    // TODO(tommi): See if we can get HlinkSimpleNavigateToMoniker to work
    // instead.  Looks like we'll need to support IHTMLDocument2 (get_URL in
    // particular), access to IWebBrowser2 etc.
    // HlinkSimpleNavigateToMoniker(moniker, url, NULL, host, bind_context,
    //                              NULL, 0, 0);

    ScopedComPtr<IWebBrowser2> wb2;
    HRESULT hr = DoQueryService(SID_SWebBrowserApp, host, wb2.Receive());
    DCHECK(wb2);
    DLOG_IF(WARNING, FAILED(hr)) << StringPrintf(L"SWebBrowserApp 0x%08X", hr);

    ScopedComPtr<IWebBrowserPriv> wbp;
    ScopedComPtr<IWebBrowserPriv2IE7> wbp2_ie7;
    ScopedComPtr<IWebBrowserPriv2IE8> wbp2_ie8;
    if (SUCCEEDED(hr = wbp.QueryFrom(wb2))) {
      ScopedVariant var_url(url);
      hr = wbp->NavigateWithBindCtx(var_url.AsInput(), NULL, NULL, NULL, NULL,
                                    bind_context, NULL);
      DLOG_IF(WARNING, FAILED(hr))
          << StringPrintf(L"NavigateWithBindCtx 0x%08X", hr);
    } else {
      DLOG(WARNING) << StringPrintf(L"IWebBrowserPriv 0x%08X", hr);
      IWebBrowserPriv2IE7* common_wbp2 = NULL;
      if (SUCCEEDED(hr = wbp2_ie7.QueryFrom(wb2))) {
        common_wbp2 = wbp2_ie7;
      } else if (SUCCEEDED(hr = wbp2_ie8.QueryFrom(wb2))) {
        common_wbp2 = reinterpret_cast<IWebBrowserPriv2IE7*>(wbp2_ie8.get());
      }

      if (common_wbp2) {
        typedef HRESULT (WINAPI* CreateUriFn)(LPCWSTR uri, DWORD flags,
            DWORD_PTR reserved, IUri** ret);

        CreateUriFn create_uri = reinterpret_cast<CreateUriFn>(
            ::GetProcAddress(::GetModuleHandleA("urlmon"), "CreateUri"));
        DCHECK(create_uri);
        if (create_uri) {
          ScopedComPtr<IUri> uri_obj;
          hr = create_uri(url, 0, 0, uri_obj.Receive());
          DLOG_IF(WARNING, FAILED(hr))
              << StringPrintf(L"create_uri 0x%08X", hr);
          hr = common_wbp2->NavigateWithBindCtx2(uri_obj, NULL, NULL, NULL,
                                                 NULL, bind_context, NULL);
          DLOG_IF(WARNING, FAILED(hr))
              << StringPrintf(L"NavigateWithBindCtx2 0x%08X", hr);
        }
      } else {
        DLOG(WARNING) << StringPrintf(L"IWebBrowserPriv2 0x%08X", hr);
        NOTREACHED();
      }
    }

    DCHECK(wbp || wbp2_ie7 || wbp2_ie8);

    ::CoTaskMemFree(url);
  }
}

void UrlmonUrlRequest::ReadAsync(int bytes_to_read) {
  // Send cached data if available.
  CComObjectStackEx<SendStream> send_stream;
  send_stream.Initialize(this);

  size_t bytes_copied = 0;
  if (cached_data_.is_valid() && cached_data_.Read(&send_stream, bytes_to_read,
                                                   &bytes_copied)) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X - bytes read from cache: %d",
                               url().c_str(), this, bytes_copied);
    return;
  }

  // if the request is finished or there's nothing more to read
  // then end the request
  if (!status_.is_io_pending() || !binding_) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X. Response finished. Status: %d",
                               url().c_str(), this, status_.status());
    EndRequest();
    return;
  }

  pending_read_size_ = bytes_to_read;
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      "- Read pending for: " << bytes_to_read;
}

STDMETHODIMP UrlmonUrlRequest::OnStartBinding(
    DWORD reserved, IBinding *binding) {
  binding_ = binding;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetPriority(LONG *priority) {
  if (!priority)
    return E_POINTER;
  *priority = THREAD_PRIORITY_NORMAL;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnLowResource(DWORD reserved) {
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnProgress(ULONG progress, ULONG max_progress,
    ULONG status_code, LPCWSTR status_text) {
  static const int kDefaultHttpRedirectCode = 302;

  switch (status_code) {
    case BINDSTATUS_REDIRECTING: {
      // Fetch the redirect status as they aren't all equal (307 in particular
      // retains the HTTP request verb).
      // We assume that valid redirect codes are 301, 302, 303 and 307. If we
      // receive anything else we would abort the request which would
      // eventually result in the request getting cancelled in Chrome.
      int redirect_status = GetHttpResponseStatus();
      DCHECK(status_text != NULL);
      DLOG(INFO) << "URL: " << url() << " redirected to "
                 << status_text;
      redirect_url_ = status_text;
      redirect_status_ =
          redirect_status > 0 ? redirect_status : kDefaultHttpRedirectCode;
      // Chrome should decide whether a redirect has to be followed. To achieve
      // this we send over a fake response to Chrome and abort the redirect.
      std::string headers = GetHttpHeaders();
      OnResponse(0, UTF8ToWide(headers).c_str(), NULL, NULL);
      ignore_redirect_stop_binding_error_ = true;
      DCHECK(binding_ != NULL);
      binding_->Abort();
      binding_ = NULL;
      return E_ABORT;
    }

    default:
      DLOG(INFO) << " Obj: " << std::hex << this << " OnProgress(" << url()
          << StringPrintf(L") code: %i status: %ls", status_code, status_text);
      break;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnStopBinding(HRESULT result, LPCWSTR error) {
  DCHECK(worker_thread_ != NULL);
  DCHECK_EQ(PlatformThread::CurrentId(), worker_thread_->thread_id());

  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " - Request stopped, Result: " << std::hex << result <<
      " Status: " << status_.status();

  if (FAILED(result)) {
    status_.set_status(URLRequestStatus::FAILED);
    status_.set_os_error(HresultToNetError(result));
    EndRequest();
  } else {
    status_.set_status(URLRequestStatus::SUCCESS);
    status_.set_os_error(0);
    ReleaseBindings();
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetBindInfo(DWORD* bind_flags,
    BINDINFO *bind_info) {
  DCHECK(worker_thread_ != NULL);
  DCHECK_EQ(PlatformThread::CurrentId(), worker_thread_->thread_id());

  if ((bind_info == NULL) || (bind_info->cbSize == 0) || (bind_flags == NULL))
    return E_INVALIDARG;

  *bind_flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;
  if (LowerCaseEqualsASCII(method(), "get")) {
    bind_info->dwBindVerb = BINDVERB_GET;
  } else if (LowerCaseEqualsASCII(method(), "post")) {
    bind_info->dwBindVerb = BINDVERB_POST;

    // Bypass caching proxies on POSTs and avoid writing responses to POST
    // requests to the browser's cache.
    *bind_flags |= BINDF_GETNEWESTVERSION | BINDF_NOWRITECACHE |
                   BINDF_PRAGMA_NO_CACHE;

    // Initialize the STGMEDIUM.
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;
    bind_info->szCustomVerb = NULL;

    if (get_upload_data(&bind_info->stgmedData.pstm) == S_OK) {
      bind_info->stgmedData.tymed = TYMED_ISTREAM;
      DLOG(INFO) << " Obj: " << std::hex << this << " POST request with "
          << Int64ToString(post_data_len()) << " bytes";
    } else {
      DLOG(INFO) << " Obj: " << std::hex << this
          << "POST request with no data!";
    }
  } else {
    NOTREACHED() << "Unknown HTTP method.";
    status_.set_status(URLRequestStatus::FAILED);
    status_.set_os_error(net::ERR_INVALID_URL);
    EndRequest();
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnDataAvailable(DWORD flags, DWORD size,
                                               FORMATETC* formatetc,
                                               STGMEDIUM* storage) {
  DCHECK(worker_thread_ != NULL);
  DCHECK_EQ(PlatformThread::CurrentId(), worker_thread_->thread_id());

  DLOG(INFO) << StringPrintf("URL: %s Obj: %X - Bytes available: %d",
                             url().c_str(), this, size);

  if (!storage || (storage->tymed != TYMED_ISTREAM)) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  IStream* read_stream = storage->pstm;
  if (!read_stream) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  HRESULT hr = S_OK;
  if (BSCF_FIRSTDATANOTIFICATION & flags) {
    DCHECK(!cached_data_.is_valid());
  }

  // Always read data into cache. We have to read all the data here at this
  // time or it won't be available later. Since the size of the data could
  // be more than pending read size, it's not straightforward (or might even
  // be impossible) to implement a true data pull model.
  size_t bytes_available = 0;
  cached_data_.Append(read_stream, &bytes_available);
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " -  Bytes read into cache: " << bytes_available;

  if (pending_read_size_ && cached_data_.is_valid()) {
    CComObjectStackEx<SendStream> send_stream;
    send_stream.Initialize(this);
    cached_data_.Read(&send_stream, pending_read_size_, &pending_read_size_);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - size read: " << pending_read_size_;
  } else {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - waiting for remote read";
  }

  if (BSCF_LASTDATANOTIFICATION & flags) {
    status_.set_status(URLRequestStatus::SUCCESS);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - end of data.";
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnObjectAvailable(REFIID iid, IUnknown *object) {
  // We are calling BindToStorage on the moniker we should always get called
  // back on OnDataAvailable and should never get OnObjectAvailable
  NOTREACHED();
  return E_NOTIMPL;
}

STDMETHODIMP UrlmonUrlRequest::BeginningTransaction(const wchar_t* url,
    const wchar_t* current_headers, DWORD reserved,
    wchar_t** additional_headers) {
  DCHECK(worker_thread_ != NULL);
  DCHECK_EQ(PlatformThread::CurrentId(), worker_thread_->thread_id());

  if (!additional_headers) {
    NOTREACHED();
    return E_POINTER;
  }

  DLOG(INFO) << "URL: " << url << " Obj: " << std::hex << this <<
      " - Request headers: \n" << current_headers;

  HRESULT hr = S_OK;

  std::string new_headers;
  if (post_data_len() > 0) {
    // Tack on the Content-Length header since when using an IStream type
    // STGMEDIUM, it looks like it doesn't get set for us :(
    new_headers = StringPrintf("Content-Length: %s\r\n",
                               Int64ToString(post_data_len()).c_str());
  }

  if (!extra_headers().empty()) {
    // TODO(robertshield): We may need to sanitize headers on POST here.
    new_headers += extra_headers();
  }

  if (!referrer().empty()) {
    // Referrer is famously misspelled in HTTP:
    new_headers += StringPrintf("Referer: %s\r\n", referrer().c_str());
  }

  if (!new_headers.empty()) {
    *additional_headers = reinterpret_cast<wchar_t*>(
        CoTaskMemAlloc((new_headers.size() + 1) * sizeof(wchar_t)));

    if (*additional_headers == NULL) {
      NOTREACHED();
      hr = E_OUTOFMEMORY;
    } else {
      lstrcpynW(*additional_headers, ASCIIToWide(new_headers).c_str(),
                new_headers.size());
    }
  }

  return hr;
}

STDMETHODIMP UrlmonUrlRequest::OnResponse(DWORD dwResponseCode,
    const wchar_t* response_headers, const wchar_t* request_headers,
    wchar_t** additional_headers) {
  DCHECK(worker_thread_ != NULL);
  DCHECK_EQ(PlatformThread::CurrentId(), worker_thread_->thread_id());

  std::string raw_headers = WideToUTF8(response_headers);

  // Security check for frame busting headers. We don't honor the headers
  // as-such, but instead simply kill requests which we've been asked to
  // look for if they specify a value for "X-Frame-Options" other than
  // "ALLOWALL" (the others are "deny" and "sameorigin"). This puts the onus
  // on the user of the UrlRequest to specify whether or not requests should
  // be inspected. For ActiveDocuments, the answer is "no", since WebKit's
  // detection/handling is sufficient and since ActiveDocuments cannot be
  // hosted as iframes. For NPAPI and ActiveX documents, the Initialize()
  // function of the PluginUrlRequest object allows them to specify how they'd
  // like requests handled. Both should set enable_frame_busting_ to true to
  // avoid CSRF attacks. Should WebKit's handling of this ever change, we will
  // need to re-visit how and when frames are killed to better mirror a policy
  // which may do something other than kill the sub-document outright.

  // NOTE(slightlyoff): We don't use net::HttpResponseHeaders here because
  //    of lingering ICU/base_noicu issues.
  if (frame_busting_enabled_) {
    std::string http_headers = net::HttpUtil::AssembleRawHeaders(
        raw_headers.c_str(), raw_headers.length());
    if (http_utils::HasFrameBustingHeader(http_headers)) {
      DLOG(ERROR) << "X-Frame-Options header other than ALLOWALL " <<
          "detected, navigation canceled";
      return E_FAIL;
    }
  }

  std::wstring url_for_persistent_cookies =
      redirect_url_.empty() ? UTF8ToWide(url()) : redirect_url_;

  std::string persistent_cookies;

  DWORD cookie_size = 0;  // NOLINT
  InternetGetCookie(url_for_persistent_cookies.c_str(), NULL, NULL,
                    &cookie_size);
  if (cookie_size) {
    scoped_array<wchar_t> cookies(new wchar_t[cookie_size + 1]);
    if (!InternetGetCookie(url_for_persistent_cookies.c_str(), NULL,
                           cookies.get(), &cookie_size)) {
      NOTREACHED() << "InternetGetCookie failed. Error: " << GetLastError();
    } else {
      persistent_cookies = WideToUTF8(cookies.get());
    }
  }

  OnResponseStarted("",
                    raw_headers.c_str(),
                    0,
                    base::Time(),
                    persistent_cookies,
                    redirect_url_.empty() ? std::string() :
                        WideToUTF8(redirect_url_),
                    redirect_status_);

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetWindow(const GUID& guid_reason,
                                         HWND* parent_window) {
  if (!parent_window) {
    return E_INVALIDARG;
  }

#ifndef NDEBUG
  wchar_t guid[40] = {0};
  ::StringFromGUID2(guid_reason, guid, arraysize(guid));

  DLOG(INFO) << " Obj: " << std::hex << this << " GetWindow: " <<
      (guid_reason == IID_IAuthenticate ? L" - IAuthenticate" :
       (guid_reason == IID_IHttpSecurity ? L"IHttpSecurity" :
        (guid_reason == IID_IWindowForBindingUI ? L"IWindowForBindingUI" :
                                                  guid)));
#endif

  // TODO(iyengar): This hits when running the URL request tests.
  DLOG_IF(ERROR, !::IsWindow(parent_window_))
      << "UrlmonUrlRequest::GetWindow - no window!";
  *parent_window = parent_window_;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::Authenticate(HWND* parent_window,
                                            LPWSTR* user_name,
                                            LPWSTR* password) {
  if (!parent_window) {
    return E_INVALIDARG;
  }

  DCHECK(::IsWindow(parent_window_));
  *parent_window = parent_window_;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnSecurityProblem(DWORD problem) {
  // Urlmon notifies the client of authentication problems, certificate
  // errors, etc by querying the object implementing the IBindStatusCallback
  // interface for the IHttpSecurity interface. If this interface is not
  // implemented then Urlmon checks for the problem codes defined below
  // and performs actions as defined below:-
  // It invokes the ReportProgress method of the protocol sink with
  // these problem codes and eventually invokes the ReportResult method
  // on the protocol sink which ends up in a call to the OnStopBinding
  // method of the IBindStatusCallBack interface.

  // MSHTML's implementation of the IBindStatusCallback interface does not
  // implement the IHttpSecurity interface. However it handles the
  // OnStopBinding call with a HRESULT of 0x800c0019 and navigates to
  // an interstitial page which presents the user with a choice of whether
  // to abort the navigation.

  // In our OnStopBinding implementation we stop the navigation and inform
  // Chrome about the result. Ideally Chrome should behave in a manner similar
  // to IE, i.e. display the SSL error interstitial page and if the user
  // decides to proceed anyway we would turn off SSL warnings for that
  // particular navigation and allow IE to download the content.
  // We would need to return the certificate information to Chrome for display
  // purposes. Currently we only return a dummy certificate to Chrome.
  // At this point we decided that it is a lot of work at this point and
  // decided to go with the easier option of implementing the IHttpSecurity
  // interface and replicating the checks performed by Urlmon. This
  // causes Urlmon to display a dialog box on the same lines as IE6.
  DLOG(INFO) << __FUNCTION__ << " Security problem : " << problem;

  // On IE6 the default IBindStatusCallback interface does not implement the
  // IHttpSecurity interface and thus causes IE to put up a certificate error
  // dialog box. We need to emulate this behavior for sites with mismatched
  // certificates to work.
  if (GetIEVersion() == IE_6)
    return S_FALSE;

  HRESULT hr = E_ABORT;

  switch (problem) {
    case ERROR_INTERNET_SEC_CERT_REV_FAILED: {
      hr = RPC_E_RETRY;
      break;
    }

    case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
    case ERROR_INTERNET_SEC_CERT_CN_INVALID:
    case ERROR_INTERNET_INVALID_CA: {
      hr = S_FALSE;
      break;
    }

    default: {
      NOTREACHED() << "Unhandled security problem : " << problem;
      break;
    }
  }
  return hr;
}

HRESULT UrlmonUrlRequest::ConnectToExistingMoniker(IMoniker* moniker,
                                                   IBindCtx* context,
                                                   const std::wstring& url) {
  if (!moniker || url.empty()) {
    NOTREACHED() << "Invalid arguments";
    return E_INVALIDARG;
  }

  DCHECK(moniker_.get() == NULL);
  DCHECK(bind_context_.get() == NULL);

  moniker_ = moniker;
  bind_context_ = context;
  set_url(WideToUTF8(url));
  return S_OK;
}

HRESULT UrlmonUrlRequest::StartAsyncDownload() {
  HRESULT hr = E_FAIL;
  if (moniker_.get() == NULL) {
    std::wstring wide_url = UTF8ToWide(url());
    hr = CreateURLMonikerEx(NULL, wide_url.c_str(), moniker_.Receive(),
                            URL_MK_UNIFORM);
    if (FAILED(hr)) {
      NOTREACHED() << "CreateURLMonikerEx failed. Error: " << hr;
    } else {
      hr = CreateAsyncBindCtx(0, this, NULL, bind_context_.Receive());
      DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtx failed. Error: " << hr;
    }
  } else {
    DCHECK(bind_context_.get() != NULL);
    hr = RegisterBindStatusCallback(bind_context_, this, NULL, 0);
  }

  if (SUCCEEDED(hr)) {
    ScopedComPtr<IStream> stream;
    hr = moniker_->BindToStorage(bind_context_, NULL, __uuidof(IStream),
                                 reinterpret_cast<void**>(stream.Receive()));
    if (FAILED(hr)) {
      // TODO(joshia): Look into. This currently fails for:
      // http://user2:secret@localhost:1337/auth-basic?set-cookie-if-challenged
      // when running the UrlRequest unit tests.
      DLOG(ERROR) <<
          StringPrintf("IUrlMoniker::BindToStorage failed. Error: 0x%08X.", hr)
          << std::endl << url();
      DCHECK(hr == MK_E_SYNTAX);
    }
  }

  DLOG_IF(ERROR, FAILED(hr))
      << StringPrintf(L"StartAsyncDownload failed: 0x%08X", hr);

  return hr;
}

void UrlmonUrlRequest::EndRequest() {
  DLOG(INFO) << __FUNCTION__;

  // In case of a redirect notification we prevent urlmon from following the
  // redirect and rely on Chrome, in which case AutomationMsg_RequestEnd
  // IPC will be sent over by Chrome to end this request.
  if (!ignore_redirect_stop_binding_error_) {
    // Special case.  If the last request was a redirect and the current OS
    // error value is E_ACCESSDENIED, that means an unsafe redirect was
    // attempted. In that case, correct the OS error value to be the more
    // specific ERR_UNSAFE_REDIRECT error value.
    if (!status_.is_success() && status_.os_error() == net::ERR_ACCESS_DENIED) {
      int status = GetHttpResponseStatus();
      if (status >= 300 && status < 400) {
        redirect_status_ = status;  // store the latest redirect status value.
        status_.set_os_error(net::ERR_UNSAFE_REDIRECT);
      }
    }
    OnResponseEnd(status_);
  } else {
    ignore_redirect_stop_binding_error_ = false;
  }

  ReleaseBindings();
  // Remove the request mapping and release the outstanding reference to us in
  // the context of the UI thread.
  // We should not access any members of the UrlmonUrlRequest object after this
  // as the object would be deleted.
  PostTask(FROM_HERE,
           NewRunnableMethod(this, &UrlmonUrlRequest::EndRequestInternal));
}

void UrlmonUrlRequest::EndRequestInternal() {
  // The request object could have been removed from the map in the
  // OnRequestEnd callback which executes on receiving the
  // AutomationMsg_RequestEnd IPC from Chrome.
  request_handler()->RemoveRequest(this);
  // The current instance could get destroyed in the context of DestroyWindow.
  // We should not access the object after this.
  DestroyWindow();
}

int UrlmonUrlRequest::GetHttpResponseStatus() const {
  if (binding_ == NULL) {
    DLOG(WARNING) << "GetHttpResponseStatus - no binding_";
    return 0;
  }

  int http_status = 0;

  ScopedComPtr<IWinInetHttpInfo> info;
  if (SUCCEEDED(info.QueryFrom(binding_))) {
    char status[10] = {0};
    DWORD buf_size = sizeof(status);
    DWORD flags = 0;
    DWORD reserved = 0;
    if (SUCCEEDED(info->QueryInfo(HTTP_QUERY_STATUS_CODE, status, &buf_size,
                                  &flags, &reserved))) {
      http_status = StringToInt(status);
    } else {
      NOTREACHED() << "Failed to get HTTP status";
    }
  } else {
    NOTREACHED() << "failed to get IWinInetHttpInfo from binding_";
  }

  return http_status;
}

std::string UrlmonUrlRequest::GetHttpHeaders() const {
  if (binding_ == NULL) {
    DLOG(WARNING) << "GetHttpResponseStatus - no binding_";
    return std::string();
  }

  ScopedComPtr<IWinInetHttpInfo> info;
  if (FAILED(info.QueryFrom(binding_))) {
    DLOG(WARNING) << "Failed to QI for IWinInetHttpInfo";
    return std::string();
  }

  return GetRawHttpHeaders(info);
}

void UrlmonUrlRequest::ReleaseBindings() {
  binding_.Release();
  if (bind_context_) {
    ::RevokeBindStatusCallback(bind_context_, this);
    bind_context_.Release();
  }
}

//
// UrlmonUrlRequest::Cache implementation.
//

size_t UrlmonUrlRequest::Cache::Size() const {
  return cache_.size();
}

bool UrlmonUrlRequest::Cache::Read(IStream* dest, size_t size,
                                   size_t* bytes_copied) {
  if (!dest || !size || !is_valid()) {
    NOTREACHED();
    return false;
  }

  // Copy the data to the destination stream and remove it from our cache.
  size_t size_written = 0;
  size_t bytes_to_write = (size <= Size() ? size : Size());

  dest->Write(&cache_[0], bytes_to_write,
              reinterpret_cast<unsigned long*>(&size_written));
  DCHECK(size_written == bytes_to_write);

  cache_.erase(cache_.begin(), cache_.begin() + bytes_to_write);

  if (bytes_copied)
    *bytes_copied = size_written;

  return (size_written != 0);
}

bool UrlmonUrlRequest::Cache::Append(IStream* source,
                                     size_t* bytes_copied) {
  if (!source) {
    NOTREACHED();
    return false;
  }

  HRESULT hr = S_OK;
  while (SUCCEEDED(hr)) {
    DWORD chunk_read = 0;  // NOLINT
    hr = source->Read(read_buffer_, sizeof(read_buffer_), &chunk_read);

    if (!chunk_read)
      break;

    std::copy(read_buffer_, read_buffer_ + chunk_read,
              back_inserter(cache_));

    if (bytes_copied)
      *bytes_copied += chunk_read;
  }

  return SUCCEEDED(hr);
}

net::Error UrlmonUrlRequest::HresultToNetError(HRESULT hr) {
  // Useful reference:
  // http://msdn.microsoft.com/en-us/library/ms775145(VS.85).aspx

  net::Error ret = net::ERR_UNEXPECTED;

  switch (hr) {
    case S_OK:
      ret = net::OK;
      break;

    case MK_E_SYNTAX:
      ret = net::ERR_INVALID_URL;
      break;

    case INET_E_CANNOT_CONNECT:
      ret = net::ERR_CONNECTION_FAILED;
      break;

    case INET_E_DOWNLOAD_FAILURE:
    case INET_E_CONNECTION_TIMEOUT:
    case E_ABORT:
      ret = net::ERR_CONNECTION_ABORTED;
      break;

    case INET_E_DATA_NOT_AVAILABLE:
      ret = net::ERR_EMPTY_RESPONSE;
      break;

    case INET_E_RESOURCE_NOT_FOUND:
      // To behave more closely to the chrome network stack, we translate this
      // error value as tunnel connection failed.  This error value is tested
      // in the ProxyTunnelRedirectTest and UnexpectedServerAuthTest tests.
      ret = net::ERR_TUNNEL_CONNECTION_FAILED;
      break;

    case INET_E_INVALID_URL:
    case INET_E_UNKNOWN_PROTOCOL:
    case INET_E_REDIRECT_FAILED:
      ret = net::ERR_INVALID_URL;
      break;

    case INET_E_INVALID_CERTIFICATE:
      ret = net::ERR_CERT_INVALID;
      break;

    case E_ACCESSDENIED:
      ret = net::ERR_ACCESS_DENIED;
      break;

    default:
      DLOG(WARNING)
          << StringPrintf("TODO: translate HRESULT 0x%08X to net::Error", hr);
      break;
  }
  return ret;
}
