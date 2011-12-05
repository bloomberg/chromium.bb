// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/json/json_value_serializer.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using webkit_glue::WebIntentServiceData;

class MockExtensionService: public TestExtensionService {
 public:
  virtual ~MockExtensionService() {}
  MOCK_CONST_METHOD0(extensions, const ExtensionSet*());
};

namespace {

// TODO(groby): Unify loading functions with extension_manifest_unittest code.
DictionaryValue* LoadManifestFile(const FilePath& path,
                                  std::string* error) {
  EXPECT_TRUE(file_util::PathExists(path));
  JSONFileValueSerializer serializer(path);
  return static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));
}

scoped_refptr<Extension> LoadExtensionWithLocation(
    const std::string& name,
    Extension::Location location,
    bool strict_error_checks,
    std::string* error) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
      .AppendASCII("manifest_tests")
      .AppendASCII(name.c_str());
  scoped_ptr<DictionaryValue> value(LoadManifestFile(path, error));
  if (!value.get())
    return NULL;
  int flags = Extension::NO_FLAGS;
  if (strict_error_checks)
    flags |= Extension::STRICT_ERROR_CHECKS;
  return Extension::CreateWithId(path.DirName(),
                                 location,
                                 *value,
                                 flags,
                                 Extension::GenerateIdForPath(path),
                                 error);
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

}  // namespace

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
    registry_.Initialize(wds_, &extension_service_);
    EXPECT_CALL(extension_service_, extensions()).
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
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_refptr<WebDataService> wds_;
  MockExtensionService extension_service_;
  ExtensionSet extensions_;
  WebIntentsRegistry registry_;
  ScopedTempDir temp_dir_;
};

// Simple consumer for WebIntentsRegistry notifications. Stores result data and
// terminates UI thread when callback is invoked.
class TestConsumer: public WebIntentsRegistry::Consumer {
 public:
   virtual void OnIntentsQueryDone(
       WebIntentsRegistry::QueryID id,
       const std::vector<webkit_glue::WebIntentServiceData>& services) {
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

   // QueryID callback is tied to.
   WebIntentsRegistry::QueryID expected_id_;

   // Result data from callback.
   std::vector<webkit_glue::WebIntentServiceData> services_;
};

TEST_F(WebIntentsRegistryTest, BasicTests) {
  webkit_glue::WebIntentServiceData service;
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
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("search"),
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  registry_.UnregisterIntentProvider(service);

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("share"),
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetAllIntents) {
  webkit_glue::WebIntentServiceData service;
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
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetAllIntentProviders(&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetSomeExtensionIntents) {
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/edit"), ASCIIToUTF16("*"),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentsFromMixedSources) {
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("http://somewhere.com/intent/edit.html");
  service.action = ASCIIToUTF16("http://webintents.org/edit");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Image Editing Service");
  registry_.RegisterIntentProvider(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/edit"), ASCIIToUTF16("*"),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"), ASCIIToUTF16("*"),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentsWithMimeMatching) {
  WebIntentServiceData services[] = {
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/*"),
                         ASCIIToUTF16("Image Sharing Service")),
    WebIntentServiceData(GURL("http://elsewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/jpeg"),
                         ASCIIToUTF16("Specific Image Editing Service")),
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/uri-list"),
                         ASCIIToUTF16("Link Sharing Service")),
    WebIntentServiceData(GURL("http://elsewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/plain"),
                         ASCIIToUTF16("Text Sharing Service"))
  };
  registry_.RegisterIntentProvider(services[0]);
  registry_.RegisterIntentProvider(services[1]);
  registry_.RegisterIntentProvider(services[2]);
  registry_.RegisterIntentProvider(services[3]);

  TestConsumer consumer;

  // Test specific match on both sides.
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("text/uri-list"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);

  // Test specific query, wildcard registration.
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);

  // Test wildcard query, specific registration.
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("text/*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);
  EXPECT_EQ(services[3], consumer.services_[1]);

  // Test wildcard query, wildcard registration.
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);
  EXPECT_EQ(services[1], consumer.services_[1]);

  // Test "catch-all" query.
  consumer.expected_id_ = registry_.GetIntentProviders(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(4U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);
  EXPECT_EQ(services[1], consumer.services_[1]);
  EXPECT_EQ(services[2], consumer.services_[2]);
  EXPECT_EQ(services[3], consumer.services_[3]);
}
