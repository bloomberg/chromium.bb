// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_shell_connection_impl.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_checker.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "content/public/common/connection_filter.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/runner/common/client_util.h"

namespace content {
namespace {

base::LazyInstance<std::unique_ptr<MojoShellConnection>>::Leaky
    g_connection_for_process = LAZY_INSTANCE_INITIALIZER;

MojoShellConnection::Factory* mojo_shell_connection_factory = nullptr;

}  // namespace

// A ref-counted object which owns the IO thread state of a
// MojoShellConnectionImpl. This includes Service and ServiceFactory
// bindings.
class MojoShellConnectionImpl::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public shell::Service,
      public shell::InterfaceFactory<shell::mojom::ServiceFactory>,
      public shell::mojom::ServiceFactory {
 public:
  using InitializeCallback = base::Callback<void(const shell::Identity&)>;
  using ServiceFactoryCallback =
      base::Callback<void(shell::mojom::ServiceRequest, const std::string&)>;

  IOThreadContext(shell::mojom::ServiceRequest service_request,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                  std::unique_ptr<shell::Connector> io_thread_connector,
                  shell::mojom::ConnectorRequest connector_request)
      : pending_service_request_(std::move(service_request)),
        io_task_runner_(io_task_runner),
        io_thread_connector_(std::move(io_thread_connector)),
        pending_connector_request_(std::move(connector_request)) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }

  // Safe to call from any thread.
  void Start(const InitializeCallback& initialize_callback,
             const ServiceFactoryCallback& create_service_callback,
             const base::Closure& stop_callback) {
    DCHECK(!started_);

    started_ = true;
    callback_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    initialize_handler_ = initialize_callback;
    create_service_callback_ = create_service_callback;
    stop_callback_ = stop_callback;
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOThreadContext::StartOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  // Safe to call any time before a message is received from a process.
  // i.e. can be called when starting the process but not afterwards.
  void AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) {
    base::AutoLock lock(lock_);
    connection_filters_.emplace_back(std::move(filter));
  }

  std::unique_ptr<ConnectionFilter> RemoveConnectionFilter(
      ConnectionFilter* filter) {
    base::AutoLock lock(lock_);
    for (auto it = connection_filters_.begin(); it != connection_filters_.end();
         ++it) {
      if (it->get() == filter) {
        std::unique_ptr<ConnectionFilter> taken = std::move(*it);
        connection_filters_.erase(it);
        return taken;
      }
    }
    return nullptr;
  }

  // Safe to call any time before Start() is called.
  void SetDefaultBinderForBrowserConnection(
      const shell::InterfaceRegistry::Binder& binder) {
    DCHECK(!started_);
    default_browser_binder_ = base::Bind(
        &IOThreadContext::CallBinderOnTaskRunner,
        base::ThreadTaskRunnerHandle::Get(), binder);
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  ~IOThreadContext() override {}

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_context_.reset(new shell::ServiceContext(
        this, std::move(pending_service_request_),
        std::move(io_thread_connector_),
        std::move(pending_connector_request_)));
  }

  void ShutDownOnIOThread() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_context_.reset();
    factory_bindings_.CloseAllBindings();
  }

  void OnBrowserConnectionLost() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    has_browser_connection_ = false;
  }

  /////////////////////////////////////////////////////////////////////////////
  // shell::Service implementation

  void OnStart(const shell::Identity& identity) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    DCHECK(!initialize_handler_.is_null());

    InitializeCallback handler = base::ResetAndReturn(&initialize_handler_);
    callback_task_runner_->PostTask(FROM_HERE, base::Bind(handler, identity));
  }

  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    std::string remote_app = remote_identity.name();
    if (remote_app == "mojo:shell") {
      // Only expose the SCF interface to the shell.
      registry->AddInterface<shell::mojom::ServiceFactory>(this);
      return true;
    }

    bool accept = false;
    for (auto& filter : connection_filters_) {
      accept = accept ||
        filter->OnConnect(remote_identity, registry,
                          service_context_->connector());
    }

    if (remote_identity.name() == "exe:content_browser" &&
        !has_browser_connection_) {
      has_browser_connection_ = true;
      registry->set_default_binder(default_browser_binder_);
      registry->SetConnectionLostClosure(
          base::Bind(&IOThreadContext::OnBrowserConnectionLost, this));
      return true;
    }

    // If no filters were interested, reject the connection.
    return accept;
  }

  bool OnStop() override {
    callback_task_runner_->PostTask(FROM_HERE, stop_callback_);
    return true;
  }

  /////////////////////////////////////////////////////////////////////////////
  // shell::InterfaceFactory<shell::mojom::ServiceFactory> implementation

  void Create(const shell::Identity& remote_identity,
              shell::mojom::ServiceFactoryRequest request) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    factory_bindings_.AddBinding(this, std::move(request));
  }

  /////////////////////////////////////////////////////////////////////////////
  // shell::mojom::ServiceFactory implementation

  void CreateService(shell::mojom::ServiceRequest request,
                     const std::string& name) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    callback_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(create_service_callback_, base::Passed(&request), name));
  }

  static void CallBinderOnTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const shell::InterfaceRegistry::Binder& binder,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle request_handle) {
    task_runner->PostTask(FROM_HERE, base::Bind(binder, interface_name,
                                                base::Passed(&request_handle)));
  }

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  shell::mojom::ServiceRequest pending_service_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<shell::Connector> io_thread_connector_;
  shell::mojom::ConnectorRequest pending_connector_request_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Callback to run once Service::OnStart is invoked.
  InitializeCallback initialize_handler_;

  // Callback to run when a new Service request is received.
  ServiceFactoryCallback create_service_callback_;

  // Callback to run if the service is stopped by the service manager.
  base::Closure stop_callback_;

  // Called once a connection has been received from the browser process & the
  // default binder (below) has been set up.
  bool has_browser_connection_ = false;

  // Default binder callback used for the browser connection's
  // InterfaceRegistry.
  //
  // TODO(rockot): Remove this once all interfaces exposed to the browser are
  // exposed via a ConnectionFilter.
  shell::InterfaceRegistry::Binder default_browser_binder_;

  std::unique_ptr<shell::ServiceContext> service_context_;
  mojo::BindingSet<shell::mojom::ServiceFactory> factory_bindings_;
  std::vector<std::unique_ptr<ConnectionFilter>> connection_filters_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnection, public:

// static
void MojoShellConnection::SetForProcess(
    std::unique_ptr<MojoShellConnection> connection) {
  DCHECK(!g_connection_for_process.Get());
  g_connection_for_process.Get() = std::move(connection);
}

// static
MojoShellConnection* MojoShellConnection::GetForProcess() {
  return g_connection_for_process.Get().get();
}

// static
void MojoShellConnection::DestroyForProcess() {
  // This joins the shell controller thread.
  g_connection_for_process.Get().reset();
}

// static
void MojoShellConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!g_connection_for_process.Get());
  mojo_shell_connection_factory = factory;
}

// static
std::unique_ptr<MojoShellConnection> MojoShellConnection::Create(
    shell::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  if (mojo_shell_connection_factory)
    return mojo_shell_connection_factory->Run();
  return base::MakeUnique<MojoShellConnectionImpl>(
      std::move(request), io_task_runner);
}

MojoShellConnection::~MojoShellConnection() {}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, public:

MojoShellConnectionImpl::MojoShellConnectionImpl(
    shell::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : weak_factory_(this) {
  shell::mojom::ConnectorRequest connector_request;
  connector_ = shell::Connector::Create(&connector_request);

  std::unique_ptr<shell::Connector> io_thread_connector = connector_->Clone();
  context_ = new IOThreadContext(
      std::move(request), io_task_runner, std::move(io_thread_connector),
      std::move(connector_request));
}

MojoShellConnectionImpl::~MojoShellConnectionImpl() {
  context_->ShutDown();
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, MojoShellConnection implementation:

void MojoShellConnectionImpl::Start() {
  context_->Start(
      base::Bind(&MojoShellConnectionImpl::OnContextInitialized,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&MojoShellConnectionImpl::CreateService,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&MojoShellConnectionImpl::OnConnectionLost,
                 weak_factory_.GetWeakPtr()));
}

void MojoShellConnectionImpl::SetInitializeHandler(
    const base::Closure& handler) {
  DCHECK(initialize_handler_.is_null());
  initialize_handler_ = handler;
}

shell::Connector* MojoShellConnectionImpl::GetConnector() {
  return connector_.get();
}

const shell::Identity& MojoShellConnectionImpl::GetIdentity() const {
  return identity_;
}

void MojoShellConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  connection_lost_handler_ = closure;
}

void MojoShellConnectionImpl::SetupInterfaceRequestProxies(
    shell::InterfaceRegistry* registry,
    shell::InterfaceProvider* provider) {
  // It's safe to bind |registry| as a raw pointer because the caller must
  // guarantee that it outlives |this|, and |this| is bound as a weak ptr here.
  context_->SetDefaultBinderForBrowserConnection(
      base::Bind(&MojoShellConnectionImpl::GetInterface,
                 weak_factory_.GetWeakPtr(), registry));

  // TODO(beng): remove provider parameter.
}

void MojoShellConnectionImpl::AddConnectionFilter(
    std::unique_ptr<ConnectionFilter> filter) {
  context_->AddConnectionFilter(std::move(filter));
}

std::unique_ptr<ConnectionFilter>
MojoShellConnectionImpl::RemoveConnectionFilter(ConnectionFilter* filter) {
  return context_->RemoveConnectionFilter(filter);
}

void MojoShellConnectionImpl::AddEmbeddedService(
    const std::string& name,
    const MojoApplicationInfo& info) {
  std::unique_ptr<EmbeddedApplicationRunner> app(
      new EmbeddedApplicationRunner(name, info));
  AddServiceRequestHandler(
      name, base::Bind(&EmbeddedApplicationRunner::BindServiceRequest,
                       base::Unretained(app.get())));
  auto result = embedded_apps_.insert(std::make_pair(name, std::move(app)));
  DCHECK(result.second);
}

void MojoShellConnectionImpl::AddServiceRequestHandler(
    const std::string& name,
    const ServiceRequestHandler& handler) {
  auto result = request_handlers_.insert(std::make_pair(name, handler));
  DCHECK(result.second);
}

void MojoShellConnectionImpl::CreateService(
    shell::mojom::ServiceRequest request,
    const std::string& name) {
  auto it = request_handlers_.find(name);
  if (it != request_handlers_.end())
    it->second.Run(std::move(request));
}

void MojoShellConnectionImpl::OnContextInitialized(
    const shell::Identity& identity) {
  identity_ = identity;
  if (!initialize_handler_.is_null())
    base::ResetAndReturn(&initialize_handler_).Run();
}

void MojoShellConnectionImpl::OnConnectionLost() {
  if (!connection_lost_handler_.is_null())
    connection_lost_handler_.Run();
}

void MojoShellConnectionImpl::GetInterface(
    shell::mojom::InterfaceProvider* provider,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  provider->GetInterface(interface_name, std::move(request_handle));
}

}  // namespace content
