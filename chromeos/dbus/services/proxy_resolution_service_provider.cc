// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/proxy_resolution_service_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"

namespace chromeos {

struct ProxyResolutionServiceProvider::Request {
 public:
  // Constructor for returning proxy info via an asynchronous D-Bus response.
  Request(const std::string& source_url,
          std::unique_ptr<dbus::Response> response,
          const dbus::ExportedObject::ResponseSender& response_sender,
          scoped_refptr<net::URLRequestContextGetter> context_getter)
      : source_url(source_url),
        response(std::move(response)),
        response_sender(response_sender),
        context_getter(context_getter) {
    DCHECK(this->response);
    DCHECK(!response_sender.is_null());
  }

  // Constructor for returning proxy info via a D-Bus signal.
  Request(const std::string& source_url,
          const std::string& signal_interface,
          const std::string& signal_name,
          scoped_refptr<net::URLRequestContextGetter> context_getter)
      : source_url(source_url),
        signal_interface(signal_interface),
        signal_name(signal_name),
        context_getter(context_getter) {
    DCHECK(!signal_interface.empty());
    DCHECK(!signal_name.empty());
  }

  ~Request() = default;

  // URL being resolved.
  const std::string source_url;

  // D-Bus response and callback for returning data on resolution completion.
  // Either these two members or |signal_interface|/|signal_name| must be
  // supplied, but not both.
  std::unique_ptr<dbus::Response> response;
  const dbus::ExportedObject::ResponseSender response_sender;

  // D-Bus interface and name for emitting result signal after resolution is
  // complete.
  // TODO(derat): Remove these and associated code after all callers use async
  // responses instead of signals: http://crbug.com/446115
  const std::string signal_interface;
  const std::string signal_name;

  // Used to get the network context associated with the profile used to run
  // this request.
  const scoped_refptr<net::URLRequestContextGetter> context_getter;

  // ProxyInfo resolved for |source_url|.
  net::ProxyInfo proxy_info;

  // Error from proxy resolution.
  std::string error;

 private:
  DISALLOW_COPY_AND_ASSIGN(Request);
};

ProxyResolutionServiceProvider::ProxyResolutionServiceProvider(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      origin_thread_(base::ThreadTaskRunnerHandle::Get()),
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
      kLibCrosServiceInterface, kResolveNetworkProxy,
      base::Bind(&ProxyResolutionServiceProvider::ResolveProxy,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyResolutionServiceProvider::OnExported,
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

void ProxyResolutionServiceProvider::ResolveProxy(
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

  // The signal interface and name arguments are optional but must be supplied
  // together.
  std::string signal_interface, signal_name;
  if (reader.HasMoreData() &&
      (!reader.PopString(&signal_interface) || signal_interface.empty() ||
       !reader.PopString(&signal_name) || signal_name.empty())) {
    LOG(ERROR) << "Method call has invalid interface/name args: "
               << method_call->ToString();
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, "Invalid interface/name args"));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      delegate_->GetRequestContext();

  // If signal information was supplied, emit a signal instead of including
  // proxy information in the response.
  std::unique_ptr<Request> request =
      !signal_interface.empty()
          ? base::MakeUnique<Request>(source_url, signal_interface, signal_name,
                                      context_getter)
          : base::MakeUnique<Request>(source_url, std::move(response),
                                      response_sender, context_getter);

  // This would ideally call PostTaskAndReply() instead of PostTask(), but
  // ResolveProxyOnNetworkThread()'s call to net::ProxyService::ResolveProxy()
  // can result in an asynchronous lookup, in which case the result won't be
  // available immediately.
  context_getter->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyResolutionServiceProvider::ResolveProxyOnNetworkThread,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(request))));

  // If we didn't already pass the response to the Request object because we're
  // returning data via a signal, send an empty response immediately.
  if (response)
    response_sender.Run(std::move(response));
}

void ProxyResolutionServiceProvider::ResolveProxyOnNetworkThread(
    std::unique_ptr<Request> request) {
  DCHECK(request->context_getter->GetNetworkTaskRunner()
             ->BelongsToCurrentThread());

  net::ProxyService* proxy_service =
      request->context_getter->GetURLRequestContext()->proxy_service();
  if (!proxy_service) {
    request->error = "No proxy service in chrome";
    OnResolutionComplete(std::move(request), net::ERR_UNEXPECTED);
    return;
  }

  Request* request_ptr = request.get();
  net::CompletionCallback callback = base::Bind(
      &ProxyResolutionServiceProvider::OnResolutionComplete,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(request)));

  VLOG(1) << "Starting network proxy resolution for "
          << request_ptr->source_url;
  const int result =
      delegate_->ResolveProxy(proxy_service, GURL(request_ptr->source_url),
                              &request_ptr->proxy_info, callback);
  if (result != net::ERR_IO_PENDING) {
    VLOG(1) << "Network proxy resolution completed synchronously.";
    callback.Run(result);
  }
}

void ProxyResolutionServiceProvider::OnResolutionComplete(
    std::unique_ptr<Request> request,
    int result) {
  DCHECK(request->context_getter->GetNetworkTaskRunner()
             ->BelongsToCurrentThread());

  if (request->error.empty() && result != net::OK)
    request->error = net::ErrorToString(result);

  origin_thread_->PostTask(
      FROM_HERE,
      base::Bind(&ProxyResolutionServiceProvider::NotifyProxyResolved,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(request))));
}

void ProxyResolutionServiceProvider::NotifyProxyResolved(
    std::unique_ptr<Request> request) {
  DCHECK(OnOriginThread());

  if (request->response) {
    // Reply to the original D-Bus method call.
    dbus::MessageWriter writer(request->response.get());
    writer.AppendString(request->proxy_info.ToPacString());
    writer.AppendString(request->error);
    request->response_sender.Run(std::move(request->response));
  } else {
    // Send a signal to the client.
    dbus::Signal signal(request->signal_interface, request->signal_name);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(request->source_url);
    writer.AppendString(request->proxy_info.ToPacString());
    writer.AppendString(request->error);
    exported_object_->SendSignal(&signal);
    VLOG(1) << "Sending signal: " << signal.ToString();
  }
}

}  // namespace chromeos
