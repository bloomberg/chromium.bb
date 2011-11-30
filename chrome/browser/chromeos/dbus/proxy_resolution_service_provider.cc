// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/exported_object.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

// The ProxyResolverInterface implementation used in production.
class ProxyResolverImpl : public ProxyResolverInterface {
 public:
  // Data being used in one proxy resolution.
  class Request {
   public:
    explicit Request(const std::string& source_url)
        : ALLOW_THIS_IN_INITIALIZER_LIST(
            completion_callback_(this, &Request::OnCompletion)),
          source_url_(source_url) {
    }

    virtual ~Request() {}

    // Callback on IO thread for when net::ProxyService::ResolveProxy
    // completes, synchronously or asynchronously.
    void OnCompletion(int result) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
      // Generate the error message if the error message is not yet set,
      // and there was an error.
      if (error_.empty() && result != net::OK)
        error_ = net::ErrorToString(result);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, notify_task_);
    }

    net::OldCompletionCallbackImpl<Request> completion_callback_;

    std::string source_url_;  // URL being resolved.
    net::ProxyInfo proxy_info_;  // ProxyInfo resolved for source_url_.
    std::string error_;  // Error from proxy resolution.
    base::Closure notify_task_;  // Task to notify of resolution result.

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  ProxyResolverImpl()
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        weak_ptr_factory_(this) {
  }

  virtual ~ProxyResolverImpl() {
    DCHECK(OnOriginThread());

    for (std::set<Request*>::iterator iter = all_requests_.begin();
         iter != all_requests_.end(); ++iter) {
      Request* request = *iter;
      LOG(WARNING) << "Pending request for " << request->source_url_;
      delete request;
    }
  }

  // ProxyResolverInterface override.
  virtual void ResolveProxy(
      const std::string& source_url,
      const std::string& signal_interface,
      const std::string& signal_name,
      scoped_refptr<dbus::ExportedObject> exported_object) {
    DCHECK(OnOriginThread());

    // Create a request slot for this proxy resolution request.
    Request* request = new Request(source_url);
    request->notify_task_ = base::Bind(
        &ProxyResolverImpl::NotifyProxyResolved,
        weak_ptr_factory_.GetWeakPtr(),
        signal_interface,
        signal_name,
        exported_object,
        request);
    all_requests_.insert(request);

    // GetDefaultProfile() and GetRequestContext() must be called on UI
    // thread.
    Profile* profile = ProfileManager::GetDefaultProfile();
    scoped_refptr<net::URLRequestContextGetter> getter =
        profile->GetRequestContext();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ProxyResolverImpl::ResolveProxyInternal,
                   request,
                   getter,
                   exported_object));
  }

 private:
  // Helper function for ResolveProxy().
  static void ResolveProxyInternal(
      Request* request,
      scoped_refptr<net::URLRequestContextGetter> getter,
      scoped_refptr<dbus::ExportedObject> exported_object) {
    // Make sure we're running on IO thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // Check if we have the URLRequestContextGetter.
    if (!getter) {
      request->error_ = "No URLRequestContextGetter";
      request->OnCompletion(net::ERR_UNEXPECTED);
      return;
    }

    // Retrieve ProxyService from profile's request context.
    net::ProxyService* proxy_service =
        getter->GetURLRequestContext()->proxy_service();
    if (!proxy_service) {
      request->error_ = "No proxy service in chrome";
      request->OnCompletion(net::ERR_UNEXPECTED);
      return;
    }

    VLOG(1) << "Starting network proxy resolution for "
            << request->source_url_;
    const int result = proxy_service->ResolveProxy(
        GURL(request->source_url_), &request->proxy_info_,
        &request->completion_callback_, NULL, net::BoundNetLog());
    if (result != net::ERR_IO_PENDING) {
      VLOG(1) << "Network proxy resolution completed synchronously.";
      request->OnCompletion(result);
    }
  }

  // Called on UI thread as task posted from Request::OnCompletion on IO
  // thread.
  void NotifyProxyResolved(
      const std::string& signal_interface,
      const std::string& signal_name,
      scoped_refptr<dbus::ExportedObject> exported_object,
      Request* request) {
    DCHECK(OnOriginThread());

    // Send a signal to the client.
    dbus::Signal signal(signal_interface, signal_name);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(request->source_url_);
    writer.AppendString(request->proxy_info_.ToPacString());
    writer.AppendString(request->error_);
    exported_object->SendSignal(&signal);
    VLOG(1) << "Sending signal: " << signal.ToString();

    std::set<Request*>::iterator iter = all_requests_.find(request);
    if (iter == all_requests_.end()) {
      LOG(ERROR) << "can't find request slot(" << request->source_url_
                 << ") in proxy-resolution queue";
    } else {
      all_requests_.erase(iter);
    }
    delete request;
  }

  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  base::PlatformThreadId origin_thread_id_;
  std::set<Request*> all_requests_;
  base::WeakPtrFactory<ProxyResolverImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverImpl);
};

ProxyResolutionServiceProvider::ProxyResolutionServiceProvider(
    ProxyResolverInterface* resolver)
    : resolver_(resolver),
      origin_thread_id_(base::PlatformThread::CurrentId()),
      weak_ptr_factory_(this) {
}

ProxyResolutionServiceProvider::~ProxyResolutionServiceProvider() {
}

void ProxyResolutionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(OnOriginThread());
  exported_object_ = exported_object;
  VLOG(1) << "ProxyResolutionServiceProvider started";
  exported_object_->ExportMethod(
      kLibCrosServiceInterface,
      kResolveNetworkProxy,
      // Weak pointers can only bind to methods without return values,
      // hence we cannot bind ResolveProxyInternal here. Instead we use a
      // static function to solve this problem.
      base::Bind(&ProxyResolutionServiceProvider::CallResolveProxyHandler,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyResolutionServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProxyResolutionServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
  VLOG(1) << "Method exported: " << interface_name << "." << method_name;
}

bool ProxyResolutionServiceProvider::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void ProxyResolutionServiceProvider::ResolveProxyHandler(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(OnOriginThread());
  VLOG(1) << "Handing method call: " << method_call->ToString();
  // The method call should contain the three string parameters.
  dbus::MessageReader reader(method_call);
  std::string source_url;
  std::string signal_interface;
  std::string signal_name;
  if (!reader.PopString(&source_url) ||
      !reader.PopString(&signal_interface) ||
      !reader.PopString(&signal_name)) {
    LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
    response_sender.Run(NULL);
    return;
  }

  resolver_->ResolveProxy(source_url,
                          signal_interface,
                          signal_name,
                          exported_object_);

  // Send an empty response for now. We'll send a signal once the network proxy
  // resolution is completed.
  dbus::Response* response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(response);
}

// static
void ProxyResolutionServiceProvider::CallResolveProxyHandler(
    base::WeakPtr<ProxyResolutionServiceProvider> provider_weak_ptr,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!provider_weak_ptr) {
    LOG(WARNING) << "Called after the object is deleted";
    response_sender.Run(NULL);
    return;
  }
  provider_weak_ptr->ResolveProxyHandler(method_call, response_sender);
}

ProxyResolutionServiceProvider* ProxyResolutionServiceProvider::Create() {
  return new ProxyResolutionServiceProvider(new ProxyResolverImpl);
}

ProxyResolutionServiceProvider*
ProxyResolutionServiceProvider::CreateForTesting(
    ProxyResolverInterface* resolver) {
  return new ProxyResolutionServiceProvider(resolver);
}

ProxyResolverInterface::~ProxyResolverInterface() {
}

}  // namespace chromeos
