// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/proxy_resolution_service_provider.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The ProxyResolverInterface implementation used in production.
class ProxyResolverImpl : public ProxyResolverInterface {
 public:
  // Data being used in one proxy resolution.
  class Request {
   public:
    explicit Request(const std::string& source_url) : source_url_(source_url) {
    }

    virtual ~Request() {}

    // Callback on IO thread for when net::ProxyService::ResolveProxy
    // completes, synchronously or asynchronously.
    void OnCompletion(scoped_refptr<base::SingleThreadTaskRunner> origin_thread,
                      int result) {
      // Generate the error message if the error message is not yet set,
      // and there was an error.
      if (error_.empty() && result != net::OK)
        error_ = net::ErrorToString(result);
      origin_thread->PostTask(FROM_HERE, notify_task_);
    }

    std::string source_url_;  // URL being resolved.
    net::ProxyInfo proxy_info_;  // ProxyInfo resolved for source_url_.
    std::string error_;  // Error from proxy resolution.
    base::Closure notify_task_;  // Task to notify of resolution result.

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  explicit ProxyResolverImpl(std::unique_ptr<ProxyResolverDelegate> delegate)
      : delegate_(std::move(delegate)),
        origin_thread_(base::ThreadTaskRunnerHandle::Get()),
        weak_ptr_factory_(this) {}

  ~ProxyResolverImpl() override {
    DCHECK(OnOriginThread());

    for (std::set<Request*>::iterator iter = all_requests_.begin();
         iter != all_requests_.end(); ++iter) {
      Request* request = *iter;
      LOG(WARNING) << "Pending request for " << request->source_url_;
      delete request;
    }
  }

  // ProxyResolverInterface override.
  void ResolveProxy(
      const std::string& source_url,
      const std::string& signal_interface,
      const std::string& signal_name,
      scoped_refptr<dbus::ExportedObject> exported_object) override {
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

    // GetRequestContext() must be called on UI thread.
    scoped_refptr<net::URLRequestContextGetter> getter =
        delegate_->GetRequestContext();

    getter->GetNetworkTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ProxyResolverImpl::ResolveProxyInternal,
                   request,
                   origin_thread_,
                   getter,
                   exported_object));
  }

 private:
  // Helper function for ResolveProxy().
  static void ResolveProxyInternal(
      Request* request,
      scoped_refptr<base::SingleThreadTaskRunner> origin_thread,
      scoped_refptr<net::URLRequestContextGetter> getter,
      scoped_refptr<dbus::ExportedObject> exported_object) {
    // Make sure we're running on IO thread.
    DCHECK(getter->GetNetworkTaskRunner()->BelongsToCurrentThread());

    // Check if we have the URLRequestContextGetter.
    if (!getter.get()) {
      request->error_ = "No URLRequestContextGetter";
      request->OnCompletion(origin_thread, net::ERR_UNEXPECTED);
      return;
    }

    // Retrieve ProxyService from profile's request context.
    net::ProxyService* proxy_service =
        getter->GetURLRequestContext()->proxy_service();
    if (!proxy_service) {
      request->error_ = "No proxy service in chrome";
      request->OnCompletion(origin_thread, net::ERR_UNEXPECTED);
      return;
    }

    VLOG(1) << "Starting network proxy resolution for "
            << request->source_url_;
    net::CompletionCallback completion_callback =
        base::Bind(&Request::OnCompletion,
                   base::Unretained(request),
                   origin_thread);
    const int result = proxy_service->ResolveProxy(
        GURL(request->source_url_), std::string(), &request->proxy_info_,
        completion_callback, NULL, NULL, net::NetLogWithSource());
    if (result != net::ERR_IO_PENDING) {
      VLOG(1) << "Network proxy resolution completed synchronously.";
      completion_callback.Run(result);
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
    return origin_thread_->BelongsToCurrentThread();
  }

  std::unique_ptr<ProxyResolverDelegate> delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_thread_;
  std::set<Request*> all_requests_;
  base::WeakPtrFactory<ProxyResolverImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverImpl);
};

ProxyResolutionServiceProvider::ProxyResolutionServiceProvider(
    ProxyResolverInterface* resolver)
    : resolver_(resolver),
      origin_thread_(base::ThreadTaskRunnerHandle::Get()),
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
  return origin_thread_->BelongsToCurrentThread();
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
    response_sender.Run(std::unique_ptr<dbus::Response>());
    return;
  }

  resolver_->ResolveProxy(source_url,
                          signal_interface,
                          signal_name,
                          exported_object_);

  // Send an empty response for now. We'll send a signal once the network proxy
  // resolution is completed.
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

// static
void ProxyResolutionServiceProvider::CallResolveProxyHandler(
    base::WeakPtr<ProxyResolutionServiceProvider> provider_weak_ptr,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!provider_weak_ptr) {
    LOG(WARNING) << "Called after the object is deleted";
    response_sender.Run(std::unique_ptr<dbus::Response>());
    return;
  }
  provider_weak_ptr->ResolveProxyHandler(method_call, response_sender);
}

ProxyResolutionServiceProvider* ProxyResolutionServiceProvider::Create(
    std::unique_ptr<ProxyResolverDelegate> delegate) {
  return new ProxyResolutionServiceProvider(
      new ProxyResolverImpl(std::move(delegate)));
}

ProxyResolverInterface::~ProxyResolverInterface() {
}

}  // namespace chromeos
