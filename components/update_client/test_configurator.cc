// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_configurator.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/patch_service/file_patcher_impl.h"
#include "components/patch_service/patch_service.h"
#include "components/patch_service/public/interfaces/constants.mojom.h"
#include "components/patch_service/public/interfaces/file_patcher.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/unzip_service/public/interfaces/constants.mojom.h"
#include "components/unzip_service/public/interfaces/unzipper.mojom.h"
#include "components/unzip_service/unzip_service.h"
#include "components/unzip_service/unzipper_impl.h"
#include "components/update_client/activity_data_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/url_request/url_request_test_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "url/gurl.h"

namespace update_client {

namespace {

std::vector<GURL> MakeDefaultUrls() {
  std::vector<GURL> urls;
  urls.push_back(GURL(POST_INTERCEPT_SCHEME
                      "://" POST_INTERCEPT_HOSTNAME POST_INTERCEPT_PATH));
  return urls;
}

class TestConnector : public service_manager::mojom::Connector {
 public:
  TestConnector()
      : patch_context_(std::make_unique<patch::PatchService>(),
                       mojo::MakeRequest(&patch_ptr_)),
        unzip_context_(std::make_unique<unzip::UnzipService>(),
                       mojo::MakeRequest(&unzip_ptr_)) {
    const service_manager::Identity service_id("TestConnector");
    patch_ptr_->OnStart(service_id,
                        base::BindOnce(&TestConnector::OnStartCallback,
                                       base::Unretained(this)));
    unzip_ptr_->OnStart(service_id,
                        base::BindOnce(&TestConnector::OnStartCallback,
                                       base::Unretained(this)));
  }

  ~TestConnector() override = default;

  void BindInterface(const service_manager::Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    service_manager::mojom::ServicePtr* service_ptr = nullptr;
    if (interface_name == "patch::mojom::FilePatcher") {
      service_ptr = &patch_ptr_;
    } else if (interface_name == "unzip::mojom::Unzipper") {
      service_ptr = &unzip_ptr_;
    } else {
      LOG(ERROR) << "Requested " << interface_name << " from the TestConnector"
                 << ", but no such instance was bound.";
      return;
    }
    (*service_ptr)
        ->OnBindInterface(service_manager::BindSourceInfo(
                              service_manager::Identity("TestConnector"),
                              service_manager::CapabilitySet()),
                          interface_name, std::move(interface_pipe),
                          base::BindRepeating(&base::DoNothing));
    std::move(callback).Run(service_manager::mojom::ConnectResult::SUCCEEDED,
                            service_manager::Identity());
  }

  void StartService(const service_manager::Identity& target,
                    StartServiceCallback callback) override {
    NOTREACHED();
  }

  void QueryService(const service_manager::Identity& target,
                    QueryServiceCallback callback) override {
    NOTREACHED();
  }

  void StartServiceWithProcess(
      const service_manager::Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      service_manager::mojom::PIDReceiverRequest pid_receiver_request,
      StartServiceWithProcessCallback callback) override {
    NOTREACHED();
  }

  void Clone(service_manager::mojom::ConnectorRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void FilterInterfaces(
      const std::string& spec,
      const service_manager::Identity& source,
      service_manager::mojom::InterfaceProviderRequest source_request,
      service_manager::mojom::InterfaceProviderPtr target) override {
    NOTREACHED();
  }

 private:
  void OnStartCallback(
      service_manager::mojom::ConnectorRequest request,
      service_manager::mojom::ServiceControlAssociatedRequest control_request) {
  }

  service_manager::mojom::ServicePtr patch_ptr_;
  service_manager::mojom::ServicePtr unzip_ptr_;
  service_manager::ServiceContext patch_context_;
  service_manager::ServiceContext unzip_context_;
  mojo::BindingSet<service_manager::mojom::Connector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestConnector);
};

}  // namespace

TestConfigurator::TestConfigurator()
    : brand_("TEST"),
      initial_time_(0),
      ondemand_time_(0),
      enabled_cup_signing_(false),
      enabled_component_updates_(true),
      connector_mojo_(base::MakeUnique<TestConnector>()),
      context_(base::MakeRefCounted<net::TestURLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get())) {
  service_manager::mojom::ConnectorPtr proxy;
  connector_mojo_->Clone(mojo::MakeRequest(&proxy));
  connector_ = base::MakeUnique<service_manager::Connector>(std::move(proxy));
}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() const {
  return initial_time_;
}

int TestConfigurator::NextCheckDelay() const {
  return 1;
}

int TestConfigurator::OnDemandDelay() const {
  return ondemand_time_;
}

int TestConfigurator::UpdateDelay() const {
  return 1;
}

std::vector<GURL> TestConfigurator::UpdateUrl() const {
  if (!update_check_url_.is_empty())
    return std::vector<GURL>(1, update_check_url_);

  return MakeDefaultUrls();
}

std::vector<GURL> TestConfigurator::PingUrl() const {
  if (!ping_url_.is_empty())
    return std::vector<GURL>(1, ping_url_);

  return UpdateUrl();
}

std::string TestConfigurator::GetProdId() const {
  return "fake_prodid";
}

base::Version TestConfigurator::GetBrowserVersion() const {
  // Needs to be larger than the required version in tested component manifests.
  return base::Version("30.0");
}

std::string TestConfigurator::GetChannel() const {
  return "fake_channel_string";
}

std::string TestConfigurator::GetBrand() const {
  return brand_;
}

std::string TestConfigurator::GetLang() const {
  return "fake_lang";
}

std::string TestConfigurator::GetOSLongName() const {
  return "Fake Operating System";
}

std::string TestConfigurator::ExtraRequestParams() const {
  return "extra=\"foo\"";
}

std::string TestConfigurator::GetDownloadPreference() const {
  return download_preference_;
}

scoped_refptr<net::URLRequestContextGetter> TestConfigurator::RequestContext()
    const {
  return context_;
}

std::unique_ptr<service_manager::Connector>
TestConfigurator::CreateServiceManagerConnector() const {
  return connector_->Clone();
}

bool TestConfigurator::EnabledDeltas() const {
  return true;
}

bool TestConfigurator::EnabledComponentUpdates() const {
  return enabled_component_updates_;
}

bool TestConfigurator::EnabledBackgroundDownloader() const {
  return false;
}

bool TestConfigurator::EnabledCupSigning() const {
  return enabled_cup_signing_;
}

void TestConfigurator::SetBrand(const std::string& brand) {
  brand_ = brand;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::SetInitialDelay(int seconds) {
  initial_time_ = seconds;
}

void TestConfigurator::SetEnabledCupSigning(bool enabled_cup_signing) {
  enabled_cup_signing_ = enabled_cup_signing;
}

void TestConfigurator::SetEnabledComponentUpdates(
    bool enabled_component_updates) {
  enabled_component_updates_ = enabled_component_updates;
}

void TestConfigurator::SetDownloadPreference(
    const std::string& download_preference) {
  download_preference_ = download_preference;
}

void TestConfigurator::SetUpdateCheckUrl(const GURL& url) {
  update_check_url_ = url;
}

void TestConfigurator::SetPingUrl(const GURL& url) {
  ping_url_ = url;
}

PrefService* TestConfigurator::GetPrefService() const {
  return nullptr;
}

ActivityDataService* TestConfigurator::GetActivityDataService() const {
  return nullptr;
}

bool TestConfigurator::IsPerUserInstall() const {
  return true;
}

std::vector<uint8_t> TestConfigurator::GetRunActionKeyHash() const {
  return std::vector<uint8_t>(std::begin(gjpm_hash), std::end(gjpm_hash));
}

}  // namespace update_client
