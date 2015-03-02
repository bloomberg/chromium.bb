// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/ios/distiller_page_factory_ios.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

namespace {
// A simple wrapper for DomDistillerService to expose it as a
// KeyedService.
class DomDistillerKeyedService
    : public KeyedService,
      public dom_distiller::DomDistillerService {
 public:
  DomDistillerKeyedService(
      scoped_ptr<dom_distiller::DomDistillerStoreInterface> store,
      scoped_ptr<dom_distiller::DistillerFactory> distiller_factory,
      scoped_ptr<dom_distiller::DistillerPageFactory> distiller_page_factory,
      scoped_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs)
      : DomDistillerService(store.Pass(),
                            distiller_factory.Pass(),
                            distiller_page_factory.Pass(),
                            distilled_page_prefs.Pass()) {}

  ~DomDistillerKeyedService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DomDistillerKeyedService);
};
}  // namespace

namespace dom_distiller {

// static
DomDistillerServiceFactory* DomDistillerServiceFactory::GetInstance() {
  return Singleton<DomDistillerServiceFactory>::get();
}

// static
DomDistillerService* DomDistillerServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<DomDistillerKeyedService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

DomDistillerServiceFactory::DomDistillerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DomDistillerService",
          BrowserStateDependencyManager::GetInstance()) {
}

DomDistillerServiceFactory::~DomDistillerServiceFactory() {
}

KeyedService* DomDistillerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      web::WebThread::GetBlockingPool()->GetSequencedTaskRunner(
          web::WebThread::GetBlockingPool()->GetSequenceToken());

  scoped_ptr<leveldb_proto::ProtoDatabaseImpl<ArticleEntry>> db(
      new leveldb_proto::ProtoDatabaseImpl<ArticleEntry>(
          background_task_runner));

  base::FilePath database_dir(
      browser_state->GetStatePath().Append(FILE_PATH_LITERAL("Articles")));

  scoped_ptr<DomDistillerStore> dom_distiller_store(
      new DomDistillerStore(db.Pass(), database_dir));

  scoped_ptr<DistillerPageFactory> distiller_page_factory(
      new DistillerPageFactoryIOS(browser_state));
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory(
      new DistillerURLFetcherFactory(browser_state->GetRequestContext()));

  dom_distiller::proto::DomDistillerOptions options;
  scoped_ptr<DistillerFactory> distiller_factory(
      new DistillerFactoryImpl(distiller_url_fetcher_factory.Pass(), options));
  scoped_ptr<DistilledPagePrefs> distilled_page_prefs(new DistilledPagePrefs(
      ios::ChromeBrowserState::FromBrowserState(browser_state)->GetPrefs()));

  DomDistillerKeyedService* service =
      new DomDistillerKeyedService(
          dom_distiller_store.Pass(), distiller_factory.Pass(),
          distiller_page_factory.Pass(), distilled_page_prefs.Pass());

  return service;
}

web::BrowserState* DomDistillerServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  // Makes normal profile and off-the-record profile use same service instance.
  return GetBrowserStateRedirectedInIncognito(browser_state);
}

}  // namespace dom_distiller
