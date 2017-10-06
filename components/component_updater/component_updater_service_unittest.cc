// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Configurator = update_client::Configurator;
using Result = update_client::CrxInstaller::Result;
using TestConfigurator = update_client::TestConfigurator;
using UpdateClient = update_client::UpdateClient;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace component_updater {

class MockInstaller : public CrxInstaller {
 public:
  MockInstaller();

  // gMock does not support mocking functions with parameters which have
  // move semantics. This function is a shim to work around it.
  void Install(std::unique_ptr<base::DictionaryValue> manifest,
               const base::FilePath& unpack_path,
               const update_client::CrxInstaller::Callback& callback) {
    Install_(manifest, unpack_path, callback);
  }

  MOCK_METHOD1(OnUpdateError, void(int error));
  MOCK_METHOD3(Install_,
               void(const std::unique_ptr<base::DictionaryValue>& manifest,
                    const base::FilePath& unpack_path,
                    const update_client::CrxInstaller::Callback& callback));
  MOCK_METHOD2(GetInstalledFile,
               bool(const std::string& file, base::FilePath* installed_file));
  MOCK_METHOD0(Uninstall, bool());

 private:
  ~MockInstaller() override;
};

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient();
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD3(Install,
               void(const std::string& id,
                    const CrxDataCallback& crx_data_callback,
                    const Callback& callback));
  MOCK_METHOD3(Update,
               void(const std::vector<std::string>& ids,
                    const CrxDataCallback& crx_data_callback,
                    const Callback& callback));
  MOCK_CONST_METHOD2(GetCrxUpdateState,
                     bool(const std::string& id, CrxUpdateItem* update_item));
  MOCK_CONST_METHOD1(IsUpdating, bool(const std::string& id));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(SendUninstallPing,
               void(const std::string& id,
                    const base::Version& version,
                    int reason,
                    const Callback& callback));

 private:
  ~MockUpdateClient() override;
};

class MockServiceObserver : public ServiceObserver {
 public:
  MockServiceObserver();
  ~MockServiceObserver() override;

  MOCK_METHOD2(OnEvent, void(Events event, const std::string&));
};

class ComponentUpdaterTest : public testing::Test {
 public:
  ComponentUpdaterTest();
  ~ComponentUpdaterTest() override;

  void SetUp() override;

  void TearDown() override;

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService& component_updater() { return *component_updater_; }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::Closure quit_closure() const { return quit_closure_; }
  static void ReadyCallback() {}

 protected:
  void RunThreads();

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::RunLoop runloop_;
  const base::Closure quit_closure_ = runloop_.QuitClosure();

  scoped_refptr<TestConfigurator> config_ =
      base::MakeRefCounted<TestConfigurator>();
  scoped_refptr<MockUpdateClient> update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ComponentUpdateService> component_updater_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterTest);
};

class OnDemandTester {
 public:
  void OnDemand(ComponentUpdateService* cus, const std::string& id);
  update_client::Error error() const { return error_; }

 private:
  void OnDemandComplete(update_client::Error error);

  update_client::Error error_ = update_client::Error::NONE;
};

MockInstaller::MockInstaller() {
}

MockInstaller::~MockInstaller() {
}

MockUpdateClient::MockUpdateClient() {
}

MockUpdateClient::~MockUpdateClient() {
}

MockServiceObserver::MockServiceObserver() {
}

MockServiceObserver::~MockServiceObserver() {
}

void OnDemandTester::OnDemand(ComponentUpdateService* cus,
                              const std::string& id) {
  cus->GetOnDemandUpdater().OnDemandUpdate(
      id,
      base::Bind(&OnDemandTester::OnDemandComplete, base::Unretained(this)));
}

void OnDemandTester::OnDemandComplete(update_client::Error error) {
  error_ = error;
}

std::unique_ptr<ComponentUpdateService> TestComponentUpdateServiceFactory(
    const scoped_refptr<Configurator>& config) {
  DCHECK(config);
  return base::MakeUnique<CrxUpdateService>(config, new MockUpdateClient());
}

ComponentUpdaterTest::ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), AddObserver(_)).Times(1);
  component_updater_ =
      base::MakeUnique<CrxUpdateService>(config_, update_client_);
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_)).Times(1);
  component_updater_.reset();
}

void ComponentUpdaterTest::SetUp() {
}

void ComponentUpdaterTest::TearDown() {
}

void ComponentUpdaterTest::RunThreads() {
  runloop_.Run();
}

TEST_F(ComponentUpdaterTest, AddObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), AddObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  component_updater().AddObserver(&observer);
}

TEST_F(ComponentUpdaterTest, RemoveObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), RemoveObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  component_updater().RemoveObserver(&observer);
}

// Tests that UpdateClient::Update is called by the timer loop when
// components are registered, and the component update starts.
// Also tests that Uninstall is called when a component is unregistered.
TEST_F(ComponentUpdaterTest, RegisterComponent) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, const base::Closure& quit_closure)
        : max_cnt_(max_cnt), quit_closure_(quit_closure) {}

    void OnUpdate(const std::vector<std::string>& ids,
                  const UpdateClient::CrxDataCallback& crx_data_callback,
                  const Callback& callback) {
      callback.Run(update_client::Error::NONE);
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        quit_closure_.Run();
    }

   private:
    const int max_cnt_;
    base::Closure quit_closure_;
  };

  base::HistogramTester ht;

  scoped_refptr<MockInstaller> installer(new MockInstaller());
  EXPECT_CALL(*installer, Uninstall()).WillOnce(Return(true));

  using update_client::jebg_hash;
  using update_client::abag_hash;

  const std::string id1 = "abagagagagagagagagagagagagagagag";
  const std::string id2 = "jebgalgnebhfojomionfpkfelancnnkf";
  std::vector<std::string> ids;
  ids.push_back(id1);
  ids.push_back(id2);

  CrxComponent crx_component1;
  crx_component1.pk_hash.assign(abag_hash, abag_hash + arraysize(abag_hash));
  crx_component1.version = base::Version("1.0");
  crx_component1.installer = installer;

  CrxComponent crx_component2;
  crx_component2.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component2.version = base::Version("0.9");
  crx_component2.installer = installer;

  // Quit after two update checks have fired.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(update_client(), Update(ids, _, _))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), IsUpdating(id1)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(crx_component1));
  EXPECT_TRUE(component_updater().RegisterComponent(crx_component2));

  RunThreads();
  EXPECT_TRUE(component_updater().UnregisterComponent(id1));

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 1, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that on-demand updates invoke UpdateClient::Install.
TEST_F(ComponentUpdaterTest, OnDemandUpdate) {
  class LoopHandler {
   public:
    explicit LoopHandler(int max_cnt) : max_cnt_(max_cnt) {}

    void OnInstall(const std::string& ids,
                   const UpdateClient::CrxDataCallback& crx_data_callback,
                   const Callback& callback) {
      callback.Run(update_client::Error::NONE);
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&LoopHandler::Quit, base::Unretained(this)));
      }
    }

   private:
    void Quit() { base::RunLoop::QuitCurrentWhenIdleDeprecated(); }

    const int max_cnt_;
  };

  base::HistogramTester ht;

  auto config = configurator();
  config->SetInitialDelay(3600);

  auto& cus = component_updater();

  // Tests calling OnDemand for an unregistered component. This call results in
  // an error, which is recorded by the OnDemandTester instance. Since the
  // component was not registered, the call is ignored for UMA metrics.
  OnDemandTester ondemand_tester_component_not_registered;
  ondemand_tester_component_not_registered.OnDemand(
      &cus, "ihfokbkgjpifnbbojhneepfflplebdkc");

  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";

  using update_client::jebg_hash;
  CrxComponent crx_component;
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.version = base::Version("0.9");
  crx_component.installer = new MockInstaller();

  LoopHandler loop_handler(1);
  EXPECT_CALL(update_client(),
              Install("jebgalgnebhfojomionfpkfelancnnkf", _, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(update_client(), Stop()).Times(1);

  EXPECT_TRUE(cus.RegisterComponent(crx_component));
  OnDemandTester ondemand_tester;
  ondemand_tester.OnDemand(&cus, id);

  base::RunLoop().Run();

  EXPECT_EQ(update_client::Error::INVALID_ARGUMENT,
            ondemand_tester_component_not_registered.error());
  EXPECT_EQ(update_client::Error::NONE, ondemand_tester.error());

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 1);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 1);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 1);
}

// Tests that throttling an update invokes UpdateClient::Install.
TEST_F(ComponentUpdaterTest, MaybeThrottle) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, const base::Closure& quit_closure)
        : max_cnt_(max_cnt), quit_closure_(quit_closure) {}

    void OnInstall(const std::string& ids,
                   const UpdateClient::CrxDataCallback& crx_data_callback,
                   const Callback& callback) {
      callback.Run(update_client::Error::NONE);
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        quit_closure_.Run();
    }

   private:
    const int max_cnt_;
    base::Closure quit_closure_;
  };

  base::HistogramTester ht;

  auto config = configurator();
  config->SetInitialDelay(3600);

  scoped_refptr<MockInstaller> installer(new MockInstaller());

  using update_client::jebg_hash;
  CrxComponent crx_component;
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.version = base::Version("0.9");
  crx_component.installer = installer;

  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(),
              Install("jebgalgnebhfojomionfpkfelancnnkf", _, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(update_client(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(crx_component));
  component_updater().MaybeThrottle(
      "jebgalgnebhfojomionfpkfelancnnkf",
      base::Bind(&ComponentUpdaterTest::ReadyCallback));

  RunThreads();

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 1);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 1);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 1);
}

}  // namespace component_updater
