// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/local_fetcher.h"
#include "mojo/shell/network_fetcher.h"
#include "mojo/shell/query_util.h"
#include "mojo/shell/shell_impl.h"
#include "mojo/shell/switches.h"
#include "third_party/mojo_services/src/content_handler/public/interfaces/content_handler.mojom.h"

namespace mojo {
namespace shell {

namespace {

// Used by TestAPI.
bool has_created_instance = false;

}  // namespace

class ApplicationManager::ContentHandlerConnection : public ErrorHandler {
 public:
  ContentHandlerConnection(ApplicationManager* manager,
                           const GURL& content_handler_url,
                           const GURL& requestor_url,
                           const std::string& qualifier)
      : manager_(manager),
        content_handler_url_(content_handler_url),
        content_handler_qualifier_(qualifier) {
    ServiceProviderPtr services;
    manager->ConnectToApplicationWithParameters(
        content_handler_url, qualifier, requestor_url, GetProxy(&services),
        nullptr, base::Closure(), std::vector<std::string>());
    MessagePipe pipe;
    content_handler_.Bind(pipe.handle0.Pass());
    services->ConnectToService(ContentHandler::Name_, pipe.handle1.Pass());
    content_handler_.set_error_handler(this);
  }

  ContentHandler* content_handler() { return content_handler_.get(); }

  GURL content_handler_url() { return content_handler_url_; }
  std::string content_handler_qualifier() { return content_handler_qualifier_; }

 private:
  // ErrorHandler implementation:
  void OnConnectionError() override { manager_->OnContentHandlerError(this); }

  ApplicationManager* manager_;
  GURL content_handler_url_;
  std::string content_handler_qualifier_;
  ContentHandlerPtr content_handler_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerConnection);
};

// static
ApplicationManager::TestAPI::TestAPI(ApplicationManager* manager)
    : manager_(manager) {
}

ApplicationManager::TestAPI::~TestAPI() {
}

bool ApplicationManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ApplicationManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->identity_to_shell_impl_.find(Identity(url)) !=
         manager_->identity_to_shell_impl_.end();
}

ApplicationManager::ApplicationManager(Delegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
}

ApplicationManager::~ApplicationManager() {
  STLDeleteValues(&url_to_content_handler_);
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  STLDeleteValues(&scheme_to_loader_);
}

void ApplicationManager::TerminateShellConnections() {
  STLDeleteValues(&identity_to_shell_impl_);
}

void ApplicationManager::ConnectToApplication(
    const GURL& requested_url,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const base::Closure& on_application_end) {
  ConnectToApplicationWithParameters(
      requested_url, std::string(), requestor_url, services.Pass(),
      exposed_services.Pass(), on_application_end, std::vector<std::string>());
}

void ApplicationManager::ConnectToApplicationWithParameters(
    const GURL& requested_url,
    const std::string& qualifier,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const base::Closure& on_application_end,
    const std::vector<std::string>& pre_redirect_parameters) {
  TRACE_EVENT_INSTANT1(
      "mojo_shell", "ApplicationManager::ConnectToApplicationWithParameters",
      TRACE_EVENT_SCOPE_THREAD, "requested_url", requested_url.spec());
  DCHECK(requested_url.is_valid());

  // We check both the mapped and resolved urls for existing shell_impls because
  // external applications can be registered for the unresolved mojo:foo urls.

  GURL mapped_url = delegate_->ResolveMappings(requested_url);
  if (ConnectToRunningApplication(mapped_url, qualifier, requestor_url,
                                  &services, &exposed_services)) {
    return;
  }

  GURL resolved_url = delegate_->ResolveMojoURL(mapped_url);
  if (ConnectToRunningApplication(resolved_url, qualifier, requestor_url,
                                  &services, &exposed_services)) {
    return;
  }

  // The application is not running, let's compute the parameters.
  if (ConnectToApplicationWithLoader(
          requested_url, qualifier, mapped_url, requestor_url, &services,
          &exposed_services, on_application_end, pre_redirect_parameters,
          GetLoaderForURL(mapped_url))) {
    return;
  }

  if (ConnectToApplicationWithLoader(
          requested_url, qualifier, resolved_url, requestor_url, &services,
          &exposed_services, on_application_end, pre_redirect_parameters,
          GetLoaderForURL(resolved_url))) {
    return;
  }

  if (ConnectToApplicationWithLoader(
          requested_url, qualifier, resolved_url, requestor_url, &services,
          &exposed_services, on_application_end, pre_redirect_parameters,
          default_loader_.get())) {
    return;
  }

  auto callback = base::Bind(
      &ApplicationManager::HandleFetchCallback, weak_ptr_factory_.GetWeakPtr(),
      requested_url, qualifier, requestor_url, base::Passed(services.Pass()),
      base::Passed(exposed_services.Pass()), on_application_end,
      pre_redirect_parameters);

  if (delegate_->CreateFetcher(
          resolved_url,
          base::Bind(callback, NativeApplicationCleanup::DONT_DELETE))) {
    return;
  }

  if (resolved_url.SchemeIsFile()) {
    new LocalFetcher(
        resolved_url, GetBaseURLAndQuery(resolved_url, nullptr),
        base::Bind(callback, NativeApplicationCleanup::DONT_DELETE));
    return;
  }

  if (!network_service_)
    ConnectToService(GURL("mojo:network_service"), &network_service_);

  const NativeApplicationCleanup cleanup =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDontDeleteOnDownload)
          ? NativeApplicationCleanup::DONT_DELETE
          : NativeApplicationCleanup::DELETE;

  new NetworkFetcher(disable_cache_, resolved_url, network_service_.get(),
                     base::Bind(callback, cleanup));
}

bool ApplicationManager::ConnectToRunningApplication(
    const GURL& resolved_url,
    const std::string& qualifier,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider>* services,
    ServiceProviderPtr* exposed_services) {
  GURL application_url = GetBaseURLAndQuery(resolved_url, nullptr);
  ShellImpl* shell_impl = GetShellImpl(application_url, qualifier);
  if (!shell_impl)
    return false;

  ConnectToClient(shell_impl, resolved_url, requestor_url, services->Pass(),
                  exposed_services->Pass());
  return true;
}

bool ApplicationManager::ConnectToApplicationWithLoader(
    const GURL& requested_url,
    const std::string& qualifier,
    const GURL& resolved_url,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider>* services,
    ServiceProviderPtr* exposed_services,
    const base::Closure& on_application_end,
    const std::vector<std::string>& parameters,
    ApplicationLoader* loader) {
  if (!loader)
    return false;

  const GURL app_url =
      requested_url.scheme() == "mojo" ? requested_url : resolved_url;

  loader->Load(
      resolved_url,
      RegisterShell(app_url, qualifier, requestor_url, services->Pass(),
                    exposed_services->Pass(), on_application_end, parameters));
  return true;
}

InterfaceRequest<Application> ApplicationManager::RegisterShell(
    const GURL& app_url,
    const std::string& qualifier,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const base::Closure& on_application_end,
    const std::vector<std::string>& parameters) {
  Identity app_identity(app_url, qualifier);

  ApplicationPtr application;
  InterfaceRequest<Application> application_request = GetProxy(&application);
  ShellImpl* shell =
      new ShellImpl(application.Pass(), this, app_identity, on_application_end);
  identity_to_shell_impl_[app_identity] = shell;
  shell->InitializeApplication(Array<String>::From(parameters));
  ConnectToClient(shell, app_url, requestor_url, services.Pass(),
                  exposed_services.Pass());
  return application_request.Pass();
}

ShellImpl* ApplicationManager::GetShellImpl(const GURL& url,
                                            const std::string& qualifier) {
  const auto& shell_it = identity_to_shell_impl_.find(Identity(url, qualifier));
  if (shell_it != identity_to_shell_impl_.end())
    return shell_it->second;
  return nullptr;
}

void ApplicationManager::ConnectToClient(
    ShellImpl* shell_impl,
    const GURL& resolved_url,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services) {
  shell_impl->ConnectToClient(resolved_url, requestor_url, services.Pass(),
                              exposed_services.Pass());
}

void ApplicationManager::HandleFetchCallback(
    const GURL& requested_url,
    const std::string& qualifier,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const base::Closure& on_application_end,
    const std::vector<std::string>& parameters,
    NativeApplicationCleanup cleanup,
    scoped_ptr<Fetcher> fetcher) {
  if (!fetcher) {
    // Network error. Drop |application_request| to tell requestor.
    return;
  }

  GURL redirect_url = fetcher->GetRedirectURL();
  if (!redirect_url.is_empty()) {
    // And around we go again... Whee!
    // TODO(sky): this loses |requested_url|.
    ConnectToApplicationWithParameters(redirect_url, qualifier, requestor_url,
                                       services.Pass(), exposed_services.Pass(),
                                       on_application_end, parameters);
    return;
  }

  // We already checked if the application was running before we fetched it, but
  // it might have started while the fetch was outstanding. We don't want to
  // have two copies of the app running, so check again.
  //
  // Also, it's possible the original URL was redirected to an app that is
  // already running.
  if (ConnectToRunningApplication(requested_url, qualifier, requestor_url,
                                  &services, &exposed_services)) {
    return;
  }

  const GURL app_url =
      requested_url.scheme() == "mojo" ? requested_url : fetcher->GetURL();

  InterfaceRequest<Application> request(
      RegisterShell(app_url, qualifier, requestor_url, services.Pass(),
                    exposed_services.Pass(), on_application_end, parameters));

  // If the response begins with a #!mojo <content-handler-url>, use it.
  GURL content_handler_url;
  std::string shebang;
  if (fetcher->PeekContentHandler(&shebang, &content_handler_url)) {
    LoadWithContentHandler(
        content_handler_url, requestor_url, qualifier, request.Pass(),
        fetcher->AsURLResponse(blocking_pool_,
                               static_cast<int>(shebang.size())));
    return;
  }

  MimeTypeToURLMap::iterator iter = mime_type_to_url_.find(fetcher->MimeType());
  if (iter != mime_type_to_url_.end()) {
    LoadWithContentHandler(iter->second, requestor_url, qualifier,
                           request.Pass(),
                           fetcher->AsURLResponse(blocking_pool_, 0));
    return;
  }

  auto alias_iter = application_package_alias_.find(app_url);
  if (alias_iter != application_package_alias_.end()) {
    // We replace the qualifier with the one our package alias requested.
    URLResponsePtr response(URLResponse::New());
    response->url = String::From(app_url.spec());

    std::string qualifier;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableMultiprocess)) {
      // Why can't we use this in single process mode? Because of
      // base::AtExitManager. If you link in ApplicationRunnerChromium into
      // your code, and then we make initialize multiple copies of the
      // application, we end up with multiple AtExitManagers and will check on
      // the second one being created.
      //
      // Why doesn't that happen when running different apps? Because
      // your_thing.mojo!base::AtExitManager and
      // my_thing.mojo!base::AtExitManager are different symbols.
      qualifier = alias_iter->second.second;
    }

    LoadWithContentHandler(alias_iter->second.first, requestor_url, qualifier,
                           request.Pass(), response.Pass());
    return;
  }

  // TODO(aa): Sanity check that the thing we got looks vaguely like a mojo
  // application. That could either mean looking for the platform-specific dll
  // header, or looking for some specific mojo signature prepended to the
  // library.
  // TODO(vtl): (Maybe this should be done by the factory/runner?)

  GURL base_resolved_url = GetBaseURLAndQuery(fetcher->GetURL(), nullptr);
  NativeRunnerFactory::Options options;
  if (url_to_native_options_.find(base_resolved_url) !=
      url_to_native_options_.end()) {
    DVLOG(2) << "Applying stored native options to resolved URL "
             << fetcher->GetURL();
    options = url_to_native_options_[base_resolved_url];
  }

  fetcher->AsPath(
      blocking_pool_,
      base::Bind(&ApplicationManager::RunNativeApplication,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(request.Pass()),
                 options, cleanup, base::Passed(fetcher.Pass())));
}

void ApplicationManager::RunNativeApplication(
    InterfaceRequest<Application> application_request,
    const NativeRunnerFactory::Options& options,
    NativeApplicationCleanup cleanup,
    scoped_ptr<Fetcher> fetcher,
    const base::FilePath& path,
    bool path_exists) {
  // We only passed fetcher to keep it alive. Done with it now.
  fetcher.reset();

  DCHECK(application_request.is_pending());

  if (!path_exists) {
    LOG(ERROR) << "Library not started because library path '" << path.value()
               << "' does not exist.";
    return;
  }

  TRACE_EVENT1("mojo_shell", "ApplicationManager::RunNativeApplication", "path",
               path.AsUTF8Unsafe());
  NativeRunner* runner = native_runner_factory_->Create(options).release();
  native_runners_.push_back(runner);
  runner->Start(path, cleanup, application_request.Pass(),
                base::Bind(&ApplicationManager::CleanupRunner,
                           weak_ptr_factory_.GetWeakPtr(), runner));
}

void ApplicationManager::RegisterContentHandler(
    const std::string& mime_type,
    const GURL& content_handler_url) {
  DCHECK(content_handler_url.is_valid())
      << "Content handler URL is invalid for mime type " << mime_type;
  mime_type_to_url_[mime_type] = content_handler_url;
}

void ApplicationManager::RegisterApplicationPackageAlias(
    const GURL& alias,
    const GURL& content_handler_package,
    const std::string& qualifier) {
  application_package_alias_[alias] =
      std::make_pair(content_handler_package, qualifier);
}

void ApplicationManager::LoadWithContentHandler(
    const GURL& content_handler_url,
    const GURL& requestor_url,
    const std::string& qualifier,
    InterfaceRequest<Application> application_request,
    URLResponsePtr url_response) {
  ContentHandlerConnection* connection = nullptr;
  std::pair<GURL, std::string> key(content_handler_url, qualifier);
  URLToContentHandlerMap::iterator iter = url_to_content_handler_.find(key);
  if (iter != url_to_content_handler_.end()) {
    connection = iter->second;
  } else {
    connection = new ContentHandlerConnection(this, content_handler_url,
                                              requestor_url, qualifier);
    url_to_content_handler_[key] = connection;
  }

  connection->content_handler()->StartApplication(application_request.Pass(),
                                                  url_response.Pass());
}

void ApplicationManager::SetLoaderForURL(scoped_ptr<ApplicationLoader> loader,
                                         const GURL& url) {
  URLToLoaderMap::iterator it = url_to_loader_.find(url);
  if (it != url_to_loader_.end())
    delete it->second;
  url_to_loader_[url] = loader.release();
}

void ApplicationManager::SetLoaderForScheme(
    scoped_ptr<ApplicationLoader> loader,
    const std::string& scheme) {
  SchemeToLoaderMap::iterator it = scheme_to_loader_.find(scheme);
  if (it != scheme_to_loader_.end())
    delete it->second;
  scheme_to_loader_[scheme] = loader.release();
}

void ApplicationManager::SetNativeOptionsForURL(
    const NativeRunnerFactory::Options& options,
    const GURL& url) {
  DCHECK(!url.has_query());  // Precondition.
  // Apply mappings and resolution to get the resolved URL.
  GURL resolved_url =
      delegate_->ResolveMojoURL(delegate_->ResolveMappings(url));
  DCHECK(!resolved_url.has_query());  // Still shouldn't have query.
  // TODO(vtl): We should probably also remove/disregard the query string (and
  // maybe canonicalize in other ways).
  DVLOG(2) << "Storing native options for resolved URL " << resolved_url
           << " (original URL " << url << ")";
  url_to_native_options_[resolved_url] = options;
}

ApplicationLoader* ApplicationManager::GetLoaderForURL(const GURL& url) {
  auto url_it = url_to_loader_.find(GetBaseURLAndQuery(url, nullptr));
  if (url_it != url_to_loader_.end())
    return url_it->second;
  auto scheme_it = scheme_to_loader_.find(url.scheme());
  if (scheme_it != scheme_to_loader_.end())
    return scheme_it->second;
  return nullptr;
}

void ApplicationManager::OnShellImplError(ShellImpl* shell_impl) {
  // Called from ~ShellImpl, so we do not need to call Destroy here.
  const Identity identity = shell_impl->identity();
  base::Closure on_application_end = shell_impl->on_application_end();
  // Remove the shell.
  auto it = identity_to_shell_impl_.find(identity);
  DCHECK(it != identity_to_shell_impl_.end());
  delete it->second;
  identity_to_shell_impl_.erase(it);
  if (!on_application_end.is_null())
    on_application_end.Run();
}

void ApplicationManager::OnContentHandlerError(
    ContentHandlerConnection* content_handler) {
  // Remove the mapping to the content handler.
  auto it = url_to_content_handler_.find(
      std::make_pair(content_handler->content_handler_url(),
                     content_handler->content_handler_qualifier()));
  DCHECK(it != url_to_content_handler_.end());
  delete it->second;
  url_to_content_handler_.erase(it);
}

ScopedMessagePipeHandle ApplicationManager::ConnectToServiceByName(
    const GURL& application_url,
    const std::string& interface_name) {
  ServiceProviderPtr services;
  ConnectToApplication(application_url, GURL(), GetProxy(&services), nullptr,
                       base::Closure());
  MessagePipe pipe;
  services->ConnectToService(interface_name, pipe.handle1.Pass());
  return pipe.handle0.Pass();
}

void ApplicationManager::CleanupRunner(NativeRunner* runner) {
  native_runners_.erase(
      std::find(native_runners_.begin(), native_runners_.end(), runner));
}

}  // namespace shell
}  // namespace mojo
