// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_

#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HistoryService;
class Profile;

// ChromeTemplateURLServiceClient provides keyword related history
// functionality for TemplateURLService.
class ChromeTemplateURLServiceClient : public TemplateURLServiceClient,
                                       public content::NotificationObserver {
 public:
  explicit ChromeTemplateURLServiceClient(Profile* profile);
  virtual ~ChromeTemplateURLServiceClient();

  // TemplateURLServiceClient:
  virtual void SetOwner(TemplateURLService* owner) OVERRIDE;
  virtual void DeleteAllSearchTermsForKeyword(
      history::KeywordID keyword_Id) OVERRIDE;
  virtual void SetKeywordSearchTermsForURL(const GURL& url,
                                           TemplateURLID id,
                                           const base::string16& term) OVERRIDE;
  virtual void AddKeywordGeneratedVisit(const GURL& url) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Profile* profile_;
  TemplateURLService* owner_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTemplateURLServiceClient);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
