// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/metrics/metrics_service.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cert/cert_database.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/url_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/network/dhcp_pac_file_fetcher_factory_chromeos.h"
#include "services/network/cert_verify_proc_chromeos.h"
#endif

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

#if defined(OS_MACOSX)
void ObserveKeychainEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::CertDatabase::GetInstance()->StartListeningForKeychainEvents();
}
#endif

}  // namespace

IOThread::Globals::Globals() {}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    net_log::ChromeNetLog* net_log,
    SystemNetworkContextManager* system_network_context_manager)
    : net_log_(net_log),
      globals_(nullptr),
      is_quic_allowed_on_init_(true),
      weak_factory_(this) {
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  BrowserThread::SetIOThreadDelegate(this);

  system_network_context_manager->SetUp(
      &network_context_request_, &network_context_params_,
      &stub_resolver_enabled_, &dns_over_https_servers_,
      &http_auth_static_params_, &http_auth_dynamic_params_,
      &is_quic_allowed_on_init_);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetIOThreadDelegate(nullptr);

  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return globals_;
}

net_log::ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

void IOThread::Init() {
  TRACE_EVENT0("startup", "IOThread::InitAsync");
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!globals_);
  globals_ = new Globals;


#if defined(OS_MACOSX)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&ObserveKeychainEvents));
#endif

  ConstructSystemRequestContext();
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  delete globals_;
  globals_ = nullptr;

  if (net_log_)
    net_log_->ShutDownBeforeThreadPool();
}

// static
void IOThread::RegisterPrefs(PrefRegistrySimple* registry) {
  data_reduction_proxy::RegisterPrefs(registry);
}

void IOThread::DisableQuic() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  globals_->quic_disabled = true;
}

void IOThread::ConstructSystemRequestContext() {
  globals_->deprecated_network_quality_estimator =
      std::make_unique<net::NetworkQualityEstimator>(
          std::make_unique<net::NetworkQualityEstimatorParams>(
              std::map<std::string, std::string>()),
          net_log_);
}
