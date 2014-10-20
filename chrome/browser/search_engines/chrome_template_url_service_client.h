// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_

#include "components/history/core/browser/history_service_observer.h"
#include "components/search_engines/template_url_service_client.h"

class HistoryService;

// ChromeTemplateURLServiceClient provides keyword related history
// functionality for TemplateURLService.
class ChromeTemplateURLServiceClient : public TemplateURLServiceClient,
                                       public history::HistoryServiceObserver {
 public:
  explicit ChromeTemplateURLServiceClient(HistoryService* history_service);
  virtual ~ChromeTemplateURLServiceClient();

  // TemplateURLServiceClient:
  virtual void Shutdown() override;
  virtual void SetOwner(TemplateURLService* owner) override;
  virtual void DeleteAllSearchTermsForKeyword(
      history::KeywordID keyword_Id) override;
  virtual void SetKeywordSearchTermsForURL(const GURL& url,
                                           TemplateURLID id,
                                           const base::string16& term) override;
  virtual void AddKeywordGeneratedVisit(const GURL& url) override;
  virtual void RestoreExtensionInfoIfNecessary(
      TemplateURL* template_url) override;

  // history::HistoryServiceObserver:
  virtual void OnURLVisited(HistoryService* history_service,
                            ui::PageTransition transition,
                            const history::URLRow& row,
                            const history::RedirectList& redirects,
                            base::Time visit_time) override;

 private:
  TemplateURLService* owner_;
  HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTemplateURLServiceClient);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
