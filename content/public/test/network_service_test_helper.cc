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
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_ANDROID)
#include "base/test/android/url_utils.h"
#include "base/test/test_support_android.h"
#endif

namespace content {

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public network::mojom::NetworkServiceTest {
 public:
  NetworkServiceTestImpl() {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseMockCertVerifierForTesting)) {
      mock_cert_verifier_ = std::make_unique<net::MockCertVerifier>();
      network::NetworkContext::SetCertVerifierForTesting(
          mock_cert_verifier_.get());
    }
  }

  ~NetworkServiceTestImpl() override {
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
  }

  // network::mojom::NetworkServiceTest:
  void AddRules(std::vector<network::mojom::RulePtr> rules,
                AddRulesCallback callback) override {
    for (const auto& rule : rules) {
      test_host_resolver_.host_resolver()->AddRule(rule->host_pattern,
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
    LOG(ERROR) << "Intentionally terminating current process to simulate"
                  " NetworkService crash for testing.";
    // Use |TerminateCurrentProcessImmediately()| instead of |CHECK()| to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
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

  void SetShouldRequireCT(ShouldRequireCT required,
                          SetShouldRequireCTCallback callback) override {
    if (required == NetworkServiceTest::ShouldRequireCT::RESET) {
      net::TransportSecurityState::SetShouldRequireCTForTesting(nullptr);
      std::move(callback).Run();
      return;
    }

    bool ct = true;
    if (NetworkServiceTest::ShouldRequireCT::DONT_REQUIRE == required)
      ct = false;

    net::TransportSecurityState::SetShouldRequireCTForTesting(&ct);
    std::move(callback).Run();
  }

  void BindRequest(network::mojom::NetworkServiceTestRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<network::mojom::NetworkServiceTest> bindings_;
  TestHostResolver test_host_resolver_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestImpl);
};

NetworkServiceTestHelper::NetworkServiceTestHelper()
    : network_service_test_impl_(new NetworkServiceTestImpl) {}

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
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
    network::mojom::NetworkServiceTestRequest request) {
  network_service_test_impl_->BindRequest(std::move(request));
}

}  // namespace content
