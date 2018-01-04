// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/uninstall_ping_sender.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_service.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeUpdateClient : public update_client::UpdateClient {
 public:
  FakeUpdateClient();

  // Returns the data we've gotten from the CrxDataCallback for ids passed to
  // the Update function.
  std::vector<update_client::CrxComponent>* data() { return &data_; }

  // Used for tests that uninstall pings get requested properly.
  struct UninstallPing {
    std::string id;
    base::Version version;
    int reason;
    UninstallPing(const std::string& id,
                  const base::Version& version,
                  int reason)
        : id(id), version(version), reason(reason) {}
  };
  std::vector<UninstallPing>& uninstall_pings() { return uninstall_pings_; }

  // update_client::UpdateClient
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void Install(const std::string& id,
               CrxDataCallback crx_data_callback,
               update_client::Callback callback) override {}
  void Update(const std::vector<std::string>& ids,
              CrxDataCallback crx_data_callback,
              update_client::Callback callback) override;
  bool GetCrxUpdateState(
      const std::string& id,
      update_client::CrxUpdateItem* update_item) const override {
    return false;
  }
  bool IsUpdating(const std::string& id) const override { return false; }
  void Stop() override {}
  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason,
                         update_client::Callback callback) override {
    uninstall_pings_.emplace_back(id, version, reason);
  }

 protected:
  friend class base::RefCounted<FakeUpdateClient>;
  ~FakeUpdateClient() override {}

  std::vector<update_client::CrxComponent> data_;
  std::vector<UninstallPing> uninstall_pings_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUpdateClient);
};

FakeUpdateClient::FakeUpdateClient() {}

void FakeUpdateClient::Update(const std::vector<std::string>& ids,
                              CrxDataCallback crx_data_callback,
                              update_client::Callback callback) {
  std::move(crx_data_callback).Run(ids, &data_);
  std::move(callback).Run(update_client::Error::NONE);
}

}  // namespace

namespace extensions {

namespace {

// A global variable for controlling whether uninstalls should cause uninstall
// pings to be sent.
UninstallPingSender::FilterResult g_should_ping =
    UninstallPingSender::DO_NOT_SEND_PING;

// Helper method to serve as an uninstall ping filter.
UninstallPingSender::FilterResult ShouldPing(const Extension* extension,
                                             UninstallReason reason) {
  return g_should_ping;
}

// A fake ExtensionSystem that lets us intercept calls to install new
// versions of an extension.
class FakeExtensionSystem : public MockExtensionSystem {
 public:
  using InstallUpdateCallback = MockExtensionSystem::InstallUpdateCallback;
  explicit FakeExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}
  ~FakeExtensionSystem() override {}

  struct InstallUpdateRequest {
    std::string extension_id;
    base::FilePath temp_dir;
  };

  std::vector<InstallUpdateRequest>* install_requests() {
    return &install_requests_;
  }

  void set_install_callback(base::OnceClosure callback) {
    next_install_callback_ = std::move(callback);
  }

  // ExtensionSystem override
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     InstallUpdateCallback install_update_callback) override {
    base::DeleteFile(temp_dir, true /*recursive*/);
    InstallUpdateRequest request;
    request.extension_id = extension_id;
    request.temp_dir = temp_dir;
    install_requests_.push_back(request);
    if (!next_install_callback_.is_null()) {
      std::move(next_install_callback_).Run();
    }
    std::move(install_update_callback).Run(true);
  }

 private:
  std::vector<InstallUpdateRequest> install_requests_;
  base::OnceClosure next_install_callback_;
};

class UpdateServiceTest : public ExtensionsTest {
 public:
  UpdateServiceTest()
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>()) {}
  ~UpdateServiceTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(
        &fake_extension_system_factory_);
    extensions_browser_client()->SetUpdateClientFactory(base::Bind(
        &UpdateServiceTest::CreateUpdateClient, base::Unretained(this)));

    update_service_ = UpdateService::Get(browser_context());
  }

 protected:
  UpdateService* update_service() const { return update_service_; }
  FakeUpdateClient* update_client() const { return update_client_.get(); }

  update_client::UpdateClient* CreateUpdateClient() {
    // We only expect that this will get called once, so consider it an error
    // if our update_client_ is already non-null.
    EXPECT_EQ(nullptr, update_client_.get());
    update_client_ = base::MakeRefCounted<FakeUpdateClient>();
    return update_client_.get();
  }

  // Helper function that creates a file at |relative_path| within |directory|
  // and fills it with |content|.
  bool AddFileToDirectory(const base::FilePath& directory,
                          const base::FilePath& relative_path,
                          const std::string& content) {
    base::FilePath full_path = directory.Append(relative_path);
    if (!CreateDirectory(full_path.DirName()))
      return false;
    int result = base::WriteFile(full_path, content.data(), content.size());
    return (static_cast<size_t>(result) == content.size());
  }

  FakeExtensionSystem* extension_system() {
    return static_cast<FakeExtensionSystem*>(
        fake_extension_system_factory_.GetForBrowserContext(browser_context()));
  }

 private:
  UpdateService* update_service_ = nullptr;
  scoped_refptr<FakeUpdateClient> update_client_;
  MockExtensionSystemFactory<FakeExtensionSystem>
      fake_extension_system_factory_;
};

TEST_F(UpdateServiceTest, BasicUpdateOperations) {
  // Create a temporary directory that a fake extension will live in and fill
  // it with some test files.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath foo_js(FILE_PATH_LITERAL("foo.js"));
  base::FilePath bar_html(FILE_PATH_LITERAL("bar/bar.html"));
  ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), foo_js, "hello"))
      << "Failed to write " << temp_dir.GetPath().value() << "/"
      << foo_js.value();
  ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), bar_html, "world"));

  ExtensionBuilder builder;
  builder.SetManifest(DictionaryBuilder()
                          .Set("name", "Foo")
                          .Set("version", "1.0")
                          .Set("manifest_version", 2)
                          .Build());
  builder.SetID(crx_file::id_util::GenerateId("whatever"));
  builder.SetPath(temp_dir.GetPath());

  scoped_refptr<Extension> extension1(builder.Build());

  ExtensionRegistry::Get(browser_context())->AddEnabled(extension1);

  ExtensionUpdateCheckParams update_check_params;
  update_check_params.update_info[extension1->id()] = ExtensionUpdateData();

  // Start an update check and verify that the UpdateClient was sent the right
  // data.
  update_service()->StartUpdateCheck(update_check_params);
  std::vector<update_client::CrxComponent>* data = update_client()->data();
  ASSERT_NE(nullptr, data);
  ASSERT_EQ(1u, data->size());

  ASSERT_EQ(data->at(0).version, extension1->version());
  update_client::CrxInstaller* installer = data->at(0).installer.get();
  ASSERT_NE(installer, nullptr);

  // The GetInstalledFile method is used when processing differential updates
  // to get a path to an existing file in an extension. We want to test a
  // number of scenarios to be user we handle invalid relative paths, don't
  // accidentally return paths outside the extension's dir, etc.
  base::FilePath tmp;
  EXPECT_TRUE(installer->GetInstalledFile(foo_js.MaybeAsASCII(), &tmp));
  EXPECT_EQ(temp_dir.GetPath().Append(foo_js), tmp) << tmp.value();

  EXPECT_TRUE(installer->GetInstalledFile(bar_html.MaybeAsASCII(), &tmp));
  EXPECT_EQ(temp_dir.GetPath().Append(bar_html), tmp) << tmp.value();

  EXPECT_FALSE(installer->GetInstalledFile("does_not_exist", &tmp));
  EXPECT_FALSE(installer->GetInstalledFile("does/not/exist", &tmp));
  EXPECT_FALSE(installer->GetInstalledFile("/does/not/exist", &tmp));
  EXPECT_FALSE(installer->GetInstalledFile("C:\\tmp", &tmp));

  base::FilePath system_temp_dir;
  ASSERT_TRUE(base::GetTempDir(&system_temp_dir));
  EXPECT_FALSE(
      installer->GetInstalledFile(system_temp_dir.MaybeAsASCII(), &tmp));

  // Test the install callback.
  base::ScopedTempDir new_version_dir;
  ASSERT_TRUE(new_version_dir.CreateUniqueTempDir());

  bool done = false;
  installer->Install(
      new_version_dir.GetPath(), std::string(),
      base::BindOnce(
          [](bool* done, const update_client::CrxInstaller::Result& result) {
            *done = true;
            EXPECT_EQ(0, result.error);
            EXPECT_EQ(0, result.extended_error);
          },
          &done));

  scoped_refptr<content::MessageLoopRunner> loop_runner =
      base::MakeRefCounted<content::MessageLoopRunner>();
  extension_system()->set_install_callback(loop_runner->QuitClosure());
  loop_runner->Run();

  std::vector<FakeExtensionSystem::InstallUpdateRequest>* requests =
      extension_system()->install_requests();
  ASSERT_EQ(1u, requests->size());
  EXPECT_EQ(requests->at(0).extension_id, extension1->id());
  EXPECT_EQ(requests->at(0).temp_dir.value(),
            new_version_dir.GetPath().value());
  EXPECT_TRUE(done);
}

TEST_F(UpdateServiceTest, UninstallPings) {
  UninstallPingSender sender(ExtensionRegistry::Get(browser_context()),
                             base::Bind(&ShouldPing));

  // Build 3 extensions.
  scoped_refptr<Extension> extension1 =
      ExtensionBuilder("1")
          .MergeManifest(DictionaryBuilder().Set("version", "1.2").Build())
          .Build();
  scoped_refptr<Extension> extension2 =
      ExtensionBuilder("2")
          .MergeManifest(DictionaryBuilder().Set("version", "2.3").Build())
          .Build();
  scoped_refptr<Extension> extension3 =
      ExtensionBuilder("3")
          .MergeManifest(DictionaryBuilder().Set("version", "3.4").Build())
          .Build();
  EXPECT_TRUE(extension1->id() != extension2->id() &&
              extension1->id() != extension3->id() &&
              extension2->id() != extension3->id());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Run tests for each uninstall reason.
  for (int reason_val = static_cast<int>(UNINSTALL_REASON_FOR_TESTING);
       reason_val < static_cast<int>(UNINSTALL_REASON_MAX); ++reason_val) {
    UninstallReason reason = static_cast<UninstallReason>(reason_val);

    // Start with 2 enabled and 1 disabled extensions.
    EXPECT_TRUE(registry->AddEnabled(extension1)) << reason;
    EXPECT_TRUE(registry->AddEnabled(extension2)) << reason;
    EXPECT_TRUE(registry->AddDisabled(extension3)) << reason;

    // Uninstall the first extension, instructing our filter not to send pings,
    // and verify none were sent.
    g_should_ping = UninstallPingSender::DO_NOT_SEND_PING;
    EXPECT_TRUE(registry->RemoveEnabled(extension1->id())) << reason;
    registry->TriggerOnUninstalled(extension1.get(), reason);
    EXPECT_TRUE(update_client()->uninstall_pings().empty()) << reason;

    // Uninstall the second and third extensions, instructing the filter to
    // send pings, and make sure we got the expected data.
    g_should_ping = UninstallPingSender::SEND_PING;
    EXPECT_TRUE(registry->RemoveEnabled(extension2->id())) << reason;
    registry->TriggerOnUninstalled(extension2.get(), reason);
    EXPECT_TRUE(registry->RemoveDisabled(extension3->id())) << reason;
    registry->TriggerOnUninstalled(extension3.get(), reason);

    std::vector<FakeUpdateClient::UninstallPing>& pings =
        update_client()->uninstall_pings();
    ASSERT_EQ(2u, pings.size()) << reason;

    EXPECT_EQ(extension2->id(), pings[0].id) << reason;
    EXPECT_EQ(extension2->version(), pings[0].version) << reason;
    EXPECT_EQ(reason, pings[0].reason) << reason;

    EXPECT_EQ(extension3->id(), pings[1].id) << reason;
    EXPECT_EQ(extension3->version(), pings[1].version) << reason;
    EXPECT_EQ(reason, pings[1].reason) << reason;

    pings.clear();
  }
}

}  // namespace

}  // namespace extensions
