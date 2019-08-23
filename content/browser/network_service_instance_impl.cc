// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_instance_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/network_service_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "net/log/net_log_util.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

#if defined(OS_POSIX)
// Environment variable pointing to credential cache file.
constexpr char kKrb5CCEnvName[] = "KRB5CCNAME";
// Environment variable pointing to Kerberos config file.
constexpr char kKrb5ConfEnvName[] = "KRB5_CONFIG";
#endif

bool g_force_create_network_service_directly = false;
network::mojom::NetworkServicePtr* g_network_service_ptr = nullptr;
network::NetworkConnectionTracker* g_network_connection_tracker;
bool g_network_service_is_responding = false;
base::Time g_last_network_service_crash;

std::deque<std::pair<std::string, base::Time>>& GetDebugEvents() {
  static base::NoDestructor<std::deque<std::pair<std::string, base::Time>>>
      debug_events;
  return *debug_events;
}

std::unique_ptr<network::NetworkService>& GetLocalNetworkService() {
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<network::NetworkService>>>
      service;
  return service->GetOrCreateValue();
}

network::mojom::NetworkServiceParamsPtr CreateNetworkServiceParams() {
  network::mojom::NetworkServiceParamsPtr network_service_params =
      network::mojom::NetworkServiceParams::New();
  network_service_params->initial_connection_type =
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType());
  network_service_params->initial_connection_subtype =
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype());

#if defined(OS_POSIX)
  // Send Kerberos environment variables to the network service.
  if (IsOutOfProcessNetworkService()) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    if (env->HasVar(kKrb5CCEnvName)) {
      env->GetVar(kKrb5CCEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5CCEnvName, value));
    }
    if (env->HasVar(kKrb5ConfEnvName)) {
      env->GetVar(kKrb5ConfEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5ConfEnvName, value));
    }
  }
#endif
  return network_service_params;
}

void CreateNetworkServiceOnIOForTesting(
    network::mojom::NetworkServiceRequest request,
    base::WaitableEvent* completion_event) {
  if (GetLocalNetworkService()) {
    GetLocalNetworkService()->Bind(std::move(request));
    return;
  }

  GetLocalNetworkService() =
      std::make_unique<network::NetworkService>(nullptr, std::move(request));
  if (completion_event)
    completion_event->Signal();
}

void BindNetworkChangeManagerRequest(
    network::mojom::NetworkChangeManagerRequest request) {
  GetNetworkService()->GetNetworkChangeManager(std::move(request));
}

base::CallbackList<void()>& GetCrashHandlersList() {
  static base::NoDestructor<base::CallbackList<void()>> s_list;
  return *s_list;
}

void OnNetworkServiceCrash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_network_service_ptr);
  DCHECK(g_network_service_ptr->is_bound());
  DCHECK(g_network_service_ptr->encountered_error());
  g_last_network_service_crash = base::Time::Now();
  GetCrashHandlersList().Notify();
  AddNetworkServiceDebugEvent("ONSC");
}

// Parses the desired granularity of NetLog capturing specified by the command
// line.
net::NetLogCaptureMode GetNetCaptureModeFromCommandLine(
    const base::CommandLine& command_line) {
  base::StringPiece switch_name = network::switches::kNetLogCaptureMode;

  if (command_line.HasSwitch(switch_name)) {
    std::string value = command_line.GetSwitchValueASCII(switch_name);

    if (value == "Default")
      return net::NetLogCaptureMode::kDefault;
    if (value == "IncludeSensitive")
      return net::NetLogCaptureMode::kIncludeSensitive;
    if (value == "Everything")
      return net::NetLogCaptureMode::kEverything;

    // Warn when using the old command line switches.
    if (value == "IncludeCookiesAndCredentials") {
      LOG(ERROR) << "Deprecated value for --" << switch_name
                 << ". Use IncludeSensitive instead";
      return net::NetLogCaptureMode::kIncludeSensitive;
    }
    if (value == "IncludeSocketBytes") {
      LOG(ERROR) << "Deprecated value for --" << switch_name
                 << ". Use Everything instead";
      return net::NetLogCaptureMode::kEverything;
    }

    LOG(ERROR) << "Unrecognized value for --" << switch_name;
  }

  return net::NetLogCaptureMode::kDefault;
}

}  // namespace

network::mojom::NetworkService* GetNetworkService() {
  if (!g_network_service_ptr)
    g_network_service_ptr = new network::mojom::NetworkServicePtr;
  static NetworkServiceClient* g_client;
  if (!g_network_service_ptr->is_bound() ||
      g_network_service_ptr->encountered_error()) {
    if (GetContentClient()->browser()->IsShuttingDown()) {
      // This happens at system shutdown, since in other scenarios the network
      // process would only be torn down once the message loop stopped running.
      // We don't want to want to start the network service again so just create
      // message pipe that's not bound to stop consumers from requesting
      // creation of the service.
      auto request = mojo::MakeRequest(g_network_service_ptr);
      auto leaked_pipe = request.PassMessagePipe().release();
    } else {
      if (GetSystemConnector() &&  // null in unit tests.
          !g_force_create_network_service_directly) {
        GetSystemConnector()->BindInterface(mojom::kNetworkServiceName,
                                            g_network_service_ptr);
        g_network_service_ptr->set_connection_error_handler(
            base::BindOnce(&OnNetworkServiceCrash));
      } else {
        // This should only be reached in unit tests.
        if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
          CreateNetworkServiceOnIOForTesting(
              mojo::MakeRequest(g_network_service_ptr),
              /*completion_event=*/nullptr);
        } else {
          base::WaitableEvent event;
          base::PostTaskWithTraits(
              FROM_HERE, {BrowserThread::IO},
              base::BindOnce(CreateNetworkServiceOnIOForTesting,
                             mojo::MakeRequest(g_network_service_ptr),
                             base::Unretained(&event)));
          event.Wait();
        }
      }

      AddNetworkServiceDebugEvent("START");
      network::mojom::NetworkServiceClientPtr client_ptr;
      auto client_request = mojo::MakeRequest(&client_ptr);
      // Call SetClient before creating NetworkServiceClient, as the latter
      // might make requests to NetworkService that depend on initialization.
      (*g_network_service_ptr)
          ->SetClient(std::move(client_ptr), CreateNetworkServiceParams());
      g_network_service_is_responding = false;
      g_network_service_ptr->QueryVersion(base::BindRepeating(
          [](base::Time start_time, uint32_t) {
            AddNetworkServiceDebugEvent("RESP");
            g_network_service_is_responding = true;
            base::TimeDelta delta = base::Time::Now() - start_time;
            UMA_HISTOGRAM_MEDIUM_TIMES("NetworkService.TimeToFirstResponse",
                                       delta);
            if (g_last_network_service_crash.is_null()) {
              UMA_HISTOGRAM_MEDIUM_TIMES(
                  "NetworkService.TimeToFirstResponse.OnStartup", delta);
            } else {
              UMA_HISTOGRAM_MEDIUM_TIMES(
                  "NetworkService.TimeToFirstResponse.AfterCrash", delta);
            }
          },
          base::Time::Now()));

      delete g_client;  // In case we're recreating the network service.
      g_client = new NetworkServiceClient(std::move(client_request));

      const base::CommandLine* command_line =
          base::CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(network::switches::kLogNetLog)) {
        base::FilePath log_path =
            command_line->GetSwitchValuePath(network::switches::kLogNetLog);

        base::DictionaryValue client_constants =
            GetContentClient()->GetNetLogConstants();

        base::File file(
            log_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
        if (!file.IsValid()) {
          LOG(ERROR) << "Failed opening NetLog: " << log_path.value();
        } else {
          (*g_network_service_ptr)
              ->StartNetLog(std::move(file),
                            GetNetCaptureModeFromCommandLine(*command_line),
                            std::move(client_constants));
        }
      }

      base::FilePath ssl_key_log_path;
      if (command_line->HasSwitch(network::switches::kSSLKeyLogFile)) {
        UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                  SSLKeyLogFileAction::kSwitchFound);
        ssl_key_log_path =
            command_line->GetSwitchValuePath(network::switches::kSSLKeyLogFile);
        LOG_IF(WARNING, ssl_key_log_path.empty())
            << "ssl-key-log-file argument missing";
      } else {
        std::unique_ptr<base::Environment> env(base::Environment::Create());
        std::string env_str;
        if (env->GetVar("SSLKEYLOGFILE", &env_str)) {
          UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                    SSLKeyLogFileAction::kEnvVarFound);
#if defined(OS_WIN)
          // base::Environment returns environment variables in UTF-8 on
          // Windows.
          ssl_key_log_path = base::FilePath(base::UTF8ToUTF16(env_str));
#else
          ssl_key_log_path = base::FilePath(env_str);
#endif
        }
      }

      if (!ssl_key_log_path.empty()) {
        base::File file(ssl_key_log_path,
                        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
        if (!file.IsValid()) {
          LOG(ERROR) << "Failed opening SSL key log file: "
                     << ssl_key_log_path.value();
        } else {
          UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                    SSLKeyLogFileAction::kLogFileEnabled);
          (*g_network_service_ptr)->SetSSLKeyLogFile(std::move(file));
        }
      }

      GetContentClient()->browser()->OnNetworkServiceCreated(
          g_network_service_ptr->get());
    }
  }
  return g_network_service_ptr->get();
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
RegisterNetworkServiceCrashHandler(base::RepeatingClosure handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!handler.is_null());

  return GetCrashHandlersList().Add(std::move(handler));
}

#if defined(OS_CHROMEOS)
net::NetworkChangeNotifier* GetNetworkChangeNotifier() {
  return BrowserMainLoop::GetInstance()->network_change_notifier();
}
#endif

void FlushNetworkServiceInstanceForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (g_network_service_ptr)
    g_network_service_ptr->FlushForTesting();
}

network::NetworkConnectionTracker* GetNetworkConnectionTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  if (!g_network_connection_tracker) {
    g_network_connection_tracker = new network::NetworkConnectionTracker(
        base::BindRepeating(&BindNetworkChangeManagerRequest));
  }
  return g_network_connection_tracker;
}

void GetNetworkConnectionTrackerFromUIThread(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  // TODO(fdoray): Investigate why this is needed. The IO thread is supposed to
  // be initialized by the time the UI thread starts running tasks.
  //
  // GetNetworkConnectionTracker() will call CreateNetworkServiceOnIO(). Here it
  // makes sure the IO thread is running when CreateNetworkServiceOnIO() is
  // called.
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::IO)) {
    // IO thread is not yet initialized. Try again in the next message pump.
    bool task_posted = base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&GetNetworkConnectionTrackerFromUIThread,
                                  std::move(callback)));
    DCHECK(task_posted);
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetNetworkConnectionTracker), std::move(callback));
}

network::NetworkConnectionTrackerAsyncGetter
CreateNetworkConnectionTrackerAsyncGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTrackerFromUIThread);
}

void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker) {
  if (g_network_connection_tracker != network_connection_tracker) {
    DCHECK(!g_network_connection_tracker || !network_connection_tracker);
    g_network_connection_tracker = network_connection_tracker;
  }
}

scoped_refptr<base::DeferredSequencedTaskRunner> GetNetworkTaskRunner() {
  DCHECK(IsInProcessNetworkService());
  static base::NoDestructor<scoped_refptr<base::DeferredSequencedTaskRunner>>
      instance(new base::DeferredSequencedTaskRunner());
  return instance->get();
}

void ForceCreateNetworkServiceDirectlyForTesting() {
  g_force_create_network_service_directly = true;
}

void ResetNetworkServiceForTesting() {
  delete g_network_service_ptr;
  g_network_service_ptr = nullptr;
}

NetworkServiceAvailability GetNetworkServiceAvailability() {
  if (!g_network_service_ptr)
    return NetworkServiceAvailability::NOT_CREATED;
  else if (!g_network_service_ptr->is_bound())
    return NetworkServiceAvailability::NOT_BOUND;
  else if (g_network_service_ptr->encountered_error())
    return NetworkServiceAvailability::ENCOUNTERED_ERROR;
  else if (!g_network_service_is_responding)
    return NetworkServiceAvailability::NOT_RESPONDING;
  else
    return NetworkServiceAvailability::AVAILABLE;
}

base::TimeDelta GetTimeSinceLastNetworkServiceCrash() {
  if (g_last_network_service_crash.is_null())
    return base::TimeDelta();
  return base::Time::Now() - g_last_network_service_crash;
}

void PingNetworkService(base::OnceClosure closure) {
  GetNetworkService();
  // Unfortunately, QueryVersion requires a RepeatingCallback.
  g_network_service_ptr->QueryVersion(base::BindRepeating(
      [](base::OnceClosure closure, uint32_t) {
        if (closure)
          std::move(closure).Run();
      },
      base::Passed(std::move(closure))));
}

void AddNetworkServiceDebugEvent(const std::string& event) {
  auto& events = GetDebugEvents();
  events.push_front({event, base::Time::Now()});
  // Keep at most 20 most recent events.
  if (events.size() > 20)
    events.pop_back();
}

std::string GetNetworkServiceDebugEventsString() {
  auto& events = GetDebugEvents();
  if (events.empty())
    return std::string();
  std::stringstream stream;
  base::Time now = base::Time::Now();
  for (const auto& info : events) {
    stream << info.first << ":" << (now - info.second).InSecondsF() << ",";
  }
  return stream.str();
}

}  // namespace content
