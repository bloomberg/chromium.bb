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
   virtual void OnIntentsQueryDone(WebIntentsRegistry::QueryID id,
                                   const std::vector<WebIntentData>& intents) {
     DCHECK(id == expected_id_);
     intents_ = intents;

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
   std::vector<WebIntentData> intents_;  // Result data from callback.
};

TEST_F(WebIntentsRegistryTest, BasicTests) {
  WebIntentData intent;
  intent.service_url = GURL("http://google.com");
  intent.action = ASCIIToUTF16("share");
  intent.type = ASCIIToUTF16("image/*");
  intent.title = ASCIIToUTF16("Google's Sharing Service");

  registry_.RegisterIntentProvider(intent);

  intent.type = ASCIIToUTF16("video/*");
  registry_.RegisterIntentProvider(intent);

  intent.action = ASCIIToUTF16("search");
  registry_.RegisterIntentProvider(intent);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("share"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(2U, consumer.intents_.size());

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("search"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.intents_.size());

  intent.action = ASCIIToUTF16("share");
  intent.type = ASCIIToUTF16("image/*");
  registry_.UnregisterIntentProvider(intent);

  consumer.expected_id_ = registry_.GetIntentProviders(ASCIIToUTF16("share"),
                                                       &consumer);
  consumer.WaitForData();
  EXPECT_EQ(1U, consumer.intents_.size());
}

TEST_F(WebIntentsRegistryTest, GetAllIntents) {
  WebIntentData intent;
  intent.service_url = GURL("http://google.com");
  intent.action = ASCIIToUTF16("share");
  intent.type = ASCIIToUTF16("image/*");
  intent.title = ASCIIToUTF16("Google's Sharing Service");
  registry_.RegisterIntentProvider(intent);

  intent.action = ASCIIToUTF16("search");
  registry_.RegisterIntentProvider(intent);

  TestConsumer consumer;
  consumer.expected_id_ = registry_.GetAllIntentProviders(&consumer);
  consumer.WaitForData();
  ASSERT_EQ(2U, consumer.intents_.size());

  if (consumer.intents_[0].action != ASCIIToUTF16("share"))
    std::swap(consumer.intents_[0],consumer.intents_[1]);

  intent.action = ASCIIToUTF16("share");
  EXPECT_EQ(intent, consumer.intents_[0]);

  intent.action = ASCIIToUTF16("search");
  EXPECT_EQ(intent, consumer.intents_[1]);
}
