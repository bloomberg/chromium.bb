// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_

#include "base/macros.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/search_engines/template_url_service_client.h"

class TemplateURLService;

namespace history {
class HistoryService;
}

namespace ios {

// TemplateURLServiceClientImpl provides keyword related history functionality
// for TemplateURLService.
class TemplateURLServiceClientImpl : public TemplateURLServiceClient,
                                     public history::HistoryServiceObserver {
 public:
  explicit TemplateURLServiceClientImpl(
      history::HistoryService* history_service);
  ~TemplateURLServiceClientImpl() override;

 private:
  // TemplateURLServiceClient:
  void Shutdown() override;
  void SetOwner(TemplateURLService* owner) override;
  void DeleteAllSearchTermsForKeyword(history::KeywordID keyword_id) override;
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const base::string16& term) override;
  void AddKeywordGeneratedVisit(const GURL& url) override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;

  TemplateURLService* owner_;
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceClientImpl);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_
