// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_url_request.h"

#include <urlmon.h>
#include <wininet.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "chrome_frame/utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

UrlmonUrlRequest::UrlmonUrlRequest()
    : pending_read_size_(0),
      headers_received_(false),
      calling_delegate_(0),
      thread_(NULL),
      parent_window_(NULL),
      privileged_mode_(false),
      pending_(false),
      is_expecting_download_(true),
      cleanup_transaction_(false) {
  DVLOG(1) << __FUNCTION__ << me();
}

UrlmonUrlRequest::~UrlmonUrlRequest() {
  DVLOG(1) << __FUNCTION__ << me();
}

std::string UrlmonUrlRequest::me() const {
  return base::StringPrintf(" id: %i Obj: %X ", id(), this);
}

bool UrlmonUrlRequest::Start() {
  DVLOG(1) << __FUNCTION__ << me() << url();
  DCHECK(thread_ == 0 || thread_ == base::PlatformThread::CurrentId());
  thread_ = base::PlatformThread::CurrentId();
  status_.Start();
  // Initialize the net::HostPortPair structure from the url initially. We may
  // not receive the ip address of the host if the request is satisfied from
  // the cache.
  socket_address_ = net::HostPortPair::FromURL(GURL(url()));
  // The UrlmonUrlRequest instance can get destroyed in the context of
  // StartAsyncDownload if BindToStorage finishes synchronously with an error.
  // Grab a reference to protect against this.
  scoped_refptr<UrlmonUrlRequest> ref(this);
  HRESULT hr = StartAsyncDownload();
  if (FAILED(hr) && status_.get_state() != UrlmonUrlRequest::Status::DONE) {
    status_.Done();
    status_.set_result(net::URLRequestStatus::FAILED, HresultToNetError(hr));
    NotifyDelegateAndDie();
  }
  return true;
}

void UrlmonUrlRequest::Stop() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DCHECK((status_.get_state() != Status::DONE) == (binding_ != NULL));
  Status::State state = status_.get_state();
  delegate_ = NULL;

  // If DownloadInHost is already requested, we will quit soon anyway.
  if (terminate_requested())
    return;

  switch (state) {
    case Status::WORKING:
      status_.Cancel();
      if (binding_)
        binding_->Abort();
      break;

    case Status::ABORTING:
      status_.Cancel();
      break;

    case Status::DONE:
      status_.Cancel();
      NotifyDelegateAndDie();
      break;
  }
}

bool UrlmonUrlRequest::Read(int bytes_to_read) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DCHECK_GE(bytes_to_read, 0);
  DCHECK_EQ(0, calling_delegate_);
  DVLOG(1) << __FUNCTION__ << me();

  is_expecting_download_ = false;

  // Re-entrancy check. Thou shall not call Read() while process OnReadComplete!
  DCHECK_EQ(0u, pending_read_size_);
  if (pending_read_size_ != 0)
    return false;

  DCHECK((status_.get_state() != Status::DONE) == (binding_ != NULL));
  if (status_.get_state() == Status::ABORTING)
    return true;

  // Send data if available.
  size_t bytes_copied = 0;
  if ((bytes_copied = SendDataToDelegate(bytes_to_read))) {
    DVLOG(1) << __FUNCTION__ << me() << " bytes read: " << bytes_copied;
    return true;
  }

  if (status_.get_state() == Status::WORKING) {
    DVLOG(1) << __FUNCTION__ << me() << " pending: " << bytes_to_read;
    pending_read_size_ = bytes_to_read;
  } else {
    DVLOG(1) << __FUNCTION__ << me() << " Response finished.";
    NotifyDelegateAndDie();
  }

  return true;
}

HRESULT UrlmonUrlRequest::InitPending(const GURL& url, IMoniker* moniker,
                                      IBindCtx* bind_context,
                                      bool enable_frame_busting,
                                      bool privileged_mode,
                                      HWND notification_window,
                                      IStream* cache) {
  DVLOG(1) << __FUNCTION__ << me() << url.spec();
  DCHECK(bind_context_ == NULL);
  DCHECK(moniker_ == NULL);
  DCHECK(cache_ == NULL);
  DCHECK(thread_ == 0 || thread_ == base::PlatformThread::CurrentId());
  thread_ = base::PlatformThread::CurrentId();
  bind_context_ = bind_context;
  moniker_ = moniker;
  enable_frame_busting_ = enable_frame_busting;
  privileged_mode_ = privileged_mode;
  parent_window_ = notification_window;
  cache_ = cache;
  set_url(url.spec());
  set_pending(true);

  // Request has already started and data is fetched. We will get the
  // GetBindInfo call as per contract but the return values are
  // ignored. So just set "get" as a method to make our GetBindInfo
  // implementation happy.
  method_ = "get";
  return S_OK;
}

void UrlmonUrlRequest::TerminateBind(const TerminateBindCallback& callback) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DVLOG(1) << __FUNCTION__ << me();
  cleanup_transaction_ = false;
  if (status_.get_state() == Status::DONE) {
    // Binding is stopped. Note result could be an error.
    callback.Run(moniker_, bind_context_, upload_data_,
                 request_headers_.c_str());
  } else {
    // WORKING (ABORTING?). Save the callback.
    // Now we will return INET_TERMINATE_BIND from ::OnDataAvailable() and in
    // ::OnStopBinding will invoke the callback passing our moniker and
    // bind context.
    terminate_bind_callback_ = callback;
    if (pending_data_) {
      // For downloads to work correctly, we must induce a call to
      // OnDataAvailable so that we can download INET_E_TERMINATED_BIND and
      // get IE into the correct state.
      // To accomplish this we read everything that's readily available in
      // the current stream.  Once we've reached the end of the stream we
      // should get E_PENDING back and then later we'll get that call
      // to OnDataAvailable.
      std::string data;
      base::win::ScopedComPtr<IStream> read_stream(pending_data_);
      HRESULT hr;
      while ((hr = ReadStream(read_stream, 0xffff, &data)) == S_OK) {
        // Just drop the data.
      }
      DLOG_IF(WARNING, hr != E_PENDING) << __FUNCTION__ <<
          base::StringPrintf(" expected E_PENDING but got 0x%08X", hr);
    }
  }
}

size_t UrlmonUrlRequest::SendDataToDelegate(size_t bytes_to_read) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DCHECK_NE(id(), -1);
  DCHECK_GT(bytes_to_read, 0U);
  size_t bytes_copied = 0;
  if (delegate_) {
    std::string read_data;
    if (cache_) {
      HRESULT hr = ReadStream(cache_, bytes_to_read, &read_data);
      if (hr == S_FALSE || read_data.length() < bytes_to_read) {
        DVLOG(1) << __FUNCTION__ << me() << "all cached data read";
        cache_.Release();
      }
    }

    if (read_data.empty() && pending_data_) {
      size_t pending_data_read_save = pending_read_size_;
      pending_read_size_ = 0;

      // AddRef the stream while we call Read to avoid a potential issue
      // where we can get a call to OnDataAvailable while inside Read and
      // in our OnDataAvailable call, we can release the stream object
      // while still using it.
      base::win::ScopedComPtr<IStream> pending(pending_data_);
      HRESULT hr = ReadStream(pending, bytes_to_read, &read_data);
      if (read_data.empty())
        pending_read_size_ = pending_data_read_save;
      // If we received S_FALSE it indicates that there is no more data in the
      // stream. Clear it to ensure that OnStopBinding correctly sends over the
      // response end notification to chrome.
      if (hr == S_FALSE)
        pending_data_.Release();
    }

    bytes_copied = read_data.length();

    if (bytes_copied) {
      ++calling_delegate_;
      DCHECK_NE(id(), -1);
      // The delegate can go away in the middle of ReadStream
      if (delegate_)
        delegate_->OnReadComplete(id(), read_data);
      --calling_delegate_;
    }
  } else {
    DLOG(ERROR) << __FUNCTION__ << me() << "no delegate";
  }

  return bytes_copied;
}

STDMETHODIMP UrlmonUrlRequest::OnStartBinding(DWORD reserved,
                                              IBinding* binding) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  binding_ = binding;
  if (pending_) {
    response_headers_ = GetHttpHeadersFromBinding(binding_);
    DCHECK(!response_headers_.empty());
  }
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
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());

  if (status_.get_state() != Status::WORKING)
    return S_OK;

  // Ignore any notifications received while we are in the pending state
  // waiting for the request to be initiated by Chrome.
  if (pending_ && status_code != BINDSTATUS_REDIRECTING)
    return S_OK;

  if (!delegate_) {
    DVLOG(1) << "Invalid delegate";
    return S_OK;
  }

  switch (status_code) {
    case BINDSTATUS_CONNECTING: {
      if (status_text) {
        socket_address_.set_host(WideToUTF8(status_text));
      }
      break;
    }

    case BINDSTATUS_REDIRECTING: {
      // If we receive a redirect for the initial pending request initiated
      // when our document loads we should stash it away and inform Chrome
      // accordingly when it requests data for the original URL.
      base::win::ScopedComPtr<BindContextInfo> info;
      BindContextInfo::FromBindContext(bind_context_, info.Receive());
      DCHECK(info);
      GURL previously_redirected(info ? info->GetUrl() : std::wstring());
      if (GURL(status_text) != previously_redirected) {
        DVLOG(1) << __FUNCTION__ << me() << "redirect from " << url()
                 << " to " << status_text;
        // Fetch the redirect status as they aren't all equal (307 in particular
        // retains the HTTP request verb).
        int http_code = GetHttpResponseStatusFromBinding(binding_);
        status_.SetRedirected(http_code, WideToUTF8(status_text));
        // Abort. We will inform Chrome in OnStopBinding callback.
        binding_->Abort();
        return E_ABORT;
      }
      break;
    }

    case BINDSTATUS_COOKIE_SENT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_READ);
      break;

    case BINDSTATUS_COOKIE_SUPPRESSED:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_SUPPRESS);
      break;

    case BINDSTATUS_COOKIE_STATE_ACCEPT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_ACCEPT);
      break;

    case BINDSTATUS_COOKIE_STATE_REJECT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_REJECT);
      break;

    case BINDSTATUS_COOKIE_STATE_LEASH:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_LEASH);
      break;

    case BINDSTATUS_COOKIE_STATE_DOWNGRADE:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_DOWNGRADE);
      break;

    case BINDSTATUS_COOKIE_STATE_UNKNOWN:
      NOTREACHED() << L"Unknown cookie state received";
      break;

    default:
      DVLOG(1) << __FUNCTION__ << me()
               << base::StringPrintf(L"code: %i status: %ls", status_code,
                                     status_text);
      break;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnStopBinding(HRESULT result, LPCWSTR error) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DVLOG(1) << __FUNCTION__ << me()
           << "- Request stopped, Result: " << std::hex << result;
  DCHECK(status_.get_state() == Status::WORKING ||
         status_.get_state() == Status::ABORTING);

  Status::State state = status_.get_state();

  // Mark we a are done.
  status_.Done();

  if (result == INET_E_TERMINATED_BIND) {
    if (terminate_requested()) {
      terminate_bind_callback_.Run(moniker_, bind_context_, upload_data_,
                                   request_headers_.c_str());
    } else {
      cleanup_transaction_ = true;
    }
    // We may have returned INET_E_TERMINATED_BIND from OnDataAvailable.
    result = S_OK;
  }

  if (state == Status::WORKING) {
    status_.set_result(result);

    // Special case. If the last request was a redirect and the current OS
    // error value is E_ACCESSDENIED, that means an unsafe redirect was
    // attempted. In that case, correct the OS error value to be the more
    // specific ERR_UNSAFE_REDIRECT error value.
    if (result == E_ACCESSDENIED) {
      int http_code = GetHttpResponseStatusFromBinding(binding_);
      if (300 <= http_code && http_code < 400) {
        status_.set_result(net::URLRequestStatus::FAILED,
                           net::ERR_UNSAFE_REDIRECT);
      }
    }

    // The code below seems easy but it is not. :)
    // The network policy in Chrome network is that error code/end_of_stream
    // should be returned only as a result of read (or start) request.
    // Here are the possible cases:
    // pending_data_|pending_read
    //     FALSE  |FALSE   => EndRequest if no headers, otherwise wait for Read.
    //     FALSE  |TRUE    => EndRequest.
    //     TRUE   |FALSE   => Wait for Read.
    //     TRUE   |TRUE    => Something went wrong!!

    if (pending_data_) {
      DCHECK_EQ(pending_read_size_, 0UL);
      ReleaseBindings();
      return S_OK;
    }

    if (headers_received_ && pending_read_size_ == 0) {
      ReleaseBindings();
      return S_OK;
    }

    // No headers or there is a pending read from Chrome.
    NotifyDelegateAndDie();
    return S_OK;
  }

  // Status::ABORTING
  if (status_.was_redirected()) {
    // Just release bindings here. Chrome will issue EndRequest(request_id)
    // after processing headers we had provided.
    if (!pending_) {
      std::string headers = GetHttpHeadersFromBinding(binding_);
      OnResponse(0, UTF8ToWide(headers).c_str(), NULL, NULL);
    }
    ReleaseBindings();
    return S_OK;
  }

  // Stop invoked.
  NotifyDelegateAndDie();
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetBindInfo(DWORD* bind_flags,
                                           BINDINFO* bind_info) {
  if ((bind_info == NULL) || (bind_info->cbSize == 0) || (bind_flags == NULL))
    return E_INVALIDARG;

  *bind_flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;

  bind_info->dwOptionsFlags = INTERNET_FLAG_NO_AUTO_REDIRECT;
  bind_info->dwOptions = BINDINFO_OPTIONS_WININETFLAG;

  // TODO(ananta)
  // Look into whether the other load flags need to be supported in chrome
  // frame.
  if (load_flags_ & net::LOAD_VALIDATE_CACHE)
    *bind_flags |= BINDF_RESYNCHRONIZE;

  if (load_flags_ & net::LOAD_BYPASS_CACHE)
    *bind_flags |= BINDF_GETNEWESTVERSION;

  if (LowerCaseEqualsASCII(method(), "get")) {
    bind_info->dwBindVerb = BINDVERB_GET;
  } else if (LowerCaseEqualsASCII(method(), "post")) {
    bind_info->dwBindVerb = BINDVERB_POST;
  } else if (LowerCaseEqualsASCII(method(), "put")) {
    bind_info->dwBindVerb = BINDVERB_PUT;
  } else {
    std::wstring verb(ASCIIToWide(StringToUpperASCII(method())));
    bind_info->dwBindVerb = BINDVERB_CUSTOM;
    bind_info->szCustomVerb = reinterpret_cast<wchar_t*>(
        ::CoTaskMemAlloc((verb.length() + 1) * sizeof(wchar_t)));
    lstrcpyW(bind_info->szCustomVerb, verb.c_str());
  }

  if (bind_info->dwBindVerb == BINDVERB_POST ||
      bind_info->dwBindVerb == BINDVERB_PUT ||
      post_data_len() > 0) {
    // Bypass caching proxies on upload requests and avoid writing responses to
    // the browser's cache.
    *bind_flags |= BINDF_GETNEWESTVERSION | BINDF_PRAGMA_NO_CACHE;

    // Attempt to avoid storing the response for upload requests.
    // See http://crbug.com/55918
    if (resource_type_ != ResourceType::MAIN_FRAME)
      *bind_flags |= BINDF_NOWRITECACHE;

    // Initialize the STGMEDIUM.
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;

    if (bind_info->dwBindVerb != BINDVERB_CUSTOM)
      bind_info->szCustomVerb = NULL;

    if (post_data_len() &&
        get_upload_data(&bind_info->stgmedData.pstm) == S_OK) {
      bind_info->stgmedData.tymed = TYMED_ISTREAM;
#pragma warning(disable:4244)
      bind_info->cbstgmedData = post_data_len();
#pragma warning(default:4244)
      DVLOG(1) << __FUNCTION__ << me() << method()
               << " request with " << base::Int64ToString(post_data_len())
               << " bytes. url=" << url();
    } else {
      DVLOG(1) << __FUNCTION__ << me() << "POST request with no data!";
    }
  }
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnDataAvailable(DWORD flags, DWORD size,
                                               FORMATETC* formatetc,
                                               STGMEDIUM* storage) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DVLOG(1) << __FUNCTION__ << me() << "bytes available: " << size;

  if (terminate_requested()) {
    DVLOG(1) << " Download requested. INET_E_TERMINATED_BIND returned";
    return INET_E_TERMINATED_BIND;
  }

  if (!storage || (storage->tymed != TYMED_ISTREAM)) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  IStream* read_stream = storage->pstm;
  if (!read_stream) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  // Some requests such as HEAD have zero data.
  if (size > 0)
    pending_data_ = read_stream;

  if (pending_read_size_) {
    size_t bytes_copied = SendDataToDelegate(pending_read_size_);
    DVLOG(1) << __FUNCTION__ << me() << "size read: " << bytes_copied;
  } else {
    DVLOG(1) << __FUNCTION__ << me() << "- waiting for remote read";
  }

  if (BSCF_LASTDATANOTIFICATION & flags) {
    if (!is_expecting_download_ || pending()) {
      DVLOG(1) << __FUNCTION__ << me() << "EOF";
      return S_OK;
    }
    // Always return INET_E_TERMINATED_BIND to allow bind context reuse
    // if DownloadToHost is suddenly requested.
    DVLOG(1) << __FUNCTION__ << " EOF: INET_E_TERMINATED_BIND returned";
    return INET_E_TERMINATED_BIND;
  }
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnObjectAvailable(REFIID iid, IUnknown* object) {
  // We are calling BindToStorage on the moniker we should always get called
  // back on OnDataAvailable and should never get OnObjectAvailable
  NOTREACHED();
  return E_NOTIMPL;
}

STDMETHODIMP UrlmonUrlRequest::BeginningTransaction(const wchar_t* url,
    const wchar_t* current_headers, DWORD reserved,
    wchar_t** additional_headers) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (!additional_headers) {
    NOTREACHED();
    return E_POINTER;
  }

  DVLOG(1) << __FUNCTION__ << me() << "headers: \n" << current_headers;

  if (status_.get_state() == Status::ABORTING) {
    // At times the BINDSTATUS_REDIRECTING notification which is sent to the
    // IBindStatusCallback interface does not have an accompanying HTTP
    // redirect status code, i.e. the attempt to query the HTTP status code
    // from the binding returns 0, 200, etc which are invalid redirect codes.
    // We don't want urlmon to follow redirects. We return E_ABORT in our
    // IBindStatusCallback::OnProgress function and also abort the binding.
    // However urlmon still tries to establish a transaction with the
    // redirected URL which confuses the web server.
    // Fix is to abort the attempted transaction.
    DLOG(WARNING) << __FUNCTION__ << me()
                  << ": Aborting connection to URL:"
                  << url
                  << " as the binding has been aborted";
    return E_ABORT;
  }

  HRESULT hr = S_OK;

  std::string new_headers;
  if (post_data_len() > 0) {
    if (is_chunked_upload()) {
      new_headers = base::StringPrintf("Transfer-Encoding: chunked\r\n");
    }
  }

  if (!extra_headers().empty()) {
    // TODO(robertshield): We may need to sanitize headers on POST here.
    new_headers += extra_headers();
  }

  if (!referrer().empty()) {
    // Referrer is famously misspelled in HTTP:
    new_headers += base::StringPrintf("Referer: %s\r\n", referrer().c_str());
  }

  // In the rare case if "User-Agent" string is already in |current_headers|.
  // We send Chrome's user agent in requests initiated within ChromeFrame to
  // enable third party content in pages rendered in ChromeFrame to correctly
  // send content for Chrome as the third party content may not be equipped to
  // identify chromeframe as the user agent. This also ensures that the user
  // agent reported in scripts in chrome frame is consistent with that sent
  // in outgoing requests.
  std::string user_agent = http_utils::AddChromeFrameToUserAgentValue(
      http_utils::GetChromeUserAgent());
  new_headers += ReplaceOrAddUserAgent(current_headers, user_agent);

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
  request_headers_ = new_headers;
  return hr;
}

STDMETHODIMP UrlmonUrlRequest::OnResponse(DWORD dwResponseCode,
    const wchar_t* response_headers, const wchar_t* request_headers,
    wchar_t** additional_headers) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DVLOG(1) << __FUNCTION__ << me() << "headers: \n" << response_headers;

  if (!delegate_) {
    DLOG(WARNING) << "Invalid delegate";
    return S_OK;
  }

  std::string raw_headers = WideToUTF8(response_headers);

  delegate_->AddPrivacyDataForUrl(url(), "", 0);

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
  if (enable_frame_busting_) {
    if (http_utils::HasFrameBustingHeader(raw_headers)) {
      DLOG(ERROR) << "X-Frame-Options header other than ALLOWALL " <<
          "detected, navigation canceled";
      return E_FAIL;
    }
  }

  DVLOG(1) << __FUNCTION__ << me() << "Calling OnResponseStarted";

  // Inform the delegate.
  headers_received_ = true;
  DCHECK_NE(id(), -1);
  delegate_->OnResponseStarted(id(),
                    "",                   // mime_type
                    raw_headers.c_str(),  // headers
                    0,                    // size
                    base::Time(),         // last_modified
                    status_.get_redirection().utf8_url,
                    status_.get_redirection().http_code,
                    socket_address_);
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetWindow(const GUID& guid_reason,
                                         HWND* parent_window) {
  if (!parent_window)
    return E_INVALIDARG;

#ifndef NDEBUG
  wchar_t guid[40] = {0};
  ::StringFromGUID2(guid_reason, guid, arraysize(guid));
  const wchar_t* str = guid;
  if (guid_reason == IID_IAuthenticate)
    str = L"IAuthenticate";
  else if (guid_reason == IID_IHttpSecurity)
    str = L"IHttpSecurity";
  else if (guid_reason == IID_IWindowForBindingUI)
    str = L"IWindowForBindingUI";
  DVLOG(1) << __FUNCTION__ << me() << "GetWindow: " << str;
#endif
  // We should return a non-NULL HWND as parent. Otherwise no dialog is shown.
  // TODO(iyengar): This hits when running the URL request tests.
  DLOG_IF(WARNING, !::IsWindow(parent_window_))
      << "UrlmonUrlRequest::GetWindow - no window!";
  *parent_window = parent_window_;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::Authenticate(HWND* parent_window,
                                            LPWSTR* user_name,
                                            LPWSTR* password) {
  if (!parent_window)
    return E_INVALIDARG;

  if (privileged_mode_)
    return E_ACCESSDENIED;

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
  DVLOG(1) << __FUNCTION__ << me() << "Security problem : " << problem;

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

HRESULT UrlmonUrlRequest::StartAsyncDownload() {
  DVLOG(1) << __FUNCTION__ << me() << url();
  HRESULT hr = E_FAIL;
  DCHECK((moniker_ && bind_context_) || (!moniker_ && !bind_context_));

  if (!moniker_.get()) {
    std::wstring wide_url = UTF8ToWide(url());
    hr = CreateURLMonikerEx(NULL, wide_url.c_str(), moniker_.Receive(),
                            URL_MK_UNIFORM);
    if (FAILED(hr)) {
      NOTREACHED() << "CreateURLMonikerEx failed. Error: " << hr;
      return hr;
    }
  }

  if (bind_context_.get() == NULL)  {
    hr = ::CreateAsyncBindCtxEx(NULL, 0, this, NULL,
                                bind_context_.Receive(), 0);
    DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtxEx failed. Error: " << hr;
  } else {
    // Use existing bind context.
    hr = ::RegisterBindStatusCallback(bind_context_, this, NULL, 0);
    DCHECK(SUCCEEDED(hr)) << "RegisterBindStatusCallback failed. Error: " << hr;
  }

  if (SUCCEEDED(hr)) {
    base::win::ScopedComPtr<IStream> stream;

    // BindToStorage may complete synchronously.
    // We still get all the callbacks - OnStart/StopBinding, this may result
    // in destruction of our object. It's fine but we access some members
    // below for debug info. :)
    base::win::ScopedComPtr<IHttpSecurity> self(this);

    // Inform our moniker patch this binding should not be tortured.
    base::win::ScopedComPtr<BindContextInfo> info;
    BindContextInfo::FromBindContext(bind_context_, info.Receive());
    DCHECK(info);
    if (info)
      info->set_chrome_request(true);

    hr = moniker_->BindToStorage(bind_context_, NULL, __uuidof(IStream),
                                 reinterpret_cast<void**>(stream.Receive()));
    if (hr == S_OK)
      DCHECK(binding_ != NULL || status_.get_state() == Status::DONE);

    if (FAILED(hr)) {
      // TODO(joshia): Look into. This currently fails for:
      // http://user2:secret@localhost:1337/auth-basic?set-cookie-if-challenged
      // when running the UrlRequest unit tests.
      DLOG(ERROR) << __FUNCTION__ << me() <<
          base::StringPrintf("IUrlMoniker::BindToStorage failed 0x%08X.", hr);
      // In most cases we'll get a MK_E_SYNTAX error here but if we abort
      // the navigation ourselves such as in the case of seeing something
      // else than ALLOWALL in X-Frame-Options.
    }
  }

  DLOG_IF(ERROR, FAILED(hr)) << me() <<
      base::StringPrintf(L"StartAsyncDownload failed: 0x%08X", hr);

  return hr;
}

void UrlmonUrlRequest::NotifyDelegateAndDie() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  DVLOG(1) << __FUNCTION__ << me();

  PluginUrlRequestDelegate* delegate = delegate_;
  delegate_ = NULL;
  ReleaseBindings();
  TerminateTransaction();
  if (delegate && id() != -1) {
    net::URLRequestStatus result = status_.get_result();
    delegate->OnResponseEnd(id(), result);
  } else {
    DLOG(WARNING) << __FUNCTION__ << me() << "no delegate";
  }
}

void UrlmonUrlRequest::TerminateTransaction() {
  if (cleanup_transaction_ && bind_context_ && moniker_) {
    // We return INET_E_TERMINATED_BIND from our OnDataAvailable implementation
    // to ensure that the transaction stays around if Chrome decides to issue
    // a download request when it finishes inspecting the headers received in
    // OnResponse. However this causes the urlmon transaction object to leak.
    // To workaround this we save away the IInternetProtocol interface which is
    // implemented by the urlmon CTransaction object in our BindContextInfo
    // instance which is maintained per bind context. Invoking Terminate
    // on this with the special flags 0x2000000 cleanly releases the
    // transaction.
    static const int kUrlmonTerminateTransactionFlags = 0x2000000;
    base::win::ScopedComPtr<BindContextInfo> info;
    BindContextInfo::FromBindContext(bind_context_, info.Receive());
    DCHECK(info);
    if (info && info->protocol()) {
      info->protocol()->Terminate(kUrlmonTerminateTransactionFlags);
    }
  }
  bind_context_.Release();
}

void UrlmonUrlRequest::ReleaseBindings() {
  binding_.Release();
  // Do not release bind_context here!
  // We may get DownloadToHost request and therefore we want the bind_context
  // to be available.
  if (bind_context_)
    ::RevokeBindStatusCallback(bind_context_, this);
}

net::Error UrlmonUrlRequest::HresultToNetError(HRESULT hr) {
  const int kInvalidHostName = 0x8007007b;
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

    // The following error codes can be returned while processing an invalid
    // url. http://msdn.microsoft.com/en-us/library/bb250493(v=vs.85).aspx
    case INET_E_INVALID_URL:
    case INET_E_UNKNOWN_PROTOCOL:
    case INET_E_REDIRECT_FAILED:
    case INET_E_SECURITY_PROBLEM:
    case kInvalidHostName:
    case E_INVALIDARG:
    case E_OUTOFMEMORY:
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
          << base::StringPrintf("TODO: translate HRESULT 0x%08X to net::Error",
                                hr);
      break;
  }
  return ret;
}


PluginUrlRequestManager::ThreadSafeFlags
    UrlmonUrlRequestManager::GetThreadSafeFlags() {
  return PluginUrlRequestManager::NOT_THREADSAFE;
}

void UrlmonUrlRequestManager::SetInfoForUrl(const std::wstring& url,
                                            IMoniker* moniker, LPBC bind_ctx) {
  CComObject<UrlmonUrlRequest>* new_request = NULL;
  CComObject<UrlmonUrlRequest>::CreateInstance(&new_request);
  if (new_request) {
    GURL start_url(url);
    DCHECK(start_url.is_valid());
    DCHECK(pending_request_ == NULL);

    base::win::ScopedComPtr<BindContextInfo> info;
    BindContextInfo::FromBindContext(bind_ctx, info.Receive());
    DCHECK(info);
    IStream* cache = info ? info->cache() : NULL;
    pending_request_ = new_request;
    pending_request_->InitPending(start_url, moniker, bind_ctx,
                                  enable_frame_busting_, privileged_mode_,
                                  notification_window_, cache);
    // Start the request
    bool is_started = pending_request_->Start();
    DCHECK(is_started);
  }
}

void UrlmonUrlRequestManager::StartRequest(int request_id,
    const AutomationURLRequest& request_info) {
  DVLOG(1) << __FUNCTION__ << " id: " << request_id;

  if (stopping_) {
    DLOG(WARNING) << __FUNCTION__ << " request not started (stopping)";
    return;
  }

  DCHECK(request_map_.find(request_id) == request_map_.end());
#ifndef NDEBUG
  if (background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    DCHECK(background_request_map_.find(request_id) ==
           background_request_map_.end());
  }
#endif  // NDEBUG
  DCHECK(GURL(request_info.url).is_valid());

  // Non frame requests like sub resources, images, etc are handled on the
  // background thread.
  if (background_worker_thread_enabled_ &&
      !ResourceType::IsFrame(
          static_cast<ResourceType::Type>(request_info.resource_type))) {
    DLOG(INFO) << "Downloading resource type "
               << request_info.resource_type
               << " on background thread";
    background_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&UrlmonUrlRequestManager::StartRequestHelper,
                   base::Unretained(this), request_id, request_info,
                   &background_request_map_, &background_resource_map_lock_));
    return;
  }
  StartRequestHelper(request_id, request_info, &request_map_, NULL);
}

void UrlmonUrlRequestManager::StartRequestHelper(
    int request_id,
    const AutomationURLRequest& request_info,
    RequestMap* request_map,
    base::Lock* request_map_lock) {
  DCHECK(request_map);
  scoped_refptr<UrlmonUrlRequest> new_request;
  bool is_started = false;
  if (pending_request_) {
    if (pending_request_->url() != request_info.url) {
      DLOG(INFO) << __FUNCTION__
                 << "Received url request for url:"
                 << request_info.url
                 << ". Stopping pending url request for url:"
                 << pending_request_->url();
      pending_request_->Stop();
      pending_request_ = NULL;
    } else {
      new_request.swap(pending_request_);
      is_started = true;
      DVLOG(1) << __FUNCTION__ << new_request->me()
               << " assigned id " << request_id;
    }
  }

  if (!is_started) {
    CComObject<UrlmonUrlRequest>* created_request = NULL;
    CComObject<UrlmonUrlRequest>::CreateInstance(&created_request);
    new_request = created_request;
  }

  new_request->Initialize(static_cast<PluginUrlRequestDelegate*>(this),
      request_id,
      request_info.url,
      request_info.method,
      request_info.referrer,
      request_info.extra_request_headers,
      request_info.upload_data,
      static_cast<ResourceType::Type>(request_info.resource_type),
      enable_frame_busting_,
      request_info.load_flags);
  new_request->set_parent_window(notification_window_);
  new_request->set_privileged_mode(privileged_mode_);

  if (request_map_lock)
    request_map_lock->Acquire();

  (*request_map)[request_id] = new_request;

  if (request_map_lock)
    request_map_lock->Release();

  if (!is_started) {
    // Freshly created, start now.
    new_request->Start();
  } else {
    // Request is already underway, call OnResponse so that the
    // other side can start reading.
    DCHECK(!new_request->response_headers().empty());
    new_request->OnResponse(
        0, UTF8ToWide(new_request->response_headers()).c_str(), NULL, NULL);
  }
}

void UrlmonUrlRequestManager::ReadRequest(int request_id, int bytes_to_read) {
  DVLOG(1) << __FUNCTION__ << " id: " << request_id;
  // if we fail to find the request in the normal map and the background
  // request map, it may mean that the request could have failed with a
  // network error.
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id,
                                                          &request_map_);
  if (request) {
    request->Read(bytes_to_read);
  } else if (background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    request = LookupRequest(request_id, &background_request_map_);
    if (request) {
      background_thread_->message_loop()->PostTask(
          FROM_HERE, base::IgnoreReturn<bool>(base::Bind(
              &UrlmonUrlRequest::Read, request.get(), bytes_to_read)));
    }
  }
  if (!request)
    DLOG(ERROR) << __FUNCTION__ << " no request found for " << request_id;
}

void UrlmonUrlRequestManager::DownloadRequestInHost(int request_id) {
  DVLOG(1) << __FUNCTION__ << " " << request_id;
  if (!IsWindow(notification_window_)) {
    NOTREACHED() << "Cannot handle download if we don't have anyone to hand it "
                    "to.";
    return;
  }

  scoped_refptr<UrlmonUrlRequest> request(LookupRequest(request_id,
                                                        &request_map_));
  if (request) {
    DownloadRequestInHostHelper(request);
  } else if (background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    request = LookupRequest(request_id, &background_request_map_);
    if (request) {
      background_thread_->message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&UrlmonUrlRequestManager::DownloadRequestInHostHelper,
                     base::Unretained(this), request.get()));
    }
  }
  if (!request)
    DLOG(ERROR) << __FUNCTION__ << " no request found for " << request_id;
}

void UrlmonUrlRequestManager::DownloadRequestInHostHelper(
    UrlmonUrlRequest* request) {
  DCHECK(request);
  UrlmonUrlRequest::TerminateBindCallback callback =
      base::Bind(&UrlmonUrlRequestManager::BindTerminated,
                 base::Unretained(this));
  request->TerminateBind(callback);
}

void UrlmonUrlRequestManager::BindTerminated(IMoniker* moniker,
                                             IBindCtx* bind_ctx,
                                             IStream* post_data,
                                             const char* request_headers) {
  DownloadInHostParams* download_params = new DownloadInHostParams;
  download_params->bind_ctx = bind_ctx;
  download_params->moniker = moniker;
  download_params->post_data = post_data;
  if (request_headers) {
    download_params->request_headers = request_headers;
  }
  ::PostMessage(notification_window_, WM_DOWNLOAD_IN_HOST,
        reinterpret_cast<WPARAM>(download_params), 0);
}

void UrlmonUrlRequestManager::GetCookiesForUrl(const GURL& url, int cookie_id) {
  DWORD cookie_size = 0;
  bool success = true;
  std::string cookie_string;

  int32 cookie_action = COOKIEACTION_READ;
  BOOL result = InternetGetCookieA(url.spec().c_str(), NULL, NULL,
                                   &cookie_size);
  DWORD error = 0;
  if (cookie_size) {
    scoped_array<char> cookies(new char[cookie_size + 1]);
    if (!InternetGetCookieA(url.spec().c_str(), NULL, cookies.get(),
                            &cookie_size)) {
      success = false;
      error = GetLastError();
      NOTREACHED() << "InternetGetCookie failed. Error: " << error;
    } else {
      cookie_string = cookies.get();
    }
  } else {
    success = false;
    error = GetLastError();
    DVLOG(1) << "InternetGetCookie failed. Error: " << error;
  }

  OnCookiesRetrieved(success, url, cookie_string, cookie_id);
  if (!success && !error)
    cookie_action = COOKIEACTION_SUPPRESS;

  AddPrivacyDataForUrl(url.spec(), "", cookie_action);
}

void UrlmonUrlRequestManager::SetCookiesForUrl(const GURL& url,
                                               const std::string& cookie) {
  DCHECK(container_);
  // Grab a reference on the container to ensure that we don't get destroyed in
  // case the InternetSetCookie call below puts up a dialog box, which can
  // happen if the cookie policy is set to prompt.
  if (container_) {
    container_->AddRef();
  }

  InternetCookieState cookie_state = static_cast<InternetCookieState>(
      InternetSetCookieExA(url.spec().c_str(), NULL, cookie.c_str(),
                           INTERNET_COOKIE_EVALUATE_P3P, NULL));

  int32 cookie_action = MapCookieStateToCookieAction(cookie_state);
  AddPrivacyDataForUrl(url.spec(), "", cookie_action);

  if (container_) {
    container_->Release();
  }
}

void UrlmonUrlRequestManager::EndRequest(int request_id) {
  DVLOG(1) << __FUNCTION__ << " id: " << request_id;
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id,
                                                          &request_map_);
  if (request) {
    request_map_.erase(request_id);
    request->Stop();
  } else if (background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    request = LookupRequest(request_id, &background_request_map_);
    if (request) {
      background_request_map_.erase(request_id);
      background_thread_->message_loop()->PostTask(
          FROM_HERE, base::Bind(&UrlmonUrlRequest::Stop, request.get()));
    }
  }
  if (!request)
    DLOG(ERROR) << __FUNCTION__ << " no request found for " << request_id;
}

void UrlmonUrlRequestManager::StopAll() {
  DVLOG(1) << __FUNCTION__;
  if (stopping_)
    return;

  stopping_ = true;

  DVLOG(1) << __FUNCTION__ << " stopping " << request_map_.size()
           << " requests";

  StopAllRequestsHelper(&request_map_, NULL);

  if (background_worker_thread_enabled_) {
    DCHECK(background_thread_.get());
    background_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&UrlmonUrlRequestManager::StopAllRequestsHelper,
                              base::Unretained(this), &background_request_map_,
                              &background_resource_map_lock_));
    background_thread_->Stop();
    background_thread_.reset();
  }
}

void UrlmonUrlRequestManager::StopAllRequestsHelper(
    RequestMap* request_map,
    base::Lock* request_map_lock) {
  DCHECK(request_map);

  DVLOG(1) << __FUNCTION__ << " stopping " << request_map->size()
           << " requests";

  if (request_map_lock)
    request_map_lock->Acquire();

  for (RequestMap::iterator it = request_map->begin();
       it != request_map->end(); ++it) {
    DCHECK(it->second != NULL);
    it->second->Stop();
  }
  request_map->clear();

  if (request_map_lock)
    request_map_lock->Release();
}

void UrlmonUrlRequestManager::OnResponseStarted(int request_id,
    const char* mime_type, const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status, const net::HostPortPair& socket_address) {
  DCHECK_NE(request_id, -1);
  DVLOG(1) << __FUNCTION__;

#ifndef NDEBUG
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id,
                                                          &request_map_);
  if (request == NULL && background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    request = LookupRequest(request_id, &background_request_map_);
  }
  DCHECK(request != NULL);
#endif  // NDEBUG
  delegate_->OnResponseStarted(request_id, mime_type, headers, size,
      last_modified, redirect_url, redirect_status, socket_address);
}

void UrlmonUrlRequestManager::OnReadComplete(int request_id,
                                             const std::string& data) {
  DCHECK_NE(request_id, -1);
  DVLOG(1) << __FUNCTION__ << " id: " << request_id;
#ifndef NDEBUG
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id,
                                                          &request_map_);
  if (request == NULL && background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    request = LookupRequest(request_id, &background_request_map_);
  }
  DCHECK(request != NULL);
#endif  // NDEBUG
  delegate_->OnReadComplete(request_id, data);
  DVLOG(1) << __FUNCTION__ << " done id: " << request_id;
}

void UrlmonUrlRequestManager::OnResponseEnd(
    int request_id,
    const net::URLRequestStatus& status) {
  DCHECK_NE(request_id, -1);
  DVLOG(1) << __FUNCTION__;
  DCHECK(status.status() != net::URLRequestStatus::CANCELED);
  RequestMap::size_type erased_count = request_map_.erase(request_id);
  if (erased_count != 1u && background_worker_thread_enabled_) {
    base::AutoLock lock(background_resource_map_lock_);
    erased_count = background_request_map_.erase(request_id);
    if (erased_count != 1u) {
      DLOG(WARNING) << __FUNCTION__
                    << " Failed to find request id:"
                    << request_id;
    }
  }
  delegate_->OnResponseEnd(request_id, status);
}

void UrlmonUrlRequestManager::OnCookiesRetrieved(bool success, const GURL& url,
    const std::string& cookie_string, int cookie_id) {
  DCHECK(url.is_valid());
  delegate_->OnCookiesRetrieved(success, url, cookie_string, cookie_id);
}

scoped_refptr<UrlmonUrlRequest> UrlmonUrlRequestManager::LookupRequest(
    int request_id, RequestMap* request_map) {
  RequestMap::iterator it = request_map->find(request_id);
  if (request_map->end() != it)
    return it->second;
  return NULL;
}

UrlmonUrlRequestManager::UrlmonUrlRequestManager()
    : stopping_(false), notification_window_(NULL),
      privileged_mode_(false),
      container_(NULL),
      background_worker_thread_enabled_(true) {
  background_thread_.reset(new ResourceFetcherThread(
      "cf_iexplore_background_thread"));
  background_worker_thread_enabled_ =
      GetConfigBool(true, kUseBackgroundThreadForSubResources);
  if (background_worker_thread_enabled_) {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_UI;
    background_thread_->StartWithOptions(options);
  }
}

UrlmonUrlRequestManager::~UrlmonUrlRequestManager() {
  StopAll();
}

void UrlmonUrlRequestManager::AddPrivacyDataForUrl(
    const std::string& url, const std::string& policy_ref,
    int32 flags) {
  DCHECK(!url.empty());

  bool fire_privacy_event = false;

  if (privacy_info_.privacy_records.empty())
    flags |= PRIVACY_URLISTOPLEVEL;

  if (!privacy_info_.privacy_impacted) {
    if (flags & (COOKIEACTION_ACCEPT | COOKIEACTION_REJECT |
                 COOKIEACTION_DOWNGRADE)) {
      privacy_info_.privacy_impacted = true;
      fire_privacy_event = true;
    }
  }

  PrivacyInfo::PrivacyEntry& privacy_entry =
      privacy_info_.privacy_records[UTF8ToWide(url)];

  privacy_entry.flags |= flags;
  privacy_entry.policy_ref = UTF8ToWide(policy_ref);

  if (fire_privacy_event && IsWindow(notification_window_)) {
    PostMessage(notification_window_, WM_FIRE_PRIVACY_CHANGE_NOTIFICATION, 1,
                0);
  }
}

UrlmonUrlRequestManager::ResourceFetcherThread::ResourceFetcherThread(
    const char* name) : base::Thread(name) {
}

UrlmonUrlRequestManager::ResourceFetcherThread::~ResourceFetcherThread() {
  Stop();
}

void UrlmonUrlRequestManager::ResourceFetcherThread::Init() {
  CoInitialize(NULL);
}

void UrlmonUrlRequestManager::ResourceFetcherThread::CleanUp() {
  CoUninitialize();
}

