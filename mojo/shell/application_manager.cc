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
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/application_fetcher.h"
#include "mojo/shell/application_instance.h"
#include "mojo/shell/content_handler_connection.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/query_util.h"
#include "mojo/shell/switches.h"

namespace mojo {
namespace shell {

namespace {

// Used by TestAPI.
bool has_created_instance = false;

void OnEmptyOnConnectCallback(uint32_t content_handler_id) {}

}  // namespace

// static
ApplicationManager::TestAPI::TestAPI(ApplicationManager* manager)
    : manager_(manager) {
}

ApplicationManager::TestAPI::~TestAPI() {
}

bool ApplicationManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ApplicationManager::TestAPI::HasRunningInstanceForURL(
    const GURL& url) const {
  return manager_->identity_to_instance_.find(Identity(url)) !=
         manager_->identity_to_instance_.end();
}

ApplicationManager::ApplicationManager(scoped_ptr<ApplicationFetcher> fetcher)
    : fetcher_(fetcher.Pass()),
      content_handler_id_counter_(0u),
      weak_ptr_factory_(this) {
  fetcher_->SetApplicationManager(this);
}

ApplicationManager::~ApplicationManager() {
  URLToContentHandlerMap url_to_content_handler(url_to_content_handler_);
  for (auto& pair : url_to_content_handler)
    pair.second->CloseConnection();
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  STLDeleteValues(&scheme_to_loader_);
}

void ApplicationManager::TerminateShellConnections() {
  STLDeleteValues(&identity_to_instance_);
}

void ApplicationManager::ConnectToApplication(
    ApplicationInstance* originator,
    URLRequestPtr app_url_request,
    const std::string& qualifier,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const CapabilityFilter& filter,
    const base::Closure& on_application_end,
    const Shell::ConnectToApplicationCallback& connect_callback) {
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->SetOriginatorInfo(originator);
  params->SetURLInfo(app_url_request.Pass());
  params->set_qualifier(qualifier);
  params->set_services(services.Pass());
  params->set_exposed_services(exposed_services.Pass());
  params->set_filter(filter);
  params->set_on_application_end(on_application_end);
  params->set_connect_callback(connect_callback);

  ConnectToApplication(params.Pass());
}

void ApplicationManager::ConnectToApplication(
    scoped_ptr<ConnectToApplicationParams> params) {
  GURL original_url = params->app_url();
  URLRequestPtr original_url_request = params->TakeAppURLRequest();

  TRACE_EVENT_INSTANT1("mojo_shell", "ApplicationManager::ConnectToApplication",
                       TRACE_EVENT_SCOPE_THREAD, "original_url",
                       original_url.spec());
  DCHECK(original_url.is_valid());
  DCHECK(original_url_request);

  // We need to look for running instances based on both the unresolved and
  // resolved urls.
  if (ConnectToRunningApplication(&params))
    return;

  GURL resolved_url = fetcher_->ResolveURL(original_url);
  params->SetURLInfo(resolved_url);
  if (ConnectToRunningApplication(&params))
    return;

  // The application is not running, let's compute the parameters.
  // NOTE: Set URL info using |original_url_request| instead of |original_url|
  // because it may contain more information (e.g., it is a POST request).
  params->SetURLInfo(original_url_request.Pass());
  ApplicationLoader* loader = GetLoaderForURL(resolved_url);
  if (loader) {
    ConnectToApplicationWithLoader(&params, resolved_url, loader);
    return;
  }

  original_url_request = params->TakeAppURLRequest();
  auto callback =
      base::Bind(&ApplicationManager::HandleFetchCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&params));
  fetcher_->FetchRequest(original_url_request.Pass(), callback);
}

bool ApplicationManager::ConnectToRunningApplication(
    scoped_ptr<ConnectToApplicationParams>* params) {
  ApplicationInstance* instance = GetApplicationInstance(
      Identity((*params)->app_url(), (*params)->qualifier()));
  if (!instance)
    return false;

  instance->ConnectToClient(params->Pass());
  return true;
}

void ApplicationManager::ConnectToApplicationWithLoader(
    scoped_ptr<ConnectToApplicationParams>* params,
    const GURL& resolved_url,
    ApplicationLoader* loader) {
  if (!(*params)->app_url().SchemeIs("mojo"))
    (*params)->SetURLInfo(resolved_url);

  loader->Load(resolved_url, RegisterInstance(params->Pass(), nullptr));
}

InterfaceRequest<Application> ApplicationManager::RegisterInstance(
    scoped_ptr<ConnectToApplicationParams> params,
    ApplicationInstance** resulting_instance) {
  Identity app_identity(params->app_url(), params->qualifier());

  ApplicationPtr application;
  InterfaceRequest<Application> application_request = GetProxy(&application);
  ApplicationInstance* instance = new ApplicationInstance(
      application.Pass(), this, params->originator_identity(), app_identity,
      params->filter(), Shell::kInvalidContentHandlerID,
      params->on_application_end());
  DCHECK(identity_to_instance_.find(app_identity) ==
         identity_to_instance_.end());
  identity_to_instance_[app_identity] = instance;
  instance->InitializeApplication();
  instance->ConnectToClient(params.Pass());
  if (resulting_instance)
    *resulting_instance = instance;
  return application_request.Pass();
}

ApplicationInstance* ApplicationManager::GetApplicationInstance(
    const Identity& identity) const {
  const auto& instance_it = identity_to_instance_.find(identity);
  if (instance_it != identity_to_instance_.end())
    return instance_it->second;
  return nullptr;
}

void ApplicationManager::HandleFetchCallback(
    scoped_ptr<ConnectToApplicationParams> params,
    scoped_ptr<Fetcher> fetcher) {
  if (!fetcher) {
    // Network error. Drop |params| to tell the requestor.
    params->connect_callback().Run(Shell::kInvalidContentHandlerID);
    return;
  }

  GURL redirect_url = fetcher->GetRedirectURL();
  if (!redirect_url.is_empty()) {
    // And around we go again... Whee!
    // TODO(sky): this loses the original URL info.
    URLRequestPtr new_request = URLRequest::New();
    new_request->url = redirect_url.spec();
    HttpHeaderPtr header = HttpHeader::New();
    header->name = "Referer";
    header->value = fetcher->GetRedirectReferer().spec();
    new_request->headers.push_back(header.Pass());
    params->SetURLInfo(new_request.Pass());
    ConnectToApplication(params.Pass());
    return;
  }

  // We already checked if the application was running before we fetched it, but
  // it might have started while the fetch was outstanding. We don't want to
  // have two copies of the app running, so check again.
  //
  // Also, it's possible the original URL was redirected to an app that is
  // already running.
  if (ConnectToRunningApplication(&params))
    return;

  if (params->app_url().scheme() != "mojo")
    params->SetURLInfo(fetcher->GetURL());

  Identity originator_identity = params->originator_identity();
  CapabilityFilter originator_filter = params->originator_filter();
  CapabilityFilter filter = params->filter();
  GURL app_url = params->app_url();
  std::string qualifier = params->qualifier();
  Shell::ConnectToApplicationCallback connect_callback =
      params->connect_callback();
  params->set_connect_callback(EmptyConnectCallback());
  ApplicationInstance* app = nullptr;
  InterfaceRequest<Application> request(RegisterInstance(params.Pass(), &app));

  // For resources that are loaded with content handlers, we group app instances
  // by site.

  // If the response begins with a #!mojo <content-handler-url>, use it.
  GURL content_handler_url;
  std::string shebang;
  // TODO(beng): it seems like some delegate should/would want to have a say in
  //             configuring the qualifier also.
  bool enable_multi_process = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMultiprocess);

  if (fetcher->PeekContentHandler(&shebang, &content_handler_url)) {
    URLResponsePtr response(fetcher->AsURLResponse(
        blocking_pool_, static_cast<int>(shebang.size())));
    std::string site =
        enable_multi_process ? response->site.To<std::string>() : std::string();
    LoadWithContentHandler(originator_identity, originator_filter,
                           content_handler_url, site, filter, connect_callback,
                           app, request.Pass(), response.Pass());
    return;
  }

  MimeTypeToURLMap::iterator iter = mime_type_to_url_.find(fetcher->MimeType());
  if (iter != mime_type_to_url_.end()) {
    URLResponsePtr response(fetcher->AsURLResponse(blocking_pool_, 0));
    std::string site =
        enable_multi_process ? response->site.To<std::string>() : std::string();
    LoadWithContentHandler(originator_identity, originator_filter, iter->second,
                           site, filter, connect_callback, app, request.Pass(),
                           response.Pass());
    return;
  }

  auto alias_iter = application_package_alias_.find(app_url);
  if (alias_iter != application_package_alias_.end()) {
    // We replace the qualifier with the one our package alias requested.
    URLResponsePtr response(URLResponse::New());
    response->url = app_url.spec();

    std::string qualifier;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableMultiprocess)) {
      // Why can't we use this in single process mode? Because of
      // base::AtExitManager. If you link in ApplicationRunner into
      // your code, and then we make initialize multiple copies of the
      // application, we end up with multiple AtExitManagers and will check on
      // the second one being created.
      //
      // Why doesn't that happen when running different apps? Because
      // your_thing.mojo!base::AtExitManager and
      // my_thing.mojo!base::AtExitManager are different symbols.
      qualifier = alias_iter->second.second;
    }

    LoadWithContentHandler(originator_identity, originator_filter,
                           alias_iter->second.first, qualifier, filter,
                           connect_callback, app, request.Pass(),
                           response.Pass());
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

  // TODO(erg): Have a better way of switching the sandbox on. For now, switch
  // it on hard coded when we're using some of the sandboxable core services.
  bool start_sandboxed = false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMojoNoSandbox)) {
    if (app_url == GURL("mojo://core_services/") && qualifier == "Core")
      start_sandboxed = true;
    else if (app_url == GURL("mojo://html_viewer/"))
      start_sandboxed = true;
  }

  connect_callback.Run(Shell::kInvalidContentHandlerID);

  fetcher->AsPath(blocking_pool_,
                  base::Bind(&ApplicationManager::RunNativeApplication,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Passed(request.Pass()), start_sandboxed,
                             options, base::Passed(fetcher.Pass())));
}

void ApplicationManager::RunNativeApplication(
    InterfaceRequest<Application> application_request,
    bool start_sandboxed,
    const NativeRunnerFactory::Options& options,
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
  runner->Start(path, start_sandboxed, NativeApplicationCleanup::DONT_DELETE,
                application_request.Pass(),
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
    const Identity& originator_identity,
    const CapabilityFilter& originator_filter,
    const GURL& content_handler_url,
    const std::string& qualifier,
    const CapabilityFilter& filter,
    const Shell::ConnectToApplicationCallback& connect_callback,
    ApplicationInstance* app,
    InterfaceRequest<Application> application_request,
    URLResponsePtr url_response) {
  ContentHandlerConnection* connection = nullptr;
  std::pair<GURL, std::string> key(content_handler_url, qualifier);
  // TODO(beng): Figure out the extent to which capability filter should be
  //             factored into handler identity.
  URLToContentHandlerMap::iterator iter = url_to_content_handler_.find(key);
  if (iter != url_to_content_handler_.end()) {
    connection = iter->second;
  } else {
    connection = new ContentHandlerConnection(
        this, originator_identity, originator_filter, content_handler_url,
        qualifier, filter, ++content_handler_id_counter_);
    url_to_content_handler_[key] = connection;
  }

  app->set_requesting_content_handler_id(connection->id());
  connection->content_handler()->StartApplication(application_request.Pass(),
                                                  url_response.Pass());
  connect_callback.Run(connection->id());
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
  GURL resolved_url = fetcher_->ResolveURL(url);
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
  return default_loader_.get();
}

void ApplicationManager::OnApplicationInstanceError(
    ApplicationInstance* instance) {
  // Called from ~ApplicationInstance, so we do not need to call Destroy here.
  const Identity identity = instance->identity();
  base::Closure on_application_end = instance->on_application_end();
  // Remove the shell.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  delete it->second;
  identity_to_instance_.erase(it);
  if (!on_application_end.is_null())
    on_application_end.Run();
}

void ApplicationManager::OnContentHandlerConnectionClosed(
    ContentHandlerConnection* content_handler) {
  // Remove the mapping to the content handler.
  auto it = url_to_content_handler_.find(
      std::make_pair(content_handler->content_handler_url(),
                     content_handler->content_handler_qualifier()));
  DCHECK(it != url_to_content_handler_.end());
  url_to_content_handler_.erase(it);
}

void ApplicationManager::CleanupRunner(NativeRunner* runner) {
  native_runners_.erase(
      std::find(native_runners_.begin(), native_runners_.end(), runner));
}

ScopedMessagePipeHandle ApplicationManager::ConnectToServiceByName(
    const GURL& application_url,
    const std::string& interface_name) {
  ServiceProviderPtr services;
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->SetURLInfo(application_url);
  params->set_services(GetProxy(&services));
  params->set_filter(GetPermissiveCapabilityFilter());
  ConnectToApplication(params.Pass());
  MessagePipe pipe;
  services->ConnectToService(interface_name, pipe.handle1.Pass());
  return pipe.handle0.Pass();
}

Shell::ConnectToApplicationCallback EmptyConnectCallback() {
  return base::Bind(&OnEmptyOnConnectCallback);
}

}  // namespace shell
}  // namespace mojo
