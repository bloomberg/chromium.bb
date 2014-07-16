// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/extensions/api/reading_list_private/reading_list_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/pref_registry/testing_pref_service_syncable.h"

using dom_distiller::ArticleEntry;
using dom_distiller::test::FakeDistiller;
using dom_distiller::test::util::CreateStoreWithFakeDB;
using dom_distiller::DomDistillerContextKeyedService;
using dom_distiller::DomDistillerService;
using dom_distiller::DistillerFactory;
using dom_distiller::DistillerPageFactory;
using dom_distiller::DomDistillerStoreInterface;
using dom_distiller::test::MockDistillerFactory;
using dom_distiller::test::MockDistillerPage;
using dom_distiller::test::MockDistillerPageFactory;
using leveldb_proto::test::FakeDB;

class ReadingListPrivateApiTest : public ExtensionApiTest {
 public:
  static KeyedService* Build(content::BrowserContext* context) {
    FakeDB<ArticleEntry>* fake_db =
        new FakeDB<ArticleEntry>(new FakeDB<ArticleEntry>::EntryMap);
    FakeDistiller* distiller = new FakeDistiller(true);
    MockDistillerPage* distiller_page = new MockDistillerPage();
    MockDistillerFactory* distiller_factory = new MockDistillerFactory();
    MockDistillerPageFactory* distiller_page_factory =
        new MockDistillerPageFactory();

    // Setting up pref service for DistilledPagePrefs.
    user_prefs::TestingPrefServiceSyncable* pref_service =
        new user_prefs::TestingPrefServiceSyncable();
    dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(
        pref_service->registry());

    DomDistillerContextKeyedService* service =
        new DomDistillerContextKeyedService(
            scoped_ptr<DomDistillerStoreInterface>(
                CreateStoreWithFakeDB(fake_db,
                                      FakeDB<ArticleEntry>::EntryMap())),
            scoped_ptr<DistillerFactory>(distiller_factory),
            scoped_ptr<DistillerPageFactory>(distiller_page_factory),
            scoped_ptr<dom_distiller::DistilledPagePrefs>(
                new dom_distiller::DistilledPagePrefs(pref_service)));
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
    EXPECT_CALL(*distiller_factory, CreateDistillerImpl())
        .WillOnce(testing::Return(distiller));
    EXPECT_CALL(*distiller_page_factory, CreateDistillerPageImpl())
        .WillOnce(testing::Return(distiller_page));
    return service;
  }
};

IN_PROC_BROWSER_TEST_F(ReadingListPrivateApiTest, ReadingListPrivate) {
  dom_distiller::DomDistillerServiceFactory::GetInstance()->SetTestingFactory(
      profile(), &ReadingListPrivateApiTest::Build);
  ASSERT_TRUE(RunComponentExtensionTest("reading_list_private")) << message_;
}
