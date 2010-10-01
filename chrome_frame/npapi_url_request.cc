// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/npapi_url_request.h"

#include "base/string_number_conversions.h"
#include "chrome_frame/np_browser_functions.h"
#include "chrome_frame/np_utils.h"
#include "net/base/net_errors.h"

class NPAPIUrlRequest : public PluginUrlRequest {
 public:
  explicit NPAPIUrlRequest(NPP instance);
  ~NPAPIUrlRequest();

  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // Called from NPAPI
  NPError OnStreamCreated(const char* mime_type, NPStream* stream);
  NPError OnStreamDestroyed(NPReason reason);
  int OnWriteReady();
  int OnWrite(void* buffer, int len);

  // Thread unsafe implementation of ref counting, since
  // this will be called on the plugin UI thread only.
  virtual unsigned long API_CALL AddRef();
  virtual unsigned long API_CALL Release();

 private:
  unsigned long ref_count_;
  NPP instance_;
  NPStream* stream_;
  size_t pending_read_size_;
  URLRequestStatus status_;

  PlatformThreadId thread_;
  static int instance_count_;
  DISALLOW_COPY_AND_ASSIGN(NPAPIUrlRequest);
};

int NPAPIUrlRequest::instance_count_ = 0;

NPAPIUrlRequest::NPAPIUrlRequest(NPP instance)
    : ref_count_(0), instance_(instance), stream_(NULL),
      pending_read_size_(0),
      status_(URLRequestStatus::FAILED, net::ERR_FAILED),
      thread_(PlatformThread::CurrentId()) {
  DLOG(INFO) << "Created request. Count: " << ++instance_count_;
}

NPAPIUrlRequest::~NPAPIUrlRequest() {
  DLOG(INFO) << "Deleted request. Count: " << --instance_count_;
}

// NPAPIUrlRequest member defines.
bool NPAPIUrlRequest::Start() {
  NPError result = NPERR_GENERIC_ERROR;
  DLOG(INFO) << "Starting URL request: " << url();
  if (LowerCaseEqualsASCII(method(), "get")) {
    // TODO(joshia): if we have extra headers for HTTP GET, then implement
    // it using XHR
    result = npapi::GetURLNotify(instance_, url().c_str(), NULL, this);
  } else if (LowerCaseEqualsASCII(method(), "post")) {
    uint32 data_len = static_cast<uint32>(post_data_len());

    std::string buffer;
    if (extra_headers().length() > 0) {
      buffer += extra_headers();
      TrimWhitespace(buffer, TRIM_ALL, &buffer);

      // Firefox looks specifically for "Content-length: \d+\r\n\r\n"
      // to detect if extra headers are added to the message buffer.
      buffer += "\r\nContent-length: ";
      buffer += base::IntToString(data_len);
      buffer += "\r\n\r\n";
    }

    std::string data;
    data.resize(data_len);
    uint32 bytes_read;
    upload_data_->Read(&data[0], data_len,
                       reinterpret_cast<ULONG*>(&bytes_read));
    DCHECK_EQ(data_len, bytes_read);
    buffer += data;

    result = npapi::PostURLNotify(instance_, url().c_str(), NULL,
        buffer.length(), buffer.c_str(), false, this);
  } else {
    NOTREACHED() << "PluginUrlRequest only supports 'GET'/'POST'";
  }

  if (NPERR_NO_ERROR != result) {
    int os_error = net::ERR_FAILED;
    switch (result) {
      case NPERR_INVALID_URL:
        os_error = net::ERR_INVALID_URL;
        break;
      default:
        break;
    }

    delegate_->OnResponseEnd(id(),
        URLRequestStatus(URLRequestStatus::FAILED, os_error));
    return false;
  }

  return true;
}

void NPAPIUrlRequest::Stop() {
  DLOG(INFO) << "Finished request: Url - " << url() << " result: "
      << static_cast<int>(status_.status());
  if (stream_) {
    npapi::DestroyStream(instance_, stream_, NPRES_USER_BREAK);
    stream_ = NULL;
  }
}

bool NPAPIUrlRequest::Read(int bytes_to_read) {
  pending_read_size_ = bytes_to_read;
  return true;
}

NPError NPAPIUrlRequest::OnStreamCreated(const char* mime_type,
                                         NPStream* stream) {
  stream_ = stream;
  status_.set_status(URLRequestStatus::IO_PENDING);
  // TODO(iyengar)
  // Add support for passing persistent cookies and information about any URL
  // redirects to Chrome.
  delegate_->OnResponseStarted(id(), mime_type, stream->headers, stream->end,
      base::Time::FromTimeT(stream->lastmodified), std::string(), 0);
  return NPERR_NO_ERROR;
}

NPError NPAPIUrlRequest::OnStreamDestroyed(NPReason reason) {
  URLRequestStatus::Status status = URLRequestStatus::FAILED;
  switch (reason) {
    case NPRES_DONE:
      status_.set_status(URLRequestStatus::SUCCESS);
      status_.set_os_error(0);
      break;
    case NPRES_USER_BREAK:
      status_.set_status(URLRequestStatus::CANCELED);
      status_.set_os_error(net::ERR_ABORTED);
      break;
    case NPRES_NETWORK_ERR:
    default:
      status_.set_status(URLRequestStatus::FAILED);
      status_.set_os_error(net::ERR_CONNECTION_CLOSED);
      break;
  }

  delegate_->OnResponseEnd(id(), status_);
  return NPERR_NO_ERROR;
}

int NPAPIUrlRequest::OnWriteReady() {
  return pending_read_size_;
}

int NPAPIUrlRequest::OnWrite(void* buffer, int len) {
  pending_read_size_ = 0;
  std::string data(reinterpret_cast<char*>(buffer), len);
  delegate_->OnReadComplete(id(), data);
  return len;
}

STDMETHODIMP_(ULONG) NPAPIUrlRequest::AddRef() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  ++ref_count_;
  return ref_count_;
}

STDMETHODIMP_(ULONG) NPAPIUrlRequest::Release() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  unsigned long ret = --ref_count_;
  if (!ret)
    delete this;

  return ret;
}

NPAPIUrlRequestManager::NPAPIUrlRequestManager() : instance_(NULL) {
}

NPAPIUrlRequestManager::~NPAPIUrlRequestManager() {
  StopAll();
}

// PluginUrlRequestManager implementation
PluginUrlRequestManager::ThreadSafeFlags
    NPAPIUrlRequestManager::GetThreadSafeFlags() {
  return PluginUrlRequestManager::NOT_THREADSAFE;
}

void NPAPIUrlRequestManager::StartRequest(int request_id,
    const IPC::AutomationURLRequest& request_info) {
  scoped_refptr<NPAPIUrlRequest> new_request(new NPAPIUrlRequest(instance_));
  DCHECK(new_request);
  if (new_request->Initialize(this, request_id, request_info.url,
        request_info.method, request_info.referrer,
        request_info.extra_request_headers,
        request_info.upload_data,
        static_cast<ResourceType::Type>(request_info.resource_type),
        enable_frame_busting_)) {
    // Add to map.
    DCHECK(request_map_.find(request_id) == request_map_.end());
    request_map_[request_id] = new_request;
    if (new_request->Start()) {
      // Keep additional reference on request for NPSTREAM
      // This will be released in NPP_UrlNotify
      new_request->AddRef();
    }
  }
}

void NPAPIUrlRequestManager::ReadRequest(int request_id, int bytes_to_read) {
  scoped_refptr<NPAPIUrlRequest> request = LookupRequest(request_id);
  if (request)
    request->Read(bytes_to_read);
}

void NPAPIUrlRequestManager::EndRequest(int request_id) {
  scoped_refptr<NPAPIUrlRequest> request = LookupRequest(request_id);
  if (request)
    request->Stop();
}

void NPAPIUrlRequestManager::StopAll() {
  std::vector<scoped_refptr<NPAPIUrlRequest> > request_list;
  // We copy the pending requests into a temporary vector as the Stop
  // function in the request could also try to delete the request from
  // the request map and the iterator could end up being invalid.
  for (RequestMap::iterator it = request_map_.begin();
       it != request_map_.end(); ++it) {
    DCHECK(it->second != NULL);
    request_list.push_back(it->second);
  }

  for (std::vector<scoped_refptr<NPAPIUrlRequest> >::size_type index = 0;
       index < request_list.size(); ++index) {
    request_list[index]->Stop();
  }
}

void NPAPIUrlRequestManager::SetCookiesForUrl(const GURL& url,
                                              const std::string& cookie) {
  // Use the newer NPAPI way if available
  if (npapi::VersionMinor() >= NPVERS_HAS_URL_AND_AUTH_INFO) {
    npapi::SetValueForURL(instance_, NPNURLVCookie, url.spec().c_str(),
                          cookie.c_str(), cookie.length());
  } else {
    DLOG(INFO) << "Host does not support NPVERS_HAS_URL_AND_AUTH_INFO.";
    DLOG(INFO) << "Attempting to set cookie using XPCOM cookie service";
    if (np_utils::SetCookiesUsingXPCOMCookieService(instance_, url.spec(),
                                                    cookie)) {
      DLOG(INFO) << "Successfully set cookies using XPCOM cookie service";
      DLOG(INFO) << cookie.c_str();
    } else {
      NOTREACHED() << "Failed to set cookies for host";
    }
  }
}

void NPAPIUrlRequestManager::GetCookiesForUrl(const GURL& url, int cookie_id) {
  std::string cookie_string;
  bool success = true;

  if (npapi::VersionMinor() >= NPVERS_HAS_URL_AND_AUTH_INFO) {
    char* cookies = NULL;
    unsigned int cookie_length = 0;
    NPError ret = npapi::GetValueForURL(instance_, NPNURLVCookie,
                                        url.spec().c_str(), &cookies,
                                        &cookie_length);
    if (ret == NPERR_NO_ERROR) {
      DLOG(INFO) << "Obtained cookies:" << cookies << " from host";
      cookie_string.append(cookies, cookie_length);
      npapi::MemFree(cookies);
    } else {
      success = false;
    }
  } else {
    DLOG(INFO) << "Host does not support NPVERS_HAS_URL_AND_AUTH_INFO.";
    DLOG(INFO) << "Attempting to read cookie using XPCOM cookie service";
    if (np_utils::GetCookiesUsingXPCOMCookieService(instance_, url.spec(),
                                                    &cookie_string)) {
      DLOG(INFO) << "Successfully read cookies using XPCOM cookie service";
      DLOG(INFO) << cookie_string.c_str();
    } else {
      success = false;
    }
  }

  if (!success)
    DLOG(INFO) << "Failed to return cookies for url:" << url.spec().c_str();

  OnCookiesRetrieved(success, url, cookie_string, cookie_id);
}

// PluginRequestDelegate implementation.
// Callbacks from NPAPIUrlRequest. Simply forward to the delegate.
void NPAPIUrlRequestManager::OnResponseStarted(int request_id,
    const char* mime_type, const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status) {
  delegate_->OnResponseStarted(request_id, mime_type, headers, size,
      last_modified, redirect_url, redirect_status);
}

void NPAPIUrlRequestManager::OnReadComplete(int request_id,
                                            const std::string& data) {
  delegate_->OnReadComplete(request_id, data);
}

void NPAPIUrlRequestManager::OnResponseEnd(int request_id,
                                           const URLRequestStatus& status) {
  // Delete from map.
  RequestMap::iterator it = request_map_.find(request_id);
  DCHECK(request_map_.end() != it);
  scoped_refptr<NPAPIUrlRequest> request = (*it).second;
  request_map_.erase(it);

  // Inform delegate unless canceled.
  if (status.status() != URLRequestStatus::CANCELED)
    delegate_->OnResponseEnd(request_id, status);
}

void NPAPIUrlRequestManager::OnCookiesRetrieved(bool success, const GURL& url,
    const std::string& cookie_string, int cookie_id) {
  delegate_->OnCookiesRetrieved(success, url, cookie_string, cookie_id);
}

// Notifications from browser. Find the NPAPIUrlRequest and forward to it.
NPError NPAPIUrlRequestManager::NewStream(NPMIMEType type,
                                          NPStream* stream,
                                          NPBool seekable,
                                          uint16* stream_type) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  if (!request)
    return NPERR_NO_ERROR;

  DCHECK(request_map_.find(request->id()) != request_map_.end());
  // We need to return the requested stream mode if we are returning a success
  // code. If we don't do this it causes Opera to blow up.
  *stream_type = NP_NORMAL;
  return request->OnStreamCreated(type, stream);
}

int32 NPAPIUrlRequestManager::WriteReady(NPStream* stream) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  if (!request)
    return 0x7FFFFFFF;
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnWriteReady();
}

int32 NPAPIUrlRequestManager::Write(NPStream* stream, int32 offset,
                                    int32 len, void* buffer) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  if (!request)
    return len;
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnWrite(buffer, len);
}

NPError NPAPIUrlRequestManager::DestroyStream(NPStream* stream,
                                              NPReason reason) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  if (!request)
    return NPERR_NO_ERROR;
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnStreamDestroyed(reason);
}

void NPAPIUrlRequestManager::UrlNotify(const char* url, NPReason reason,
                                       void* notify_data) {
  NPAPIUrlRequest* request = RequestFromNotifyData(notify_data);
  DCHECK(request != NULL);
  if (request) {
    request->Stop();
    request->Release();
  }
}

scoped_refptr<NPAPIUrlRequest> NPAPIUrlRequestManager::LookupRequest(
    int request_id) {
  RequestMap::iterator index = request_map_.find(request_id);
  if (index != request_map_.end())
    return index->second;
  return NULL;
}
