// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"

namespace chromeos {

struct ProxyResolutionServiceProvider::Request {
 public:
  Request(const std::string& source_url,
          scoped_refptr<net::URLRequestContextGetter> context_getter,
          scoped_refptr<base::SingleThreadTaskRunner> notify_thread,
          NotifyCallback notify_callback)
      : source_url(source_url),
        context_getter(context_getter),
        notify_thread(std::move(notify_thread)),
        notify_callback(std::move(notify_callback)) {}
  ~Request() = default;

  // Invokes |notify_callback| on |notify_thread|.
  void PostNotify(const std::string& error, const std::string& pac_string) {
    notify_thread->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(notify_callback), error, pac_string));
  }

  // URL being resolved.
  const std::string source_url;

  // Used to get the network context associated with the profile used to run
  // this request.
  const scoped_refptr<net::URLRequestContextGetter> context_getter;

  // Handle to ProxyResolutionService's Request
  std::unique_ptr<net::ProxyResolutionService::Request> request;

  // ProxyInfo resolved for |source_url|.
  net::ProxyInfo proxy_info;

  // The callback to invoke on completion, as well as the thread it should be
  // invoked on.
  scoped_refptr<base::SingleThreadTaskRunner> notify_thread;
  NotifyCallback notify_callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(Request);
};

ProxyResolutionServiceProvider::ProxyResolutionServiceProvider()
    : origin_thread_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

ProxyResolutionServiceProvider::~ProxyResolutionServiceProvider() {
  DCHECK(OnOriginThread());
}

void ProxyResolutionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(OnOriginThread());
  exported_object_ = exported_object;
  VLOG(1) << "ProxyResolutionServiceProvider started";
  exported_object_->ExportMethod(
      kNetworkProxyServiceInterface, kNetworkProxyServiceResolveProxyMethod,
      base::BindRepeating(&ProxyResolutionServiceProvider::DbusResolveProxy,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ProxyResolutionServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

bool ProxyResolutionServiceProvider::OnOriginThread() {
  return origin_thread_->BelongsToCurrentThread();
}

void ProxyResolutionServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (success)
    VLOG(1) << "Method exported: " << interface_name << "." << method_name;
  else
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void ProxyResolutionServiceProvider::DbusResolveProxy(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(OnOriginThread());

  VLOG(1) << "Handling method call: " << method_call->ToString();
  dbus::MessageReader reader(method_call);
  std::string source_url;
  if (!reader.PopString(&source_url)) {
    LOG(ERROR) << "Method call lacks source URL: " << method_call->ToString();
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, "No source URL string arg"));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  NotifyCallback notify_dbus_callback =
      base::BindOnce(&ProxyResolutionServiceProvider::NotifyProxyResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response),
                     std::move(response_sender));

  ResolveProxyInternal(source_url, std::move(notify_dbus_callback));
}

void ProxyResolutionServiceProvider::ResolveProxyInternal(
    const std::string& source_url,
    NotifyCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      request_context_getter_for_test_
          ? request_context_getter_for_test_
          : ProfileManager::GetPrimaryUserProfile()->GetRequestContext();

  std::unique_ptr<Request> request = std::make_unique<Request>(
      source_url, context_getter, origin_thread_, std::move(callback));

  // This would ideally call PostTaskAndReply() instead of PostTask(), but
  // ResolveProxyOnNetworkThread()'s call to
  // net::ProxyResolutionService::DbusResolveProxy() can result in an
  // asynchronous lookup, in which case the result won't be available
  // immediately.
  context_getter->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ProxyResolutionServiceProvider::ResolveProxyOnNetworkThread,
          std::move(request)));
}

// static
void ProxyResolutionServiceProvider::ResolveProxyOnNetworkThread(
    std::unique_ptr<Request> request) {
  DCHECK(request->context_getter->GetNetworkTaskRunner()
             ->BelongsToCurrentThread());

  net::ProxyResolutionService* proxy_resolution_service =
      request->context_getter->GetURLRequestContext()
          ->proxy_resolution_service();
  if (!proxy_resolution_service) {
    request->PostNotify("No proxy service in chrome", "DIRECT");
    return;
  }

  Request* request_ptr = request.get();
  net::CompletionCallback callback =
      base::BindRepeating(&ProxyResolutionServiceProvider::OnResolutionComplete,
                          base::Passed(std::move(request)));

  VLOG(1) << "Starting network proxy resolution for "
          << request_ptr->source_url;
  const int result = proxy_resolution_service->ResolveProxy(
      GURL(request_ptr->source_url), std::string(), &request_ptr->proxy_info,
      callback, &request_ptr->request, net::NetLogWithSource());
  if (result != net::ERR_IO_PENDING) {
    VLOG(1) << "Network proxy resolution completed synchronously.";
    callback.Run(result);
  }
}

// static
void ProxyResolutionServiceProvider::OnResolutionComplete(
    std::unique_ptr<Request> request,
    int result) {
  DCHECK(request->context_getter->GetNetworkTaskRunner()
             ->BelongsToCurrentThread());

  request->request.reset();

  request->PostNotify(
      result == net::OK ? "" : net::ErrorToString(result),
      result == net::OK ? request->proxy_info.ToPacString() : "DIRECT");
}

void ProxyResolutionServiceProvider::NotifyProxyResolved(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::string& error,
    const std::string& pac_string) {
  DCHECK(OnOriginThread());

  // Reply to the original D-Bus method call.
  dbus::MessageWriter writer(response.get());
  writer.AppendString(pac_string);
  writer.AppendString(error);
  response_sender.Run(std::move(response));
}

}  // namespace chromeos
