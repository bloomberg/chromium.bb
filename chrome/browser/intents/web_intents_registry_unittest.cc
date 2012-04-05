// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/intents/default_web_intent_service.h"
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
  MOCK_CONST_METHOD2(GetExtensionById,
                     const Extension*(const std::string&, bool));
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
  return Extension::Create(path.DirName(),
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
    EXPECT_CALL(extension_service_, GetExtensionById(testing::_, testing::_)).
        WillRepeatedly(
            testing::Invoke(this, &WebIntentsRegistryTest::GetExtensionById));
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  const Extension* GetExtensionById(const std::string& extension_id,
                                    testing::Unused) {
    for (ExtensionSet::const_iterator iter = extensions_.begin();
         iter != extensions_.end(); ++iter) {
      if ((*iter)->id() == extension_id)
        return &**iter;
    }

    return NULL;
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

   virtual void OnIntentsDefaultsQueryDone(
       WebIntentsRegistry::QueryID id,
       const DefaultWebIntentService& default_service) {
     DCHECK(id == expected_id_);
     default_ = default_service;

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

   // Result default data from callback.
   DefaultWebIntentService default_;
};

TEST_F(WebIntentsRegistryTest, BasicTests) {
  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Google's Sharing Service");

  registry_.RegisterIntentService(service);

  service.type = ASCIIToUTF16("video/*");
  service.title = ASCIIToUTF16("Second Service");
  registry_.RegisterIntentService(service);

  service.action = ASCIIToUTF16("search");
  registry_.RegisterIntentService(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentServices(ASCIIToUTF16("share"),
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentServices(ASCIIToUTF16("search"),
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  registry_.UnregisterIntentService(service);

  consumer.expected_id_ = registry_.GetIntentServices(ASCIIToUTF16("share"),
                                                       ASCIIToUTF16("*"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentServicesForExtensionFilter) {
  scoped_refptr<Extension> share_extension(
      LoadAndExpectSuccess("intent_valid.json"));
  scoped_refptr<Extension> edit_extension(
      LoadAndExpectSuccess("intent_valid_2.json"));
  extensions_.Insert(share_extension);
  extensions_.Insert(edit_extension);
  ASSERT_EQ(2U, extensions_.size());

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentServicesForExtensionFilter(
      ASCIIToUTF16("http://webintents.org/edit"),
      ASCIIToUTF16("image/*"),
      edit_extension->id(),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetAllIntents) {
  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  service.title = ASCIIToUTF16("Google's Sharing Service");
  registry_.RegisterIntentService(service);

  service.action = ASCIIToUTF16("search");
  registry_.RegisterIntentService(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetAllIntentServices(&consumer);
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
  consumer.expected_id_ = registry_.GetAllIntentServices(&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetSomeExtensionIntents) {
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentServices(
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
  registry_.RegisterIntentService(service);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/edit"), ASCIIToUTF16("*"),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"), ASCIIToUTF16("*"),
      &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentsWithMimeMatching) {
  WebIntentServiceData services[] = {
    WebIntentServiceData(GURL("http://elsewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/*"),
                         ASCIIToUTF16("Image Sharing Service")),
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/jpeg"),
                         ASCIIToUTF16("Specific Image Editing Service")),
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/uri-list"),
                         ASCIIToUTF16("Text Link Sharing Service")),
    WebIntentServiceData(GURL("http://somewhere2.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/plain"),
                         ASCIIToUTF16("Text Sharing Service"))
  };
  registry_.RegisterIntentService(services[0]);
  registry_.RegisterIntentService(services[1]);
  registry_.RegisterIntentService(services[2]);
  registry_.RegisterIntentService(services[3]);

  TestConsumer consumer;

  // Test specific match on both sides.
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("text/uri-list"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);

  // Test specific query, wildcard registration.
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);

  // Test wildcard query, specific registration.
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("text/*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);
  EXPECT_EQ(services[3], consumer.services_[1]);

  // Test wildcard query, wildcard registration.
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);
  EXPECT_EQ(services[1], consumer.services_[1]);

  // Test "catch-all" query.
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(4U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);
  EXPECT_EQ(services[1], consumer.services_[1]);
  EXPECT_EQ(services[2], consumer.services_[2]);
  EXPECT_EQ(services[3], consumer.services_[3]);
}

TEST_F(WebIntentsRegistryTest, TestGetDefaults) {
  DefaultWebIntentService default_service;
  default_service.action = ASCIIToUTF16("share");
  default_service.type = ASCIIToUTF16("type/*");
  // Values here are just dummies to test for preservation.
  default_service.user_date = 1;
  default_service.suppression = 4;
  default_service.service_url = "service_url";
  registry_.RegisterDefaultIntentService(default_service);

  TestConsumer consumer;

  // Test we can retrieve default entries by action.
  consumer.expected_id_ = registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"),
      ASCIIToUTF16("type/plain"),
      GURL("http://www.google.com/"),
      &consumer);

  consumer.WaitForData();

  EXPECT_EQ("service_url", consumer.default_.service_url);
  EXPECT_EQ(1, consumer.default_.user_date);
  EXPECT_EQ(4, consumer.default_.suppression);

  // Test that no action match means we don't retrieve any
  // default entries.
  consumer.default_ = DefaultWebIntentService();
  ASSERT_EQ("", consumer.default_.service_url);
  consumer.expected_id_ = registry_.GetDefaultIntentService(
      ASCIIToUTF16("no-share"),
      ASCIIToUTF16("type/plain"),
      GURL("http://www.google.com/"),
      &consumer);

  consumer.WaitForData();

  EXPECT_EQ("", consumer.default_.service_url);

  // Test that no type match means we don't retrieve any
  // default entries (they get filtered out).
  consumer.default_ = DefaultWebIntentService();
  ASSERT_EQ("", consumer.default_.service_url);
  consumer.expected_id_ = registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"),
      ASCIIToUTF16("notype/plain"),
      GURL("http://www.google.com/"),
      &consumer);

  consumer.WaitForData();

  EXPECT_EQ("", consumer.default_.service_url);
}

// Verify that collapsing equivalent intents works properly.
TEST_F(WebIntentsRegistryTest, CollapseIntents) {
  WebIntentsRegistry::IntentServiceList services;

  // Add two intents with identical |service_url|, |title|, and |action|.
  services.push_back(
      WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      ASCIIToUTF16("Image Sharing Service")));
  services.push_back(
      WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/jpg"),
      ASCIIToUTF16("Image Sharing Service")));
  // Service that differs in disposition.
  services.push_back(
      WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      ASCIIToUTF16("Image Sharing Service")));
  ASSERT_EQ(WebIntentServiceData::DISPOSITION_WINDOW,
      services.back().disposition);
  services.back().disposition = WebIntentServiceData::DISPOSITION_INLINE;
  // Service that differs in title.
  services.push_back(
      WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      ASCIIToUTF16("Sharing Service")));
  // Service that differs in |action|.
  services.push_back(
      WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("http://webintents.org/share-old"),
      ASCIIToUTF16("image/png"),
      ASCIIToUTF16("Image Sharing Service")));
  // Service that differs in |service_url|.
  services.push_back(
      WebIntentServiceData(GURL("http://zoo.com/share.html"),
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      ASCIIToUTF16("Image Sharing Service")));

  // Only the first two services should be collapsed.
  registry_.CollapseIntents(&services);
  ASSERT_EQ(5UL, services.size());

  // Joined services have their mime types combined
  EXPECT_EQ(ASCIIToUTF16("image/png,image/jpg"), services[0].type);

  // Verify the remaining services via distinguishing characteristics.
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE, services[1].disposition);
  EXPECT_EQ(ASCIIToUTF16("Sharing Service"), services[2].title);
  EXPECT_EQ(ASCIIToUTF16("http://webintents.org/share-old"),
      services[3].action);
  EXPECT_EQ(GURL("http://zoo.com/share.html").spec(),
      services[4].service_url.spec());
}

// Verify that GetIntentServices collapses equivalent intents.
TEST_F(WebIntentsRegistryTest, GetIntentsCollapsesEquivalentIntents) {
  WebIntentServiceData services[] = {
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/png"),
                         ASCIIToUTF16("Image Sharing Service")),
    WebIntentServiceData(GURL("http://somewhere.com/intent/share.html"),
                        ASCIIToUTF16("http://webintents.org/share"),
                        ASCIIToUTF16("image/jpg"),
                        ASCIIToUTF16("Image Sharing Service"))
  };
  registry_.RegisterIntentService(services[0]);
  registry_.RegisterIntentService(services[1]);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/*"), &consumer);
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(ASCIIToUTF16("image/png,image/jpg"), consumer.services_[0].type);
}
