// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/libcros_service_library.h"

#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "cros/chromeos_libcros_service.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

class LibCrosServiceLibraryImpl : public LibCrosServiceLibrary {
 public:
  // Base class for all services of LibCrosService.
  // Each subclass should declare DISABLE_RUNNABLE_METHOD_REFCOUNT to disable
  // refcounting, which is okay since the subclass's object will exist as a
  // scoped_ptr in Singleton LibCrosServiceLibraryImpl, guaranteeing that its
  // lifetime is longer than that of any message loop.
  class ServicingLibrary {
   public:
    explicit ServicingLibrary(LibCrosServiceConnection service_connection);
    virtual ~ServicingLibrary();

    // Clears service_connection_ (which is stored as weak pointer) so that it
    // can't be used anymore.
    virtual void ClearServiceConnection();

   protected:
    LibCrosServiceConnection service_connection_;  // Weak pointer.
    // Lock for data members to synchronize access on multiple threads.
    base::Lock data_lock_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ServicingLibrary);
  };

  // Library that provides network proxy service for LibCrosService.
  // For now, it only processes proxy resolution requests for ChromeOS clients.
  class NetworkProxyLibrary : public ServicingLibrary {
   public:
    explicit NetworkProxyLibrary(LibCrosServiceConnection connection);
    virtual ~NetworkProxyLibrary();

   private:
    // Data being used in one proxy resolution.
    class Request {
     public:
      explicit Request(const std::string& source_url);
      virtual ~Request() {}

      // Callback on IO thread for when net::ProxyService::ResolveProxy
      // completes, synchronously or asynchronously.
      void OnCompletion(int result);
      net::CompletionCallbackImpl<Request> completion_callback_;

      std::string source_url_;  // URL being resolved.
      int result_;  // Result of proxy resolution.
      net::ProxyInfo proxy_info_;  // ProxyInfo resolved for source_url_.
      std::string error_;  // Error from proxy resolution.
      Task* notify_task_;  // Task to notify of resolution result.

     private:
      DISALLOW_COPY_AND_ASSIGN(Request);
    };

    // Static callback passed to LibCrosService to be invoked when ChromeOS
    // clients send network proxy resolution requests to the service running in
    // chrome executable.  Called on UI thread from dbus request.
    static void ResolveProxyHandler(void* object, const char* source_url);

    void ResolveProxy(const std::string& source_url);

    // Wrapper on UI thread to call LibCrosService::NotifyNetworkProxyResolved.
    void NotifyProxyResolved(Request* request);

    std::vector<Request*> all_requests_;

    DISALLOW_COPY_AND_ASSIGN(NetworkProxyLibrary);
  };

  LibCrosServiceLibraryImpl();
  virtual ~LibCrosServiceLibraryImpl();

  // LibCrosServiceLibrary implementation.

  // Starts LibCrosService running on dbus if not already started.
  virtual void StartService();

 private:
  // Connection to LibCrosService.
  LibCrosServiceConnection service_connection_;

  // Libraries that form LibCrosService.
  scoped_ptr<NetworkProxyLibrary> network_proxy_lib_;

  DISALLOW_COPY_AND_ASSIGN(LibCrosServiceLibraryImpl);
};

//---------------- LibCrosServiceLibraryImpl: public ---------------------------

LibCrosServiceLibraryImpl::LibCrosServiceLibraryImpl()
    : service_connection_(NULL) {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    LOG(ERROR) << "Cros library has not been loaded.";
  }
}

LibCrosServiceLibraryImpl::~LibCrosServiceLibraryImpl() {
  if (service_connection_) {
    // Clear service connections in servicing libraries which held the former
    // as weak pointers.
    if (network_proxy_lib_.get())
      network_proxy_lib_->ClearServiceConnection();

    StopLibCrosService(service_connection_);
    VLOG(1) << "LibCrosService stopped.";
    service_connection_ = NULL;
  }
}

// Called on UI thread to start service for LibCrosService.
void LibCrosServiceLibraryImpl::StartService() {
  // Make sure we're running on UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &LibCrosServiceLibraryImpl::StartService));
    return;
  }
  if (service_connection_)  // Service has already been started.
    return;
  // Starts LibCrosService; the returned connection is used for future
  // interactions with the service.
  service_connection_ = StartLibCrosService();
  if (!service_connection_) {
    LOG(WARNING) << "Error starting LibCrosService";
    return;
  }
  network_proxy_lib_.reset(new NetworkProxyLibrary(service_connection_));
  VLOG(1) << "LibCrosService started.";
}

//------------- LibCrosServiceLibraryImpl::ServicingLibrary: public ------------

LibCrosServiceLibraryImpl::ServicingLibrary::ServicingLibrary(
    LibCrosServiceConnection connection)
    : service_connection_(connection) {
}

LibCrosServiceLibraryImpl::ServicingLibrary::~ServicingLibrary() {
  ClearServiceConnection();
}

void LibCrosServiceLibraryImpl::ServicingLibrary::ClearServiceConnection() {
  base::AutoLock lock(data_lock_);
  service_connection_ = NULL;
}

//----------- LibCrosServiceLibraryImpl::NetworkProxyLibrary: public -----------

LibCrosServiceLibraryImpl::NetworkProxyLibrary::NetworkProxyLibrary(
    LibCrosServiceConnection connection)
    : ServicingLibrary(connection) {
  // Register callback for LibCrosService::ResolveNetworkProxy.
  SetNetworkProxyResolver(&ResolveProxyHandler, this, service_connection_);
}

LibCrosServiceLibraryImpl::NetworkProxyLibrary::~NetworkProxyLibrary() {
  base::AutoLock lock(data_lock_);
  if (!all_requests_.empty()) {
    for (size_t i = all_requests_.size() - 1;  i >= 0; --i) {
      LOG(WARNING) << "Pending request for " << all_requests_[i]->source_url_;
      delete all_requests_[i];
    }
    all_requests_.clear();
  }
}

//----------- LibCrosServiceLibraryImpl::NetworkProxyLibrary: private ----------

// Static, called on UI thread from LibCrosService::ResolveProxy via dbus.
void LibCrosServiceLibraryImpl::NetworkProxyLibrary::ResolveProxyHandler(
    void* object, const char* source_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NetworkProxyLibrary* lib = static_cast<NetworkProxyLibrary*>(object);
  // source_url will be freed when this function returns, so make a copy of it.
  std::string url(source_url);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(lib, &NetworkProxyLibrary::ResolveProxy, url));
}

// Called on IO thread as task posted from ResolveProxyHandler on UI thread.
void LibCrosServiceLibraryImpl::NetworkProxyLibrary::ResolveProxy(
    const std::string& source_url) {
  // Make sure we're running on IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Create a request slot for this proxy resolution request.
  Request* request = new Request(source_url);
  request->notify_task_ = NewRunnableMethod(this,
      &NetworkProxyLibrary::NotifyProxyResolved, request);

  // Retrieve ProxyService from profile's request context.
  net::URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  net::ProxyService* proxy_service = NULL;
  if (getter)
    proxy_service = getter->GetURLRequestContext()->proxy_service();

  // Check that we have valid proxy service and service_connection.
  if (!proxy_service) {
     request->error_ = "No proxy service in chrome";
  } else {
    base::AutoLock lock(data_lock_);
    // Queue request slot.
    all_requests_.push_back(request);
    if (!service_connection_)
      request->error_ = "LibCrosService not started";
  }
  if (request->error_ != "") {  // Error string was just set.
    LOG(ERROR) << request->error_;
    request->result_ = net::OK;  // Set to OK since error string is set.
  } else {
    VLOG(1) << "Starting networy proxy resolution for " << request->source_url_;
    request->result_ = proxy_service->ResolveProxy(
        GURL(request->source_url_), &request->proxy_info_,
        &request->completion_callback_, NULL, net::BoundNetLog());
  }
  if (request->result_ != net::ERR_IO_PENDING) {
    VLOG(1) << "Network proxy resolution completed synchronously.";
    request->OnCompletion(request->result_);
  }
}

// Called on UI thread as task posted from Request::OnCompletion on IO thread.
void LibCrosServiceLibraryImpl::NetworkProxyLibrary::NotifyProxyResolved(
    Request* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(data_lock_);
  if (service_connection_) {
    if (!NotifyNetworkProxyResolved(request->source_url_.c_str(),
                                    request->proxy_info_.ToPacString().c_str(),
                                    request->error_.c_str(),
                                    service_connection_)) {
      LOG(ERROR) << "LibCrosService has error with NotifyNetworkProxyResolved";
    } else {
      VLOG(1) << "LibCrosService has notified proxy resoloution for "
              << request->source_url_;
    }
  }
  std::vector<Request*>::iterator iter =
      std::find(all_requests_.begin(), all_requests_.end(), request);
  DCHECK(iter != all_requests_.end());
  all_requests_.erase(iter);
  delete request;
}

//---------- LibCrosServiceLibraryImpl::NetworkProxyLibrary::Request -----------

LibCrosServiceLibraryImpl::NetworkProxyLibrary::Request::Request(
    const std::string& source_url)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          completion_callback_(this, &Request::OnCompletion)),
      source_url_(source_url),
      result_(net::ERR_FAILED),
      notify_task_(NULL) {
}

// Called on IO thread when net::ProxyService::ResolveProxy has completed.
void LibCrosServiceLibraryImpl::NetworkProxyLibrary::Request::OnCompletion(
    int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  result_ = result;
  if (result_ != net::OK)
    error_ = net::ErrorToString(result_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, notify_task_);
  notify_task_ = NULL;
}

//--------------------- LibCrosServiceLibraryStubImpl --------------------------

class LibCrosServiceLibraryStubImpl : public LibCrosServiceLibrary {
 public:
  LibCrosServiceLibraryStubImpl() {}
  virtual ~LibCrosServiceLibraryStubImpl() {}

  // LibCrosServiceLibrary overrides.
  virtual void StartService() {}

  DISALLOW_COPY_AND_ASSIGN(LibCrosServiceLibraryStubImpl);
};

//--------------------------- LibCrosServiceLibrary ----------------------------

// Static.
LibCrosServiceLibrary* LibCrosServiceLibrary::GetImpl(bool stub) {
  if (stub)
    return new LibCrosServiceLibraryStubImpl();
  return new LibCrosServiceLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run, so are all its
// scoped_ptred class members.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::LibCrosServiceLibraryImpl);
DISABLE_RUNNABLE_METHOD_REFCOUNT(
    chromeos::LibCrosServiceLibraryImpl::NetworkProxyLibrary);

