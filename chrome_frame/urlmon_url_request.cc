// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_url_request.h"

#include <wininet.h>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/logging.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "chrome_frame/utils.h"
#include "net/http/http_util.h"
#include "net/http/http_response_headers.h"

static const LARGE_INTEGER kZero = {0};
static const ULARGE_INTEGER kUnsignedZero = {0};
int UrlmonUrlRequest::instance_count_ = 0;
const char kXFrameOptionsHeader[] = "X-Frame-Options";

UrlmonUrlRequest::UrlmonUrlRequest()
    : pending_read_size_(0),
      status_(URLRequestStatus::FAILED, net::ERR_FAILED),
      thread_(PlatformThread::CurrentId()),
      is_request_started_(false),
      post_data_len_(0),
      redirect_status_(0),
      parent_window_(NULL) {
  DLOG(INFO) << StringPrintf("Created request. Obj: %X", this)
      << " Count: " << ++instance_count_;
}

UrlmonUrlRequest::~UrlmonUrlRequest() {
  DLOG(INFO) << StringPrintf("Deleted request. Obj: %X", this)
      << " Count: " << --instance_count_;
}

bool UrlmonUrlRequest::Start() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  status_.set_status(URLRequestStatus::IO_PENDING);
  HRESULT hr = StartAsyncDownload();
  if (FAILED(hr)) {
    // Do not call EndRequest() here since it will attempt to free references
    // that have not been established.
    status_.set_os_error(HresultToNetError(hr));
    status_.set_status(URLRequestStatus::FAILED);
    DLOG(ERROR) << "StartAsyncDownload failed";
    OnResponseEnd(status_);
    return false;
  }

  // Take a self reference to maintain COM lifetime. This will be released
  // in EndRequest. Set a flag indicating that we have an additional
  // reference here
  is_request_started_ = true;
  AddRef();
  request_handler()->AddRequest(this);
  return true;
}

void UrlmonUrlRequest::Stop() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  if (binding_) {
    binding_->Abort();
  } else {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_os_error(net::ERR_FAILED);
    EndRequest();
  }
}

bool UrlmonUrlRequest::Read(int bytes_to_read) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this);

  // Send cached data if available.
  CComObjectStackEx<SendStream> send_stream;
  send_stream.Initialize(this);
  size_t bytes_copied = 0;
  if (cached_data_.is_valid() && cached_data_.Read(&send_stream, bytes_to_read,
                                                   &bytes_copied)) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X - bytes read from cache: %d",
                               url().c_str(), this, bytes_copied);
    return true;
  }

  // if the request is finished or there's nothing more to read
  // then end the request
  if (!status_.is_io_pending() || !binding_) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X. Response finished. Status: %d",
                               url().c_str(), this, status_.status());
    EndRequest();
    return true;
  }

  pending_read_size_ = bytes_to_read;
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      "- Read pending for: " << bytes_to_read;
  return true;
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
  switch (status_code) {
    case BINDSTATUS_REDIRECTING:
      DCHECK(status_text != NULL);
      DLOG(INFO) << "URL: " << url() << " redirected to "
                 << status_text;
      redirect_url_ = status_text;
      // Fetch the redirect status as they aren't all equal (307 in particular
      // retains the HTTP request verb).
      redirect_status_ = GetHttpResponseStatus();
      break;

    default:
      DLOG(INFO) << " Obj: " << std::hex << this << " OnProgress(" << url()
          << StringPrintf(L") code: %i status: %ls", status_code, status_text);
      break;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnStopBinding(HRESULT result, LPCWSTR error) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

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
  }

  // Release these variables after reporting EndRequest since we might need to
  // access their state.
  binding_ = NULL;
  bind_context_ = NULL;

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetBindInfo(DWORD* bind_flags,
    BINDINFO *bind_info) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

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

    scoped_refptr<net::UploadData> upload_data(upload_data());
    post_data_len_ = upload_data.get() ? upload_data->GetContentLength() : 0;
    if (post_data_len_) {
      DLOG(INFO) << " Obj: " << std::hex << this << " POST request with "
          << Int64ToString(post_data_len_) << " bytes";
      CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
      HRESULT hr =
          CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
      if (FAILED(hr)) {
        NOTREACHED();
        return hr;
      }
      upload_stream->Initialize(upload_data.get());

      // Fill the STGMEDIUM with the data to post
      bind_info->stgmedData.tymed = TYMED_ISTREAM;
      bind_info->stgmedData.pstm = static_cast<IStream*>(upload_stream);
      bind_info->stgmedData.pstm->AddRef();
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
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
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
    cached_data_.Create();
  }

  // Always read data into cache. We have to read all the data here at this
  // time or it won't be available later. Since the size of the data could
  // be more than pending read size, it's not straightforward (or might even
  // be impossible) to implement a true data pull model.
  size_t bytes_available = 0;
  cached_data_.Append(read_stream, &bytes_available);
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " -  Bytes read into cache: " << bytes_available;

  if (pending_read_size_) {
    CComObjectStackEx<SendStream> send_stream;
    send_stream.Initialize(this);
    cached_data_.Read(&send_stream, pending_read_size_, &pending_read_size_);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - size read: " << pending_read_size_;
    pending_read_size_ = 0;
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
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  if (!additional_headers) {
    NOTREACHED();
    return E_POINTER;
  }

  DLOG(INFO) << "URL: " << url << " Obj: " << std::hex << this <<
      " - Request headers: \n" << current_headers;

  HRESULT hr = S_OK;

  std::string new_headers;
  if (post_data_len_ > 0) {
    // Tack on the Content-Length header since when using an IStream type
    // STGMEDIUM, it looks like it doesn't get set for us :(
    new_headers = StringPrintf("Content-Length: %s\r\n",
                               Int64ToString(post_data_len_).c_str());
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
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);

  std::string raw_headers = WideToUTF8(response_headers);

  // Security check for frame busting headers. We don't honor the headers
  // as-such, but instead simply kill requests which we've been asked to
  // look for. This puts the onus on the user of the UrlRequest to specify
  // whether or not requests should be inspected. For ActiveDocuments, the
  // answer is "no", since WebKit's detection/handling is sufficient and since
  // ActiveDocuments cannot be hosted as iframes. For NPAPI and ActiveX
  // documents, the Initialize() function of the PluginUrlRequest object
  // allows them to specify how they'd like requests handled. Both should
  // set enable_frame_busting_ to true to avoid CSRF attacks.
  // Should WebKit's handling of this ever change, we will need to re-visit
  // how and when frames are killed to better mirror a policy which may
  // do something other than kill the sub-document outright.

  // NOTE(slightlyoff): We don't use net::HttpResponseHeaders here because
  //    of lingering ICU/base_noicu issues.
  if (frame_busting_enabled_ &&
      net::HttpUtil::HasHeader(raw_headers, kXFrameOptionsHeader)) {
    DLOG(ERROR) << "X-Frame-Options header detected, navigation canceled";
    return E_FAIL;
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
  // Special case.  If the last request was a redirect and the current OS
  // error value is E_ACCESSDENIED, that means an unsafe redirect was attempted.
  // In that case, correct the OS error value to be the more specific
  // ERR_UNSAFE_REDIRECT error value.
  if (!status_.is_success() && status_.os_error() == net::ERR_ACCESS_DENIED) {
    int status = GetHttpResponseStatus();
    if (status >= 300 && status < 400) {
      redirect_status_ = status;  // store the latest redirect status value.
      status_.set_os_error(net::ERR_UNSAFE_REDIRECT);
    }
  }
  request_handler()->RemoveRequest(this);
  OnResponseEnd(status_);

  // If the request was started then we must have an additional reference on the
  // request.
  if (is_request_started_) {
    is_request_started_ = false;
    Release();
  }
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
    if (SUCCEEDED(info->QueryInfo(HTTP_QUERY_STATUS_CODE, status, &buf_size,
                                  0, NULL))) {
      http_status = StringToInt(status);
    } else {
      NOTREACHED() << "Failed to get HTTP status";
    }
  } else {
    NOTREACHED() << "failed to get IWinInetHttpInfo from binding_";
  }

  return http_status;
}

//
// UrlmonUrlRequest::Cache implementation.
//

size_t UrlmonUrlRequest::Cache::Size() {
  size_t size = 0;
  if (stream_) {
    STATSTG cache_stat = {0};
    stream_->Stat(&cache_stat, STATFLAG_NONAME);

    DCHECK_EQ(0, cache_stat.cbSize.HighPart);
    size = cache_stat.cbSize.LowPart;
  }

  return size;
}

size_t UrlmonUrlRequest::Cache::CurrentPos() {
  size_t pos = 0;
  if (stream_) {
    ULARGE_INTEGER current_index = {0};
    stream_->Seek(kZero, STREAM_SEEK_CUR, &current_index);

    DCHECK_EQ(0, current_index.HighPart);
    pos = current_index.LowPart;
  }

  return pos;
}

size_t UrlmonUrlRequest::Cache::SizeRemaining() {
  size_t size = Size();
  size_t pos = CurrentPos();
  size_t size_remaining = 0;

  if (size) {
    DCHECK(pos <= size);
    size_remaining = size - pos;
  }
  return size_remaining;
}

void UrlmonUrlRequest::Cache::Clear() {
  if (!stream_) {
    NOTREACHED();
    return;
  }

  HRESULT hr = stream_->SetSize(kUnsignedZero);
  DCHECK(SUCCEEDED(hr));
}

bool UrlmonUrlRequest::Cache::Read(IStream* dest, size_t size,
                                   size_t* bytes_copied) {
  if (!dest || !size) {
    NOTREACHED();
    return false;
  }

  // Copy the data and clear cache if there is no more data to copy.
  ULARGE_INTEGER size_to_copy = {size, 0};
  ULARGE_INTEGER size_written = {0};
  stream_->CopyTo(dest, size_to_copy, NULL, &size_written);

  if (size_written.LowPart && bytes_copied)
    *bytes_copied = size_written.LowPart;

  if (!SizeRemaining()) {
    Clear();
    stream_->Seek(kZero, STREAM_SEEK_SET, NULL);
  }

  return (size_written.LowPart != 0);
}

bool UrlmonUrlRequest::Cache::Append(IStream* source,
                                     size_t* bytes_copied) {
  if (!source) {
    NOTREACHED();
    return false;
  }

  size_t current_pos = CurrentPos();
  stream_->Seek(kZero, STREAM_SEEK_END, NULL);

  HRESULT hr = S_OK;
  while (SUCCEEDED(hr)) {
    DWORD chunk_read = 0;  // NOLINT
    hr = source->Read(read_buffer_, sizeof(read_buffer_), &chunk_read);
    if (!chunk_read)
      break;

    DWORD chunk_written = 0;  // NOLINT
    stream_->Write(read_buffer_, chunk_read, &chunk_written);
    DCHECK_EQ(chunk_read, chunk_written);

    if (bytes_copied)
      *bytes_copied += chunk_written;
  }

  LARGE_INTEGER last_read_position = {current_pos, 0};
  stream_->Seek(last_read_position, STREAM_SEEK_SET, NULL);
  return SUCCEEDED(hr);
}

bool UrlmonUrlRequest::Cache::Create() {
  DCHECK(stream_ == NULL);
  bool ret = SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, stream_.Receive()));
  DCHECK(ret && stream_);
  return ret;
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
