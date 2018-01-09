// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_service_test_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/network/network_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/interfaces/network_change_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_ANDROID)
#include "base/test/android/url_utils.h"
#include "base/test/test_support_android.h"
#endif

namespace content {

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public mojom::NetworkServiceTest {
 public:
  NetworkServiceTestImpl() {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseMockCertVerifierForTesting)) {
      mock_cert_verifier_ = std::make_unique<net::MockCertVerifier>();
      NetworkContext::SetCertVerifierForTesting(mock_cert_verifier_.get());
    }

    // This class is constructed in all test processes but we only want to
    // add it in the utility process where the network service is run, to avoid
    // clashing with the other HostResolver in browsertests.
    if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType) == switches::kUtilityProcess) {
      test_host_resolver_ = std::make_unique<TestHostResolver>();
    }
  }

  ~NetworkServiceTestImpl() override {
    NetworkContext::SetCertVerifierForTesting(nullptr);
  }

  // mojom::NetworkServiceTest:
  void AddRules(std::vector<mojom::RulePtr> rules,
                AddRulesCallback callback) override {
    for (const auto& rule : rules) {
      test_host_resolver_->host_resolver()->AddRule(rule->host_pattern,
                                                    rule->replacement);
    }
    std::move(callback).Run();
  }

  void SimulateNetworkChange(network::mojom::ConnectionType type,
                             SimulateNetworkChangeCallback callback) override {
    DCHECK(net::NetworkChangeNotifier::HasNetworkChangeNotifier());
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::ConnectionType(type));
    std::move(callback).Run();
  }

  void SimulateCrash() override {
    LOG(ERROR) << "Intentionally issuing kill signal to current process to"
               << " simulate NetworkService crash for testing.";
    // Use |Process::Terminate()| instead of |CHECK()| to avoid 'Fatal error'
    // dialog on Windows debug.
    base::Process::Current().Terminate(1, false);
  }

  void MockCertVerifierSetDefaultResult(
      int32_t default_result,
      MockCertVerifierSetDefaultResultCallback callback) override {
    mock_cert_verifier_->set_default_result(default_result);
    std::move(callback).Run();
  }

  void MockCertVerifierAddResultForCertAndHost(
      const scoped_refptr<net::X509Certificate>& cert,
      const std::string& host_pattern,
      const net::CertVerifyResult& verify_result,
      int32_t rv,
      MockCertVerifierAddResultForCertAndHostCallback callback) override {
    mock_cert_verifier_->AddResultForCertAndHost(cert, host_pattern,
                                                 verify_result, rv);
    std::move(callback).Run();
  }

  void BindRequest(mojom::NetworkServiceTestRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<mojom::NetworkServiceTest> bindings_;
  std::unique_ptr<TestHostResolver> test_host_resolver_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestImpl);
};

NetworkServiceTestHelper::NetworkServiceTestHelper()
    : network_service_test_impl_(new NetworkServiceTestImpl) {}

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;

  registry->AddInterface(
      base::Bind(&NetworkServiceTestHelper::BindNetworkServiceTestRequest,
                 base::Unretained(this)));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  service_manager::SandboxType sandbox_type =
      service_manager::SandboxTypeFromCommandLine(*command_line);
  if (IsUnsandboxedSandboxType(sandbox_type) ||
      sandbox_type == service_manager::SANDBOX_TYPE_NETWORK) {
    // Register the EmbeddedTestServer's certs, so that any SSL connections to
    // it succeed. Only do this when file I/O is allowed in the current process.
#if defined(OS_ANDROID)
    base::InitAndroidTestPaths(base::android::GetIsolatedTestRoot());
#endif
    net::EmbeddedTestServer::RegisterTestCerts();

    // Also add the QUIC test certificate.
    net::TestRootCerts* root_certs = net::TestRootCerts::GetInstance();
    root_certs->AddFromFile(
        net::GetTestCertsDirectory().AppendASCII("quic-root.pem"));
  }
}

void NetworkServiceTestHelper::BindNetworkServiceTestRequest(
    mojom::NetworkServiceTestRequest request) {
  network_service_test_impl_->BindRequest(std::move(request));
}

}  // namespace content
