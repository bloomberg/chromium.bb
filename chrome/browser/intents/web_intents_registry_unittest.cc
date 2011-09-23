// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/common/json_value_serializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockExtensionService: public ExtensionServiceInterface {
 public:
  virtual ~MockExtensionService() {}
  MOCK_CONST_METHOD0(extensions, const ExtensionList*());
  MOCK_METHOD0(pending_extension_manager, PendingExtensionManager*());
  MOCK_METHOD4(UpdateExtension, bool(const std::string& id,
                                     const FilePath& path,
                                     const GURL& download_url,
                                     CrxInstaller** out_crx_installer));
  MOCK_CONST_METHOD2(GetExtensionById,
                     const Extension*(const std::string& id,
                                      bool include_disabled));
  MOCK_CONST_METHOD1(GetInstalledExtension,
                     const Extension*(const std::string& id));
  MOCK_CONST_METHOD1(IsExtensionEnabled,bool(const std::string& extension_id));
  MOCK_CONST_METHOD1(IsExternalExtensionUninstalled,
                     bool(const std::string& extension_id));
  MOCK_METHOD1(UpdateExtensionBlacklist,
               void(const std::vector<std::string>& blacklist));
  MOCK_METHOD0(CheckAdminBlacklist, void());;
  MOCK_METHOD0(CheckForUpdatesSoon,void());

  MOCK_METHOD3(MergeDataAndStartSyncing,
               SyncError(syncable::ModelType type,
                         const SyncDataList& initial_sync_data,
                         SyncChangeProcessor* sync_processor));
  MOCK_METHOD1(StopSyncing, void(syncable::ModelType type));
  MOCK_CONST_METHOD1(GetAllSyncData, SyncDataList(syncable::ModelType type));
  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const tracked_objects::Location& from_here,
                         const SyncChangeList& change_list));
};

// TODO(groby): Unify loading functions with extension_manifest_unittest code.
DictionaryValue* LoadManifestFile(const std::string& filename,
                                  std::string* error) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
      .AppendASCII("manifest_tests")
      .AppendASCII(filename.c_str());
  EXPECT_TRUE(file_util::PathExists(path));

  JSONFileValueSerializer serializer(path);
  return static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));
}

namespace {
scoped_refptr<Extension> LoadExtensionWithLocation(
    DictionaryValue* value,
    Extension::Location location,
    bool strict_error_checks,
    std::string* error) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions").AppendASCII("manifest_tests");
  int flags = Extension::NO_FLAGS;
  if (strict_error_checks)
    flags |= Extension::STRICT_ERROR_CHECKS;
  return Extension::Create(path.DirName(), location, *value, flags, error);
}

scoped_refptr<Extension> LoadExtensionWithLocation(
    const std::string& name,
    Extension::Location location,
    bool strict_error_checks,
    std::string* error) {
  scoped_ptr<DictionaryValue> value(LoadManifestFile(name, error));
  if (!value.get())
    return NULL;
  return LoadExtensionWithLocation(value.get(), location,
                                   strict_error_checks, error);
}

scoped_refptr<Extension> LoadExtension(const std::string& name,
                                       std::string* error) {
  return LoadExtensionWithLocation(name, Extension::INTERNAL, false, error);
}

scoped_refptr<Extension> LoadAndExpectSuccess(const std::string& name) {
  std::string error;
  scoped_refptr<Extension> extension = LoadExtension(name, &error);
  EXPECT_TRUE(extension) << name;
  EXPECT_EQ("", error) << name;
  return extension;
}

}

class WebIntentsRegistryTest : public testing::Test {
 public:
   WebIntentsRegistryTest()
     : ui_thread_(BrowserThread::UI, &message_loop_),
       db_thread_(BrowserThread::DB) {}

 protected:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch("--enable-web-intents");

    db_thread_.Start();
    wds_ = new WebDataService();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    wds_->Init(temp_dir_.path());
    registry_.Initialize(wds_, &extensionService_);
    EXPECT_CALL(extensionService_, extensions()).
        WillRepeatedly(testing::Return(&extensions_));
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  scoped_refptr<WebDataService> wds_;
  MockExtensionService extensionService_;
  ExtensionList extensions_;
  WebIntentsRegistry registry_;
  ScopedTempDir temp_dir_;
};

// Simple consumer for WebIntentsRegistry notifications. Stores result data and
// terminates UI thread when callback is invoked.
class TestConsumer: public WebIntentsRegistry::Consumer {
 public:
   virtual void OnIntentsQueryDone(
       WebIntentsRegistry::QueryID id,
       const std::vector<WebIntentServiceData>& services) {
     DCHECK(id == expected_id_);
     services_ = services;

     DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
     MessageLoop::current()->Quit();
   }

   // Wait for the UI message loop to terminate - happens when OnIntesQueryDone
   // is invoked.
   void WaitForData() {
     DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
     MessageLoop::current()->Run();
   }

   WebIntentsRegistry::QueryID expected_id_;  // QueryID callback is tied to.
   std::vector<WebIntentServiceData> services_;  // Result data from callback.
};

TEST_F(WebIntentsRegistryTest, BasicTests) {
  WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Google's Sharing Service");

  registry_.RegisterIntentProvider(service);

  service.type = ASCIIToUTF16("video/*");
  registry_.RegisterIntentProvider(service);

  service.action = ASCIIToUTF16("search");
  registry_.RegisterIntentProvider(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("share"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("search"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  registry_.UnregisterIntentProvider(service);

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("share"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetAllIntents) {
  WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Google's Sharing Service");
  registry_.RegisterIntentProvider(service);

  service.action = ASCIIToUTF16("search");
  registry_.RegisterIntentProvider(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetAllIntentProviders(&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  if (consumer.services_[0].action != ASCIIToUTF16("share"))
    std::swap(consumer.services_[0],consumer.services_[1]);

  service.action = ASCIIToUTF16("share");
  EXPECT_EQ(service, consumer.services_[0]);

  service.action = ASCIIToUTF16("search");
  EXPECT_EQ(service, consumer.services_[1]);
}

TEST_F(WebIntentsRegistryTest, GetExtensionIntents) {
  extensions_.push_back(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.push_back(LoadAndExpectSuccess("intent_valid_2.json"));

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetAllIntentProviders(&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetSomeExtensionIntents) {
  extensions_.push_back(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.push_back(LoadAndExpectSuccess("intent_valid_2.json"));

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/edit"),&consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentsFromMixedSources) {
  extensions_.push_back(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.push_back(LoadAndExpectSuccess("intent_valid_2.json"));

  WebIntentServiceData service;
  service.service_url = GURL("http://somewhere.com/intent/edit.html");
  service.action = ASCIIToUTF16("http://webintents.org/edit");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Image Editing Service");
  registry_.RegisterIntentProvider(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/edit"),&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),&consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}
