// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/template_url_service_client_impl.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/page_transition_types.h"

namespace ios {

TemplateURLServiceClientImpl::TemplateURLServiceClientImpl(
    history::HistoryService* history_service)
    : owner_(nullptr), history_service_(history_service) {
  // TODO(sky): bug 1166191. The keywords should be moved into the history
  // db, which will mean we no longer need this notification and the history
  // backend can handle automatically adding the search terms as the user
  // navigates.
  if (history_service_)
    history_service_->AddObserver(this);
}

TemplateURLServiceClientImpl::~TemplateURLServiceClientImpl() {}

void TemplateURLServiceClientImpl::Shutdown() {
  // TemplateURLServiceClientImpl is owned by TemplateURLService which is a
  // KeyedService with a dependency on HistoryService, thus |history_service_|
  // outlives the ChromeTemplateURLServiceClient.
  //
  // Remove self from |history_service_| observers in the shutdown phase of the
  // two-phases since KeyedService are not supposed to use a dependend service
  // after the Shutdown call.
  if (history_service_) {
    history_service_->RemoveObserver(this);
    history_service_ = nullptr;
  }
}

void TemplateURLServiceClientImpl::SetOwner(TemplateURLService* owner) {
  DCHECK(owner);
  DCHECK(!owner_);
  owner_ = owner;
}

void TemplateURLServiceClientImpl::DeleteAllSearchTermsForKeyword(
    history::KeywordID keyword_id) {
  if (history_service_)
    history_service_->DeleteAllSearchTermsForKeyword(keyword_id);
}

void TemplateURLServiceClientImpl::SetKeywordSearchTermsForURL(
    const GURL& url,
    TemplateURLID id,
    const base::string16& term) {
  if (history_service_)
    history_service_->SetKeywordSearchTermsForURL(url, id, term);
}

void TemplateURLServiceClientImpl::AddKeywordGeneratedVisit(const GURL& url) {
  if (history_service_) {
    history_service_->AddPage(
        url, base::Time::Now(), nullptr, 0, GURL(), history::RedirectList(),
        ui::PAGE_TRANSITION_KEYWORD_GENERATED, history::SOURCE_BROWSED, false);
  }
}

void TemplateURLServiceClientImpl::OnURLVisited(
    history::HistoryService* history_service,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  DCHECK_EQ(history_service, history_service_);
  if (!owner_)
    return;

  TemplateURLService::URLVisitedDetails details = {
      row.url(),
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_KEYWORD),
  };
  owner_->OnHistoryURLVisited(details);
}

}  // namespace ios
