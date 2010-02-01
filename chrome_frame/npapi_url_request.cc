// Copyright 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome_frame/npapi_url_request.h"

#include "base/string_util.h"
#include "chrome_frame/np_browser_functions.h"
#include "net/base/net_errors.h"

class NPAPIUrlRequest : public PluginUrlRequest {
 public:
  explicit NPAPIUrlRequest(NPP instance);
  ~NPAPIUrlRequest();

  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // Called from NPAPI
  bool OnStreamCreated(const char* mime_type, NPStream* stream);
  NPError OnStreamDestroyed(NPReason reason);
  int OnWriteReady();
  int OnWrite(void* buffer, int len);

  // Thread unsafe implementation of ref counting, since
  // this will be called on the plugin UI thread only.
  virtual unsigned long API_CALL AddRef();
  virtual unsigned long API_CALL Release();

 private:
  PluginUrlRequestDelegate* delegate_;
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
    result = npapi::PostURLNotify(instance_, url().c_str(), NULL,
        extra_headers().length(), extra_headers().c_str(), false, this);
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

bool NPAPIUrlRequest::OnStreamCreated(const char* mime_type, NPStream* stream) {
  stream_ = stream;
  status_.set_status(URLRequestStatus::IO_PENDING);
  // TODO(iyengar)
  // Add support for passing persistent cookies and information about any URL
  // redirects to Chrome.
  delegate_->OnResponseStarted(id(), mime_type, stream->headers, stream->end,
      base::Time::FromTimeT(stream->lastmodified), std::string(),
      std::string(), 0);
  return true;
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
  delegate_->OnReadComplete(id(), buffer, len);
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
bool NPAPIUrlRequestManager::IsThreadSafe() {
  return false;
}

void NPAPIUrlRequestManager::StartRequest(int request_id,
    const IPC::AutomationURLRequest& request_info) {
  scoped_refptr<NPAPIUrlRequest> new_request(new NPAPIUrlRequest(instance_));
  DCHECK(new_request);
  if (new_request->Initialize(this, request_id, request_info.url,
        request_info.method, request_info.referrer,
        request_info.extra_request_headers, request_info.upload_data.get(),
        enable_frame_busting_)) {
    // Add to map.
    DCHECK(NULL == request_map_[request_id].get());
    request_map_[request_id] = new_request;
    if (new_request->Start()) {
      // Keep additional reference on request for NPSTREAM
      // This will be released in NPP_UrlNotify
      new_request->AddRef();
    }
  }
}

void NPAPIUrlRequestManager::ReadRequest(int request_id, int bytes_to_read) {
  scoped_refptr<NPAPIUrlRequest> request = request_map_[request_id];
  DCHECK(request.get());
  if (request)
    request->Read(bytes_to_read);
}

void NPAPIUrlRequestManager::EndRequest(int request_id) {
  scoped_refptr<NPAPIUrlRequest> request = request_map_[request_id];
  if (request)
    request->Stop();
}

void NPAPIUrlRequestManager::StopAll() {
  for (RequestMap::iterator index = request_map_.begin();
       index != request_map_.end();
       ++index) {
    scoped_refptr<NPAPIUrlRequest> request = (*index).second;
    request->Stop();
  }
}

// PluginRequestDelegate implementation.
// Callbacks from NPAPIUrlRequest. Simply forward to the delegate.
void NPAPIUrlRequestManager::OnResponseStarted(int request_id,
    const char* mime_type, const char* headers, int size,
    base::Time last_modified, const std::string& peristent_cookies,
    const std::string& redirect_url, int redirect_status) {
  delegate_->OnResponseStarted(request_id, mime_type, headers, size,
      last_modified, peristent_cookies, redirect_url, redirect_status);
}

void NPAPIUrlRequestManager::OnReadComplete(int request_id, const void* buffer,
                                            int len) {
  delegate_->OnReadComplete(request_id, buffer, len);
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

// Notifications from browser. Find the NPAPIUrlRequest and forward to it.
bool NPAPIUrlRequestManager::NewStream(NPMIMEType type, NPStream* stream,
                                       NPBool seekable, uint16* stream_type) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  // We need to return the requested stream mode if we are returning a success
  // code. If we don't do this it causes Opera to blow up.
  *stream_type = NP_NORMAL;
  return request->OnStreamCreated(type, stream);
}

int32 NPAPIUrlRequestManager::WriteReady(NPStream* stream) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnWriteReady();
}

int32 NPAPIUrlRequestManager::Write(NPStream* stream, int32 offset,
                                    int32 len, void* buffer) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnWrite(buffer, len);
}

NPError NPAPIUrlRequestManager::DestroyStream(NPStream* stream,
                                              NPReason reason) {
  NPAPIUrlRequest* request = RequestFromNotifyData(stream->notifyData);
  DCHECK(request_map_.find(request->id()) != request_map_.end());
  return request->OnStreamDestroyed(reason);
}

void NPAPIUrlRequestManager::UrlNotify(const char* url, NPReason reason,
                                       void* notify_data) {
  NPAPIUrlRequest* request = RequestFromNotifyData(notify_data);
  if (request) {
    request->Stop();
    request->Release();
  }
}
