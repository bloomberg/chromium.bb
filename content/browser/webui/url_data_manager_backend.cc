// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/histogram_internals_request_job.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/tcmalloc_internals_request_job.h"
#include "content/browser/webui/shared_resources_data_source.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/appcache/view_appcache_internals_job.h"

using appcache::AppCacheService;

namespace content {

namespace {

// TODO(tsepez) remove unsafe-eval when bidichecker_packaged.js fixed.
const char kChromeURLContentSecurityPolicyHeaderBase[] =
    "Content-Security-Policy: script-src chrome://resources "
    "'self' 'unsafe-eval'; ";

const char kChromeURLXFrameOptionsHeader[] = "X-Frame-Options: DENY";

bool SchemeIsInSchemes(const std::string& scheme,
                       const std::vector<std::string>& schemes) {
  return std::find(schemes.begin(), schemes.end(), scheme) != schemes.end();
}

// Parse a URL into the components used to resolve its request. |source_name|
// is the hostname and |path| is the remaining portion of the URL.
void URLToRequest(const GURL& url, std::string* source_name,
                  std::string* path) {
  DCHECK(url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         SchemeIsInSchemes(
             url.scheme(),
             GetContentClient()->browser()->GetAdditionalWebUISchemes()));

  if (!url.is_valid()) {
    NOTREACHED();
    return;
  }

  // Our input looks like: chrome://source_name/extra_bits?foo .
  // So the url's "host" is our source, and everything after the host is
  // the path.
  source_name->assign(url.host());

  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url_parse::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

}  // namespace

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob,
                            public base::SupportsWeakPtr<URLRequestChromeJob> {
 public:
  // |is_incognito| set when job is generated from an incognito profile.
  URLRequestChromeJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      URLDataManagerBackend* backend,
                      bool is_incognito);

  // net::URLRequestJob implementation.
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;

  // Used to notify that the requested data's |mime_type| is ready.
  void MimeTypeAvailable(const std::string& mime_type);

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.
  void DataAvailable(base::RefCountedMemory* bytes);

  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

  void set_allow_caching(bool allow_caching) {
    allow_caching_ = allow_caching;
  }

  void set_add_content_security_policy(bool add_content_security_policy) {
    add_content_security_policy_ = add_content_security_policy;
  }

  void set_content_security_policy_object_source(
      const std::string& data) {
    content_security_policy_object_source_ = data;
  }

  void set_content_security_policy_frame_source(
      const std::string& data) {
    content_security_policy_frame_source_ = data;
  }

  void set_deny_xframe_options(bool deny_xframe_options) {
    deny_xframe_options_ = deny_xframe_options;
  }

  // Returns true when job was generated from an incognito profile.
  bool is_incognito() const {
    return is_incognito_;
  }

 private:
  virtual ~URLRequestChromeJob();

  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most net::URLRequestJob implementations.)
  void StartAsync();

  // Do the actual copy from data_ (the data we're serving) into |buf|.
  // Separate from ReadRawData so we can handle async I/O.
  void CompleteRead(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<base::RefCountedMemory> data_;
  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // If true, set a header in the response to prevent it from being cached.
  bool allow_caching_;

  // If true, set the Content Security Policy (CSP) header.
  bool add_content_security_policy_;

  // These are used with the CSP.
  std::string content_security_policy_object_source_;
  std::string content_security_policy_frame_source_;

  // If true, sets  the "X-Frame-Options: DENY" header.
  bool deny_xframe_options_;

  // True when job is generated from an incognito profile.
  const bool is_incognito_;

  // The backend is owned by ChromeURLRequestContext and always outlives us.
  URLDataManagerBackend* backend_;

  base::WeakPtrFactory<URLRequestChromeJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeJob);
};

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         URLDataManagerBackend* backend,
                                         bool is_incognito)
    : net::URLRequestJob(request, network_delegate),
      data_offset_(0),
      pending_buf_size_(0),
      allow_caching_(true),
      add_content_security_policy_(true),
      content_security_policy_object_source_("object-src 'none';"),
      content_security_policy_frame_source_("frame-src 'none';"),
      deny_xframe_options_(true),
      is_incognito_(is_incognito),
      backend_(backend),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(backend);
}

URLRequestChromeJob::~URLRequestChromeJob() {
  CHECK(!backend_->HasPendingJob(this));
}

void URLRequestChromeJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestChromeJob::StartAsync,
                 weak_factory_.GetWeakPtr()));

  TRACE_EVENT_ASYNC_BEGIN1("browser", "DataManager:Request", this, "URL",
      request_->url().possibly_invalid_spec());
}

void URLRequestChromeJob::Kill() {
  backend_->RemoveRequest(this);
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

int URLRequestChromeJob::GetResponseCode() const {
  return net::HTTP_OK;
}

void URLRequestChromeJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(!info->headers);
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");

  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (add_content_security_policy_) {
    std::string base = kChromeURLContentSecurityPolicyHeaderBase;
    base.append(content_security_policy_object_source_);
    base.append(content_security_policy_frame_source_);
    info->headers->AddHeader(base);
  }

  if (deny_xframe_options_)
    info->headers->AddHeader(kChromeURLXFrameOptionsHeader);

  if (!allow_caching_)
    info->headers->AddHeader("Cache-Control: no-cache");
}

void URLRequestChromeJob::MimeTypeAvailable(const std::string& mime_type) {
  set_mime_type(mime_type);
  NotifyHeadersComplete();
}

void URLRequestChromeJob::DataAvailable(base::RefCountedMemory* bytes) {
  TRACE_EVENT_ASYNC_END0("browser", "DataManager:Request", this);
  if (bytes) {
    // The request completed, and we have all the data.
    // Clear any IO pending status.
    SetStatus(net::URLRequestStatus());

    data_ = bytes;
    int bytes_read;
    if (pending_buf_.get()) {
      CHECK(pending_buf_->data());
      CompleteRead(pending_buf_, pending_buf_size_, &bytes_read);
      pending_buf_ = NULL;
      NotifyReadComplete(bytes_read);
    }
  } else {
    // The request failed.
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_FAILED));
  }
}

bool URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  if (!data_.get()) {
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    DCHECK(!pending_buf_.get());
    CHECK(buf->data());
    pending_buf_ = buf;
    pending_buf_size_ = buf_size;
    return false;  // Tell the caller we're still waiting for data.
  }

  // Otherwise, the data is available.
  CompleteRead(buf, buf_size, bytes_read);
  return true;
}

void URLRequestChromeJob::CompleteRead(net::IOBuffer* buf, int buf_size,
                                       int* bytes_read) {
  int remaining = static_cast<int>(data_->size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), data_->front() + data_offset_, buf_size);
    data_offset_ += buf_size;
  }
  *bytes_read = buf_size;
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (!backend_->StartRequest(request_, this)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
  }
}

namespace {

// Gets mime type for data that is available from |source| by |path|.
// After that, notifies |job| that mime type is available. This method
// should be called on the UI thread, but notification is performed on
// the IO thread.
void GetMimeTypeOnUI(URLDataSourceImpl* source,
                     const std::string& path,
                     const base::WeakPtr<URLRequestChromeJob>& job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string mime_type = source->source()->GetMimeType(path);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&URLRequestChromeJob::MimeTypeAvailable, job, mime_type));
}

}  // namespace

namespace {

class ChromeProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  ChromeProtocolHandler(ResourceContext* resource_context,
                        bool is_incognito,
                        AppCacheService* appcache_service,
                        ChromeBlobStorageContext* blob_storage_context)
      : resource_context_(resource_context),
        is_incognito_(is_incognito),
        appcache_service_(appcache_service),
        blob_storage_context_(blob_storage_context) {}
  virtual ~ChromeProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    DCHECK(request);

    // Check for chrome://view-http-cache/*, which uses its own job type.
    if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
      return ViewHttpCacheJobFactory::CreateJobForRequest(request,
                                                          network_delegate);

    // Next check for chrome://appcache-internals/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIAppCacheInternalsHost) {
      return appcache::ViewAppCacheInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, appcache_service_);
    }

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, blob_storage_context_->controller());
    }

#if defined(USE_TCMALLOC)
    // Next check for chrome://tcmalloc/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUITcmallocHost) {
      return new TcmallocInternalsRequestJob(request, network_delegate);
    }
#endif

    // Next check for chrome://histograms/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIHistogramHost) {
      return new HistogramInternalsRequestJob(request, network_delegate);
    }

    // Fall back to using a custom handler
    return new URLRequestChromeJob(
        request, network_delegate,
        GetURLDataManagerForResourceContext(resource_context_), is_incognito_);
  }

 private:
  // These members are owned by ProfileIOData, which owns this ProtocolHandler.
  content::ResourceContext* const resource_context_;

  // True when generated from an incognito profile.
  const bool is_incognito_;
  AppCacheService* appcache_service_;
  ChromeBlobStorageContext* blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProtocolHandler);
};

}  // namespace

URLDataManagerBackend::URLDataManagerBackend()
    : next_request_id_(0) {
  URLDataSource* shared_source = new SharedResourcesDataSource();
  URLDataSourceImpl* source_impl =
      new URLDataSourceImpl(shared_source->GetSource(), shared_source);
  AddDataSource(source_impl);
}

URLDataManagerBackend::~URLDataManagerBackend() {
  for (DataSourceMap::iterator i = data_sources_.begin();
       i != data_sources_.end(); ++i) {
    i->second->backend_ = NULL;
  }
  data_sources_.clear();
}

// static
net::URLRequestJobFactory::ProtocolHandler*
URLDataManagerBackend::CreateProtocolHandler(
    content::ResourceContext* resource_context,
    bool is_incognito,
    AppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(resource_context);
  return new ChromeProtocolHandler(
      resource_context, is_incognito, appcache_service, blob_storage_context);
}

void URLDataManagerBackend::AddDataSource(
    URLDataSourceImpl* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DataSourceMap::iterator i = data_sources_.find(source->source_name());
  if (i != data_sources_.end()) {
    if (!source->source()->ShouldReplaceExistingSource())
      return;
    i->second->backend_ = NULL;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = this;
}

bool URLDataManagerBackend::HasPendingJob(
    URLRequestChromeJob* job) const {
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool URLDataManagerBackend::StartRequest(const net::URLRequest* request,
                                         URLRequestChromeJob* job) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string path;
  URLToRequest(request->url(), &source_name, &path);

  // Look up the data source for the request.
  DataSourceMap::iterator i = data_sources_.find(source_name);
  if (i == data_sources_.end())
    return false;

  URLDataSourceImpl* source = i->second;

  if (!source->source()->ShouldServiceRequest(request))
    return false;

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  job->set_allow_caching(source->source()->AllowCaching());
  job->set_add_content_security_policy(
      source->source()->ShouldAddContentSecurityPolicy());
  job->set_content_security_policy_object_source(
      source->source()->GetContentSecurityPolicyObjectSrc());
  job->set_content_security_policy_frame_source(
      source->source()->GetContentSecurityPolicyFrameSrc());
  job->set_deny_xframe_options(
      source->source()->ShouldDenyXFrameOptions());

  // Forward along the request to the data source.
  MessageLoop* target_message_loop =
      source->source()->MessageLoopForRequestPath(path);
  if (!target_message_loop) {
    bool is_incognito = job->is_incognito();
    job->MimeTypeAvailable(source->source()->GetMimeType(path));
    // Eliminate potentially dangling pointer to avoid future use.
    job = NULL;

    // The DataSource is agnostic to which thread StartDataRequest is called
    // on for this path.  Call directly into it from this thread, the IO
    // thread.
    source->source()->StartDataRequest(
        path, is_incognito,
        base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
  } else {
    // URLRequestChromeJob should receive mime type before data. This
    // is guaranteed because request for mime type is placed in the
    // message loop before request for data. And correspondingly their
    // replies are put on the IO thread in the same order.
    target_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&GetMimeTypeOnUI,
                   scoped_refptr<URLDataSourceImpl>(source),
                   path, job->AsWeakPtr()));

    // The DataSource wants StartDataRequest to be called on a specific thread,
    // usually the UI thread, for this path.
    target_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&URLDataManagerBackend::CallStartRequest,
                   make_scoped_refptr(source), path, job->is_incognito(),
                   request_id));
  }
  return true;
}

void URLDataManagerBackend::CallStartRequest(
    scoped_refptr<URLDataSourceImpl> source,
    const std::string& path,
    bool is_incognito,
    int request_id) {
  source->source()->StartDataRequest(
      path,
      is_incognito,
      base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
}

void URLDataManagerBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void URLDataManagerBackend::DataAvailable(RequestID request_id,
                                          base::RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job(i->second);
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

namespace {

class DevToolsJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  DevToolsJobFactory(content::ResourceContext* resource_context,
                     bool is_incognito);
  virtual ~DevToolsJobFactory();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // |resource_context_| and |network_delegate_| are owned by ProfileIOData,
  // which owns this ProtocolHandler.
  content::ResourceContext* const resource_context_;

  // True when generated from an incognito profile.
  const bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsJobFactory);
};

DevToolsJobFactory::DevToolsJobFactory(
    content::ResourceContext* resource_context,
    bool is_incognito)
    : resource_context_(resource_context),
      is_incognito_(is_incognito) {
  DCHECK(resource_context_);
}

DevToolsJobFactory::~DevToolsJobFactory() {}

net::URLRequestJob*
DevToolsJobFactory::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new URLRequestChromeJob(
      request, network_delegate,
      GetURLDataManagerForResourceContext(resource_context_), is_incognito_);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateDevToolsProtocolHandler(content::ResourceContext* resource_context,
                              bool is_incognito) {
  return new DevToolsJobFactory(resource_context, is_incognito);
}

}  // namespace content
