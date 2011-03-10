// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connection_tester.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/firefox_proxy_settings.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace {

// ExperimentURLRequestContext ------------------------------------------------

// An instance of ExperimentURLRequestContext is created for each experiment
// run by ConnectionTester. The class initializes network dependencies according
// to the specified "experiment".
class ExperimentURLRequestContext : public net::URLRequestContext {
 public:
  explicit ExperimentURLRequestContext(
      net::URLRequestContext* proxy_request_context)
      : proxy_request_context_(proxy_request_context) {}

  int Init(const ConnectionTester::Experiment& experiment) {
    int rv;

    // Create a custom HostResolver for this experiment.
    net::HostResolver* host_resolver_tmp = NULL;
    rv = CreateHostResolver(experiment.host_resolver_experiment,
                            &host_resolver_tmp);
    if (rv != net::OK)
      return rv;  // Failure.
    set_host_resolver(host_resolver_tmp);

    // Create a custom ProxyService for this this experiment.
    scoped_refptr<net::ProxyService> proxy_service_tmp = NULL;
    rv = CreateProxyService(experiment.proxy_settings_experiment,
                            &proxy_service_tmp);
    if (rv != net::OK)
      return rv;  // Failure.
    set_proxy_service(proxy_service_tmp);

    // The rest of the dependencies are standard, and don't depend on the
    // experiment being run.
    set_cert_verifier(new net::CertVerifier);
    set_dnsrr_resolver(new net::DnsRRResolver);
    set_ftp_transaction_factory(new net::FtpNetworkLayer(host_resolver_tmp));
    set_ssl_config_service(new net::SSLConfigServiceDefaults);
    set_http_auth_handler_factory(net::HttpAuthHandlerFactory::CreateDefault(
        host_resolver_tmp));

    net::HttpNetworkSession::Params session_params;
    session_params.host_resolver = host_resolver_tmp;
    session_params.dnsrr_resolver = dnsrr_resolver();
    session_params.cert_verifier = cert_verifier();
    session_params.proxy_service = proxy_service_tmp;
    session_params.http_auth_handler_factory = http_auth_handler_factory();
    session_params.ssl_config_service = ssl_config_service();
    scoped_refptr<net::HttpNetworkSession> network_session(
        new net::HttpNetworkSession(session_params));
    set_http_transaction_factory(new net::HttpCache(
        network_session,
        net::HttpCache::DefaultBackend::InMemory(0)));
    // In-memory cookie store.
    set_cookie_store(new net::CookieMonster(NULL, NULL));

    return net::OK;
  }

 protected:
  virtual ~ExperimentURLRequestContext() {
    delete ftp_transaction_factory();
    delete http_transaction_factory();
    delete http_auth_handler_factory();
    delete dnsrr_resolver();
    delete cert_verifier();
    delete host_resolver();
  }

 private:
  // Creates a host resolver for |experiment|. On success returns net::OK and
  // fills |host_resolver| with a new pointer. Otherwise returns a network
  // error code.
  int CreateHostResolver(
      ConnectionTester::HostResolverExperiment experiment,
      net::HostResolver** host_resolver) {
    // Create a vanilla HostResolver that disables caching.
    const size_t kMaxJobs = 50u;
    net::HostResolverImpl* impl =
        new net::HostResolverImpl(NULL, NULL, kMaxJobs, NULL);

    *host_resolver = impl;

    // Modify it slightly based on the experiment being run.
    switch (experiment) {
      case ConnectionTester::HOST_RESOLVER_EXPERIMENT_PLAIN:
        return net::OK;
      case ConnectionTester::HOST_RESOLVER_EXPERIMENT_DISABLE_IPV6:
        impl->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
        return net::OK;
      case ConnectionTester::HOST_RESOLVER_EXPERIMENT_IPV6_PROBE: {
        // Note that we don't use impl->ProbeIPv6Support() since that finishes
        // asynchronously and may not take effect in time for the test.
        // So instead we will probe synchronously (might take 100-200 ms).
        net::AddressFamily family = net::IPv6Supported() ?
            net::ADDRESS_FAMILY_UNSPECIFIED : net::ADDRESS_FAMILY_IPV4;
        impl->SetDefaultAddressFamily(family);
        return net::OK;
      }
      default:
        NOTREACHED();
        return net::ERR_UNEXPECTED;
    }
  }

  // Creates a proxy config service for |experiment|. On success returns net::OK
  // and fills |config_service| with a new pointer. Otherwise returns a network
  // error code.
  int CreateProxyConfigService(
      ConnectionTester::ProxySettingsExperiment experiment,
      scoped_ptr<net::ProxyConfigService>* config_service) {
    scoped_ptr<base::ThreadRestrictions::ScopedAllowIO> allow_io;
    switch (experiment) {
      case ConnectionTester::PROXY_EXPERIMENT_USE_SYSTEM_SETTINGS:
        return CreateSystemProxyConfigService(config_service);
      case ConnectionTester::PROXY_EXPERIMENT_USE_FIREFOX_SETTINGS:
        // http://crbug.com/67664: This call can lead to blocking IO on the IO
        // thread.  This is a bug and should be fixed.
        allow_io.reset(new base::ThreadRestrictions::ScopedAllowIO);
        return CreateFirefoxProxyConfigService(config_service);
      case ConnectionTester::PROXY_EXPERIMENT_USE_AUTO_DETECT:
        config_service->reset(new net::ProxyConfigServiceFixed(
            net::ProxyConfig::CreateAutoDetect()));
        return net::OK;
      case ConnectionTester::PROXY_EXPERIMENT_USE_DIRECT:
        config_service->reset(new net::ProxyConfigServiceFixed(
            net::ProxyConfig::CreateDirect()));
        return net::OK;
      default:
        NOTREACHED();
        return net::ERR_UNEXPECTED;
    }
  }

  // Creates a proxy service for |experiment|. On success returns net::OK
  // and fills |config_service| with a new pointer. Otherwise returns a network
  // error code.
  int CreateProxyService(
      ConnectionTester::ProxySettingsExperiment experiment,
      scoped_refptr<net::ProxyService>* proxy_service) {
    // Create an appropriate proxy config service.
    scoped_ptr<net::ProxyConfigService> config_service;
    int rv = CreateProxyConfigService(experiment, &config_service);
    if (rv != net::OK)
      return rv;  // Failure.

    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSingleProcess)) {
      // We can't create a standard proxy resolver in single-process mode.
      // Rather than falling-back to some other implementation, fail.
      return net::ERR_NOT_IMPLEMENTED;
    }

    *proxy_service = net::ProxyService::CreateUsingV8ProxyResolver(
        config_service.release(),
        0u,
        new net::ProxyScriptFetcherImpl(proxy_request_context_),
        host_resolver(),
        NULL);

    return net::OK;
  }

  // Creates a proxy config service that pulls from the system proxy settings.
  // On success returns net::OK and fills |config_service| with a new pointer.
  // Otherwise returns a network error code.
  int CreateSystemProxyConfigService(
      scoped_ptr<net::ProxyConfigService>* config_service) {
#if defined(OS_LINUX)
    // TODO(eroman): This is not supported on Linux yet, because of how
    // construction needs ot happen on the UI thread.
    return net::ERR_NOT_IMPLEMENTED;
#else
    config_service->reset(
        net::ProxyService::CreateSystemProxyConfigService(
            MessageLoop::current(), NULL));
    return net::OK;
#endif
  }

  // Creates a fixed proxy config service that is initialized using Firefox's
  // current proxy settings. On success returns net::OK and fills
  // |config_service| with a new pointer. Otherwise returns a network error
  // code.
  int CreateFirefoxProxyConfigService(
      scoped_ptr<net::ProxyConfigService>* config_service) {
    // Fetch Firefox's proxy settings (can fail if Firefox is not installed).
    FirefoxProxySettings firefox_settings;
    if (!FirefoxProxySettings::GetSettings(&firefox_settings))
      return net::ERR_FILE_NOT_FOUND;

    if (FirefoxProxySettings::SYSTEM == firefox_settings.config_type())
      return CreateSystemProxyConfigService(config_service);

    net::ProxyConfig config;
    if (firefox_settings.ToProxyConfig(&config)) {
      config_service->reset(new net::ProxyConfigServiceFixed(config));
      return net::OK;
    }

    return net::ERR_FAILED;
  }

  const scoped_refptr<net::URLRequestContext> proxy_request_context_;
};

}  // namespace

// ConnectionTester::TestRunner ----------------------------------------------

// TestRunner is a helper class for running an individual experiment. It can
// be deleted any time after it is started, and this will abort the request.
class ConnectionTester::TestRunner : public net::URLRequest::Delegate {
 public:
  // |tester| must remain alive throughout the TestRunner's lifetime.
  // |tester| will be notified of completion.
  explicit TestRunner(ConnectionTester* tester)
      : tester_(tester),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

  // Starts running |experiment|. Notifies tester->OnExperimentCompleted() when
  // it is done.
  void Run(const Experiment& experiment);

  // Overridden from net::URLRequest::Delegate:
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);
  // TODO(eroman): handle cases requiring authentication.

 private:
  // The number of bytes to read each response body chunk.
  static const int kReadBufferSize = 1024;

  // Starts reading the response's body (and keeps reading until an error or
  // end of stream).
  void ReadBody(net::URLRequest* request);

  // Called when the request has completed (for both success and failure).
  void OnResponseCompleted(net::URLRequest* request);
  void OnExperimentCompletedWithResult(int result);

  ConnectionTester* tester_;
  scoped_ptr<net::URLRequest> request_;

  ScopedRunnableMethodFactory<TestRunner> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

void ConnectionTester::TestRunner::OnResponseStarted(net::URLRequest* request) {
  if (!request->status().is_success()) {
    OnResponseCompleted(request);
    return;
  }

  // Start reading the body.
  ReadBody(request);
}

void ConnectionTester::TestRunner::OnReadCompleted(net::URLRequest* request,
                                                   int bytes_read) {
  if (bytes_read <= 0) {
    OnResponseCompleted(request);
    return;
  }

  // Keep reading until the stream is closed. Throw the data read away.
  ReadBody(request);
}

void ConnectionTester::TestRunner::ReadBody(net::URLRequest* request) {
  // Read the response body |kReadBufferSize| bytes at a time.
  scoped_refptr<net::IOBuffer> unused_buffer(
      new net::IOBuffer(kReadBufferSize));
  int num_bytes;
  if (request->Read(unused_buffer, kReadBufferSize, &num_bytes)) {
    OnReadCompleted(request, num_bytes);
  } else if (!request->status().is_io_pending()) {
    // Read failed synchronously.
    OnResponseCompleted(request);
  }
}

void ConnectionTester::TestRunner::OnResponseCompleted(
    net::URLRequest* request) {
  int result = net::OK;
  if (!request->status().is_success()) {
    DCHECK_NE(net::ERR_IO_PENDING, request->status().os_error());
    result = request->status().os_error();
  }

  // Post a task to notify the parent rather than handling it right away,
  // to avoid re-entrancy problems with URLRequest. (Don't want the caller
  // to end up deleting the URLRequest while in the middle of processing).
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &TestRunner::OnExperimentCompletedWithResult, result));
}

void ConnectionTester::TestRunner::OnExperimentCompletedWithResult(int result) {
  tester_->OnExperimentCompleted(result);
}

void ConnectionTester::TestRunner::Run(const Experiment& experiment) {
  // Try to create a net::URLRequestContext for this experiment.
  scoped_refptr<ExperimentURLRequestContext> context(
      new ExperimentURLRequestContext(tester_->proxy_request_context_));
  int rv = context->Init(experiment);
  if (rv != net::OK) {
    // Complete the experiment with a failure.
    tester_->OnExperimentCompleted(rv);
    return;
  }

  // Fetch a request using the experimental context.
  request_.reset(new net::URLRequest(experiment.url, this));
  request_->set_context(context);
  request_->Start();
}

// ConnectionTester ----------------------------------------------------------

ConnectionTester::ConnectionTester(
    Delegate* delegate,
    net::URLRequestContext* proxy_request_context)
    : delegate_(delegate), proxy_request_context_(proxy_request_context) {
  DCHECK(delegate);
  DCHECK(proxy_request_context);
}

ConnectionTester::~ConnectionTester() {
  // Cancellation happens automatically by deleting test_runner_.
}

void ConnectionTester::RunAllTests(const GURL& url) {
  // Select all possible experiments to run. (In no particular order).
  // It is possible that some of these experiments are actually duplicates.
  GetAllPossibleExperimentCombinations(url, &remaining_experiments_);

  delegate_->OnStartConnectionTestSuite();
  StartNextExperiment();
}

// static
string16 ConnectionTester::ProxySettingsExperimentDescription(
    ProxySettingsExperiment experiment) {
  // TODO(eroman): Use proper string resources.
  switch (experiment) {
    case PROXY_EXPERIMENT_USE_DIRECT:
      return ASCIIToUTF16("Don't use any proxy");
    case PROXY_EXPERIMENT_USE_SYSTEM_SETTINGS:
      return ASCIIToUTF16("Use system proxy settings");
    case PROXY_EXPERIMENT_USE_FIREFOX_SETTINGS:
      return ASCIIToUTF16("Use Firefox's proxy settings");
    case PROXY_EXPERIMENT_USE_AUTO_DETECT:
      return ASCIIToUTF16("Auto-detect proxy settings");
    default:
      NOTREACHED();
      return string16();
  }
}

// static
string16 ConnectionTester::HostResolverExperimentDescription(
    HostResolverExperiment experiment) {
  // TODO(eroman): Use proper string resources.
  switch (experiment) {
    case HOST_RESOLVER_EXPERIMENT_PLAIN:
      return string16();
    case HOST_RESOLVER_EXPERIMENT_DISABLE_IPV6:
      return ASCIIToUTF16("Disable IPv6 host resolving");
    case HOST_RESOLVER_EXPERIMENT_IPV6_PROBE:
      return ASCIIToUTF16("Probe for IPv6 host resolving");
    default:
      NOTREACHED();
      return string16();
  }
}

// static
void ConnectionTester::GetAllPossibleExperimentCombinations(
    const GURL& url,
    ConnectionTester::ExperimentList* list) {
  list->clear();
  for (size_t resolver_experiment = 0;
       resolver_experiment < HOST_RESOLVER_EXPERIMENT_COUNT;
       ++resolver_experiment) {
    for (size_t proxy_experiment = 0;
         proxy_experiment < PROXY_EXPERIMENT_COUNT;
         ++proxy_experiment) {
      Experiment experiment(
          url,
          static_cast<ProxySettingsExperiment>(proxy_experiment),
          static_cast<HostResolverExperiment>(resolver_experiment));
      list->push_back(experiment);
    }
  }
}

void ConnectionTester::StartNextExperiment() {
  DCHECK(!remaining_experiments_.empty());
  DCHECK(!current_test_runner_.get());

  delegate_->OnStartConnectionTestExperiment(current_experiment());

  current_test_runner_.reset(new TestRunner(this));
  current_test_runner_->Run(current_experiment());
}

void ConnectionTester::OnExperimentCompleted(int result) {
  Experiment current = current_experiment();

  // Advance to the next experiment.
  remaining_experiments_.erase(remaining_experiments_.begin());
  current_test_runner_.reset();

  // Notify the delegate of completion.
  delegate_->OnCompletedConnectionTestExperiment(current, result);

  if (remaining_experiments_.empty()) {
    delegate_->OnCompletedConnectionTestSuite();
  } else {
    StartNextExperiment();
  }
}
