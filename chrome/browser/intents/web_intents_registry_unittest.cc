// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    registry_.Initialize(wds_);
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
