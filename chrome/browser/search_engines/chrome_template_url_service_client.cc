// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/chrome_template_url_service_client.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/constants.h"

ChromeTemplateURLServiceClient::ChromeTemplateURLServiceClient(Profile* profile)
    : profile_(profile),
      owner_(NULL) {
  DCHECK(profile);

  // Register for notifications.
  // TODO(sky): bug 1166191. The keywords should be moved into the history
  // db, which will mean we no longer need this notification and the history
  // backend can handle automatically adding the search terms as the user
  // navigates.
  content::Source<Profile> profile_source(profile->GetOriginalProfile());
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
                              profile_source);
}

ChromeTemplateURLServiceClient::~ChromeTemplateURLServiceClient() {
}

void ChromeTemplateURLServiceClient::SetOwner(TemplateURLService* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void ChromeTemplateURLServiceClient::DeleteAllSearchTermsForKeyword(
    TemplateURLID id) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->DeleteAllSearchTermsForKeyword(id);
}

void ChromeTemplateURLServiceClient::SetKeywordSearchTermsForURL(
    const GURL& url,
    TemplateURLID id,
    const base::string16& term) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->SetKeywordSearchTermsForURL(url, id, term);
}

void ChromeTemplateURLServiceClient::AddKeywordGeneratedVisit(const GURL& url) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->AddPage(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false);
}

void ChromeTemplateURLServiceClient::RestoreExtensionInfoIfNecessary(
    TemplateURL* template_url) {
  const TemplateURLData& data = template_url->data();
  GURL url(data.url());
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    const std::string& extension_id = url.host();
    template_url->set_extension_info(make_scoped_ptr(
        new TemplateURL::AssociatedExtensionInfo(
            TemplateURL::OMNIBOX_API_EXTENSION, extension_id)));
  }
}

void ChromeTemplateURLServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_HISTORY_URL_VISITED);

  if (!owner_)
    return;

  content::Details<history::URLVisitedDetails> history_details(details);
  TemplateURLService::URLVisitedDetails visited_details;
  visited_details.url = history_details->row.url();
  visited_details.is_keyword_transition =
      content::PageTransitionStripQualifier(history_details->transition) ==
      content::PAGE_TRANSITION_KEYWORD;
  owner_->OnHistoryURLVisited(visited_details);
}
