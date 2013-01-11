// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/web_intents_handler.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::Extension;
using webkit_glue::WebIntentServiceData;

class MockExtensionService: public TestExtensionService {
 public:
  virtual ~MockExtensionService() {}
  MOCK_CONST_METHOD0(extensions, const ExtensionSet*());
  MOCK_CONST_METHOD2(GetExtensionById,
                     const Extension*(const std::string&, bool));
  MOCK_CONST_METHOD1(GetInstalledExtension,
                     const Extension*(const std::string& id));
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
    std::string* error) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
      .AppendASCII("manifest_tests")
      .AppendASCII(name.c_str());
  scoped_ptr<DictionaryValue> value(LoadManifestFile(path, error));
  if (!value.get())
    return NULL;
  return Extension::Create(path.DirName(),
                           location,
                           *value,
                           Extension::NO_FLAGS,
                           Extension::GenerateIdForPath(path),
                           error);
}

scoped_refptr<Extension> LoadExtension(const std::string& name,
                                       std::string* error) {
  return LoadExtensionWithLocation(name, Extension::INTERNAL, error);
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
    extensions::ManifestHandler::Register(extension_manifest_keys::kIntents,
                                          new extensions::WebIntentsHandler);
  }

  virtual void TearDown() {
    // Clear all references to wds to force it destruction.
    wds_->ShutdownOnUIThread();
    wds_ = NULL;

    // Schedule another task on the DB thread to notify us that it's safe to
    // carry on with the test.
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                            base::Bind(&base::WaitableEvent::Signal,
                                       base::Unretained(&done)));
    done.Wait();
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
  base::ScopedTempDir temp_dir_;
};

// Base consumer for WebIntentsRegistry results.
class TestConsumer {
 public:
  // Wait for the UI message loop to terminate - happens when OnIntesQueryDone
  // is invoked.
  void WaitForData() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Run();
  }
};

// Consumer of service lists. Stores result data and
// terminates UI thread when callback is invoked.
class ServiceListConsumer : public TestConsumer {
 public:
  void Accept(
      const std::vector<webkit_glue::WebIntentServiceData>& services) {
    services_ = services;

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  bool ResultsContain(const webkit_glue::WebIntentServiceData& service) {
    for (size_t i = 0; i < services_.size(); ++i) {
      if (services_[i] == service)
        return true;
    }
    return false;
  }

  // Result data from callback.
  std::vector<webkit_glue::WebIntentServiceData> services_;
};

// Consume or defaultservice lists. Stores result data and
// terminates UI thread when callback is invoked.
class DefaultServiceListConsumer : public TestConsumer {
 public:
  void Accept(const std::vector<DefaultWebIntentService>& services) {
    services_ = services;

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  bool ResultsContain(const DefaultWebIntentService& service) {
    for (size_t i = 0; i < services_.size(); ++i) {
      if (services_[i] == service)
        return true;
    }
    return false;
  }

  // Result data from callback.
  std::vector<DefaultWebIntentService> services_;
};

// Consumer of a default service. Stores result data and
// terminates UI thread when callback is invoked.
class DefaultServiceConsumer : public TestConsumer {
 public:
  void Accept(
      const DefaultWebIntentService& default_service) {
    service_ = default_service;

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  // Result default data from callback.
  DefaultWebIntentService service_;
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

  ServiceListConsumer consumer;
  registry_.GetIntentServices(ASCIIToUTF16("share"), ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  EXPECT_EQ(2U, consumer.services_.size());

  registry_.GetIntentServices(ASCIIToUTF16("search"), ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.services_.size());

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  registry_.UnregisterIntentService(service);

  registry_.GetIntentServices(ASCIIToUTF16("share"), ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
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

  WebIntentsRegistry::IntentServiceList services;
  registry_.GetIntentServicesForExtensionFilter(
      ASCIIToUTF16("http://webintents.org/edit"),
      ASCIIToUTF16("image/*"),
      edit_extension->id(),
      &services);
  ASSERT_EQ(1U, services.size());

  EXPECT_EQ(edit_extension->url().spec() + "services/edit",
            services[0].service_url.spec());
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

  ServiceListConsumer consumer;
  registry_.GetAllIntentServices(base::Bind(&ServiceListConsumer::Accept,
                                            base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  if (consumer.services_[0].action != ASCIIToUTF16("share"))
    std::swap(consumer.services_[0], consumer.services_[1]);

  service.action = ASCIIToUTF16("share");
  EXPECT_EQ(service, consumer.services_[0]);

  service.action = ASCIIToUTF16("search");
  EXPECT_EQ(service, consumer.services_[1]);
}

TEST_F(WebIntentsRegistryTest, GetExtensionIntents) {
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  ServiceListConsumer consumer;
  registry_.GetAllIntentServices(base::Bind(&ServiceListConsumer::Accept,
                                            base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetSomeExtensionIntents) {
  extensions_.Insert(LoadAndExpectSuccess("intent_valid.json"));
  extensions_.Insert(LoadAndExpectSuccess("intent_valid_2.json"));
  ASSERT_EQ(2U, extensions_.size());

  ServiceListConsumer consumer;
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/edit"),
                              ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
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

  ServiceListConsumer consumer;
  registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/edit"), ASCIIToUTF16("*"),
      base::Bind(&ServiceListConsumer::Accept,
          base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());

  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, GetIntentsWithMimeAndLiteralMatching) {
  WebIntentServiceData services[] = {
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/*"),
                         string16(),
                         GURL("http://elsewhere.com/intent/share.html"),
                         ASCIIToUTF16("Image Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/jpeg"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Specific Image Editing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/uri-list"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Link Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("text/plain"),
                         string16(),
                         GURL("http://somewhere2.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("elsewhere"),
                         string16(),
                         GURL("http://elsewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("somewhere"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("nota/*"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("*nomime"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("*/nomime"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("*/*nomime"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("*/*/nomime"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("nomime/*"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("x-type/*"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("x-/*"),  // actually a string literal
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Text Sharing Service"))
  };
  registry_.RegisterIntentService(services[0]);
  registry_.RegisterIntentService(services[1]);
  registry_.RegisterIntentService(services[2]);
  registry_.RegisterIntentService(services[3]);
  registry_.RegisterIntentService(services[4]);
  registry_.RegisterIntentService(services[5]);
  registry_.RegisterIntentService(services[6]);
  registry_.RegisterIntentService(services[7]);
  registry_.RegisterIntentService(services[8]);
  registry_.RegisterIntentService(services[9]);
  registry_.RegisterIntentService(services[10]);
  registry_.RegisterIntentService(services[11]);
  registry_.RegisterIntentService(services[12]);
  registry_.RegisterIntentService(services[13]);

  ServiceListConsumer consumer;

  // Test specific match on both sides.
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("text/uri-list"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);

  // Test specific query, wildcard registration.
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("image/png"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);

  // Test wildcard query, specific registration.
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("text/*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[2], consumer.services_[0]);
  EXPECT_EQ(services[3], consumer.services_[1]);

  // Test wildcard query, wildcard registration.
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("image/*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(services[0], consumer.services_[0]);
  EXPECT_EQ(services[1], consumer.services_[1]);

  // Test "catch-all" query.
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(5U, consumer.services_.size());
  EXPECT_TRUE(consumer.ResultsContain(services[0]));
  EXPECT_TRUE(consumer.ResultsContain(services[1]));
  EXPECT_TRUE(consumer.ResultsContain(services[2]));
  EXPECT_TRUE(consumer.ResultsContain(services[3]));
  EXPECT_TRUE(consumer.ResultsContain(services[12]));

  // Test retrieve string literal match.
  registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"), ASCIIToUTF16("elsewhere"),
      base::Bind(&ServiceListConsumer::Accept,
                 base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(services[4], consumer.services_[0]);

  // Test retrieve MIME-looking type but actually isn't
  // doesn't wildcard match.
  registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("nota/mimetype"),
      base::Bind(&ServiceListConsumer::Accept,
                 base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(0U, consumer.services_.size());

  // Also a MIME-ish type that actually isn't.
  registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("x-/mimetype"),
      base::Bind(&ServiceListConsumer::Accept,
                 base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(0U, consumer.services_.size());

  // Extension MIME type will match wildcard.
  registry_.GetIntentServices(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("x-type/mimetype"),
      base::Bind(&ServiceListConsumer::Accept,
                       base::Unretained(&consumer)));
  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
}

TEST_F(WebIntentsRegistryTest, TestGetAllDefaultIntentServices) {
  DefaultWebIntentService s0;
  s0.action = ASCIIToUTF16("share");
  s0.type = ASCIIToUTF16("text/*");
  // Values here are just dummies to test for preservation.
  s0.user_date = 1;
  s0.suppression = 4;
  s0.service_url = "service_url";
  registry_.RegisterDefaultIntentService(s0);

  DefaultWebIntentService s1;
  s1.action = ASCIIToUTF16("pick");
  s1.type = ASCIIToUTF16("image/*");
  // Values here are just dummies to test for preservation.
  s1.user_date = 1;
  s1.suppression = 4;
  s1.service_url = "service_url";
  registry_.RegisterDefaultIntentService(s1);

  DefaultWebIntentService s2;
  s2.action = ASCIIToUTF16("save");
  s2.type = ASCIIToUTF16("application/*");
  // Values here are just dummies to test for preservation.
  s2.user_date = 1;
  s2.suppression = 4;
  s2.service_url = "service_url";
  registry_.RegisterDefaultIntentService(s2);

  DefaultServiceListConsumer consumer;

  registry_.GetAllDefaultIntentServices(
      base::Bind(&DefaultServiceListConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  EXPECT_TRUE(consumer.ResultsContain(s0));
  EXPECT_TRUE(consumer.ResultsContain(s1));
  EXPECT_TRUE(consumer.ResultsContain(s2));
}

TEST_F(WebIntentsRegistryTest, TestGetDefaults) {
  // Ignore QO-default related calls.
  EXPECT_CALL(extension_service_, GetInstalledExtension(testing::_)).
      WillRepeatedly(testing::ReturnNull());

  DefaultWebIntentService default_service;
  default_service.action = ASCIIToUTF16("share");
  default_service.type = ASCIIToUTF16("text/*");
  // Values here are just dummies to test for preservation.
  default_service.user_date = 1;
  default_service.suppression = 4;
  default_service.service_url = "service_url";
  registry_.RegisterDefaultIntentService(default_service);

  DefaultServiceConsumer consumer;

  // Test we can retrieve default entries by action.
  registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"), ASCIIToUTF16("text/plain"),
      GURL("http://www.google.com/"),
      base::Bind(&DefaultServiceConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  EXPECT_EQ("service_url", consumer.service_.service_url);
  EXPECT_EQ(1, consumer.service_.user_date);
  EXPECT_EQ(4, consumer.service_.suppression);

  // Can get for wildcard.
  consumer.service_ = DefaultWebIntentService();
  registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"),
      ASCIIToUTF16("text/*"),
      GURL("http://www.google.com/"),
      base::Bind(&DefaultServiceConsumer::Accept,
                 base::Unretained(&consumer)));
  consumer.WaitForData();
  EXPECT_EQ("service_url", consumer.service_.service_url);
  EXPECT_EQ(1, consumer.service_.user_date);
  EXPECT_EQ(4, consumer.service_.suppression);

  // Test that no action match means we don't retrieve any
  // default entries.
  consumer.service_ = DefaultWebIntentService();
  ASSERT_EQ("", consumer.service_.service_url);
  registry_.GetDefaultIntentService(
      ASCIIToUTF16("no-share"), ASCIIToUTF16("text/plain"),
      GURL("http://www.google.com/"),
      base::Bind(&DefaultServiceConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  EXPECT_EQ("", consumer.service_.service_url);

  // Test that no type match means we don't retrieve any
  // default entries (they get filtered out).
  consumer.service_ = DefaultWebIntentService();
  ASSERT_EQ("", consumer.service_.service_url);
  registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"), ASCIIToUTF16("image/plain"),
      GURL("http://www.google.com/"),
      base::Bind(&DefaultServiceConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  EXPECT_EQ("", consumer.service_.service_url);

  // Check that a string-literal type won't match.
  consumer.service_ = DefaultWebIntentService();
  ASSERT_EQ("", consumer.service_.service_url);
  registry_.GetDefaultIntentService(
      ASCIIToUTF16("share"),
      ASCIIToUTF16("literal"),
      GURL("http://www.google.com/"),
      base::Bind(&DefaultServiceConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  EXPECT_EQ("", consumer.service_.service_url);
}

// Verify that collapsing equivalent intents works properly.
TEST_F(WebIntentsRegistryTest, CollapseIntents) {
  WebIntentsRegistry::IntentServiceList services;

  // Add two intents with identical |service_url|, |title|, and |action|.
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("Image Sharing Service")));
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/jpg"),
      string16(),
      GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("Image Sharing Service")));
  // Service that differs in disposition.
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("Image Sharing Service")));
  ASSERT_EQ(WebIntentServiceData::DISPOSITION_WINDOW,
      services.back().disposition);
  services.back().disposition = WebIntentServiceData::DISPOSITION_INLINE;
  // Service that differs in title.
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("Sharing Service")));
  // Service that differs in |action|.
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share-old"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://somewhere.com/intent/share.html"),
      ASCIIToUTF16("Image Sharing Service")));
  // Service that differs in |service_url|.
  services.push_back(
      WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://zoo.com/share.html"),
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
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/png"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Image Sharing Service")),
    WebIntentServiceData(ASCIIToUTF16("http://webintents.org/share"),
                         ASCIIToUTF16("image/jpg"),
                         string16(),
                         GURL("http://somewhere.com/intent/share.html"),
                         ASCIIToUTF16("Image Sharing Service"))
  };
  registry_.RegisterIntentService(services[0]);
  registry_.RegisterIntentService(services[1]);

  ServiceListConsumer consumer;
  registry_.GetIntentServices(ASCIIToUTF16("http://webintents.org/share"),
                              ASCIIToUTF16("image/*"),
                              base::Bind(&ServiceListConsumer::Accept,
                                         base::Unretained(&consumer)));

  consumer.WaitForData();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(ASCIIToUTF16("image/png,image/jpg"), consumer.services_[0].type);
}

TEST_F(WebIntentsRegistryTest, UnregisterDefaultIntentServicesForServiceURL) {

  const GURL service_url_0("http://jibfest.com/dozer");
  const GURL service_url_1("http://kittyfizzer.com/fizz");

  DefaultWebIntentService s0;
  s0.action = ASCIIToUTF16("share");
  s0.type = ASCIIToUTF16("text/*");
  // Values here are just dummies to test for preservation.
  s0.user_date = 1;
  s0.suppression = 4;
  s0.service_url = service_url_0.spec();
  registry_.RegisterDefaultIntentService(s0);

  DefaultWebIntentService s1;
  s1.action = ASCIIToUTF16("whack");
  s1.type = ASCIIToUTF16("text/*");
  // Values here are just dummies to test for preservation.
  s1.user_date = 1;
  s1.suppression = 4;
  s1.service_url = service_url_1.spec();
  registry_.RegisterDefaultIntentService(s1);

  DefaultServiceListConsumer consumer;

  registry_.GetAllDefaultIntentServices(
      base::Bind(&DefaultServiceListConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  ASSERT_EQ(2U, consumer.services_.size());

  registry_.UnregisterServiceDefaults(service_url_0);
  MessageLoop::current()->RunUntilIdle();

  registry_.GetAllDefaultIntentServices(
      base::Bind(&DefaultServiceListConsumer::Accept,
                 base::Unretained(&consumer)));

  consumer.WaitForData();

  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ(service_url_1.spec(), consumer.services_[0].service_url);
}
