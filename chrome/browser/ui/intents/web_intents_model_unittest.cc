// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/ui/intents/web_intents_model.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_model.h"

using content::BrowserThread;

class WebIntentsModelTest : public testing::Test {
 public:
  WebIntentsModelTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

 protected:
  virtual void SetUp() {
    db_thread_.Start();
    wds_ = new WebDataService();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    wds_->Init(temp_dir_.path());

    registry_.Initialize(wds_, NULL);
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  void LoadRegistry() {
    {
      webkit_glue::WebIntentServiceData service;
      service.service_url = GURL("http://www.google.com/share");
      service.action = ASCIIToUTF16("SHARE");
      service.type = ASCIIToUTF16("text/url");
      service.title = ASCIIToUTF16("Google");
      registry_.RegisterIntentService(service);
    }
    {
      webkit_glue::WebIntentServiceData service;
      service.service_url = GURL("http://picasaweb.google.com/share");
      service.action = ASCIIToUTF16("EDIT");
      service.type = ASCIIToUTF16("image/*");
      service.title = ASCIIToUTF16("Picasa");
      registry_.RegisterIntentService(service);
    }
    {
      webkit_glue::WebIntentServiceData service;
      service.service_url = GURL("http://www.digg.com/share");
      service.action = ASCIIToUTF16("SHARE");
      service.type = ASCIIToUTF16("text/url");
      service.title = ASCIIToUTF16("Digg");
      registry_.RegisterIntentService(service);
    }
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_refptr<WebDataService> wds_;
  WebIntentsRegistry registry_;
  ScopedTempDir temp_dir_;
};

class WaitingWebIntentsObserver : public WebIntentsModel::Observer {
 public:
  WaitingWebIntentsObserver() : event_(true, false), added_(0) {}

  virtual void TreeModelBeginBatch(WebIntentsModel* model) {}

  virtual void TreeModelEndBatch(WebIntentsModel* model) {
    event_.Signal();
    MessageLoop::current()->Quit();
  }

  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count) {
    added_++;
  }

  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* node,
                                int start,
                                int count) {
  }

  virtual void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) {
  }

  void Wait() {
    MessageLoop::current()->Run();
    event_.Wait();
  }

  base::WaitableEvent event_;
  int added_;
};

TEST_F(WebIntentsModelTest, NodeIDs) {
  LoadRegistry();
  WaitingWebIntentsObserver obs;
  WebIntentsModel intents_model(&registry_);
  intents_model.AddWebIntentsTreeObserver(&obs);
  obs.Wait();

  WebIntentsTreeNode* n1 = new WebIntentsTreeNode(ASCIIToUTF16("origin"));
  intents_model.Add(intents_model.GetRoot(), n1,
                    intents_model.GetRoot()->child_count());
  EXPECT_EQ(ASCIIToUTF16("origin"), intents_model.GetTreeNodeId(n1));

  WebIntentsTreeNode* ncheck = intents_model.GetTreeNode("origin");
  EXPECT_EQ(ncheck, n1);

  base::ListValue nodes;
  intents_model.GetChildNodeList(
      intents_model.GetTreeNode("www.google.com"), 0, 1, &nodes);
  EXPECT_EQ(static_cast<size_t>(1), nodes.GetSize());
  base::DictionaryValue* dict;
  EXPECT_TRUE(nodes.GetDictionary(0, &dict));

  std::string val;
  EXPECT_TRUE(dict->GetString("site", &val));
  EXPECT_EQ("www.google.com", val);
  EXPECT_TRUE(dict->GetString("name", &val));
  EXPECT_EQ("Google", val);
  EXPECT_TRUE(dict->GetString("url", &val));
  EXPECT_EQ("http://www.google.com/share", val);
  EXPECT_TRUE(dict->GetString("icon", &val));
  EXPECT_EQ("http://www.google.com/favicon.ico", val);
  base::ListValue* types_list;
  EXPECT_TRUE(dict->GetList("types", &types_list));
  EXPECT_EQ(static_cast<size_t>(1), types_list->GetSize());
  EXPECT_TRUE(types_list->GetString(0, &val));
  EXPECT_EQ("text/url", val);
  bool bval;
  EXPECT_TRUE(dict->GetBoolean("blocked", &bval));
  EXPECT_FALSE(bval);
  EXPECT_TRUE(dict->GetBoolean("disabled", &bval));
  EXPECT_FALSE(bval);
}

TEST_F(WebIntentsModelTest, LoadFromWebData) {
  LoadRegistry();
  WaitingWebIntentsObserver obs;
  WebIntentsModel intents_model(&registry_);
  intents_model.AddWebIntentsTreeObserver(&obs);
  obs.Wait();
  EXPECT_EQ(6, obs.added_);

  WebIntentsTreeNode* node = intents_model.GetTreeNode("www.google.com");
  ASSERT_NE(static_cast<WebIntentsTreeNode*>(NULL), node);
  EXPECT_EQ(WebIntentsTreeNode::TYPE_ORIGIN, node->Type());
  EXPECT_EQ(ASCIIToUTF16("www.google.com"), node->GetTitle());
  EXPECT_EQ(1, node->child_count());
  node = node->GetChild(0);
  ASSERT_EQ(WebIntentsTreeNode::TYPE_SERVICE, node->Type());
  ServiceTreeNode* snode = static_cast<ServiceTreeNode*>(node);
  EXPECT_EQ(ASCIIToUTF16("Google"), snode->ServiceName());
  EXPECT_EQ(ASCIIToUTF16("SHARE"), snode->Action());
  EXPECT_EQ(ASCIIToUTF16("http://www.google.com/share"), snode->ServiceUrl());
  EXPECT_EQ(static_cast<size_t>(1), snode->Types().GetSize());
  string16 stype;
  ASSERT_TRUE(snode->Types().GetString(0, &stype));
  EXPECT_EQ(ASCIIToUTF16("text/url"), stype);

  node = intents_model.GetTreeNode("www.digg.com");
  ASSERT_NE(static_cast<WebIntentsTreeNode*>(NULL), node);
  EXPECT_EQ(WebIntentsTreeNode::TYPE_ORIGIN, node->Type());
  EXPECT_EQ(ASCIIToUTF16("www.digg.com"), node->GetTitle());
}

TEST_F(WebIntentsModelTest, TestMultipleIntentsOnHost) {
  LoadRegistry();
  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("http://www.google.com/xedit");
  service.action = ASCIIToUTF16("XEDIT");
  service.type = ASCIIToUTF16("text/plain");
  service.title = ASCIIToUTF16("XEdit");
  registry_.RegisterIntentService(service);

  WaitingWebIntentsObserver obs;
  WebIntentsModel intents_model(&registry_);
  intents_model.AddWebIntentsTreeObserver(&obs);
  obs.Wait();
  EXPECT_EQ(7, obs.added_);

  WebIntentsTreeNode* node = intents_model.GetTreeNode("www.google.com");
  ASSERT_EQ(2, node->child_count());
  node = node->GetChild(1);
  ASSERT_EQ(WebIntentsTreeNode::TYPE_SERVICE, node->Type());
  ASSERT_EQ(WebIntentsTreeNode::TYPE_SERVICE, node->Type());
  ServiceTreeNode* snode = static_cast<ServiceTreeNode*>(node);
  EXPECT_EQ(ASCIIToUTF16("XEDIT"), snode->Action());
  EXPECT_EQ(ASCIIToUTF16("http://www.google.com/xedit"), snode->ServiceUrl());
}
