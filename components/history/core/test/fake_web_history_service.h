// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_FAKE_WEB_HISTORY_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_FAKE_WEB_HISTORY_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/history/core/browser/web_history_service.h"
#include "url/gurl.h"

class OAuth2TokenService;
class SigninManagerBase;

namespace history {

// A fake WebHistoryService for testing.
//
// Use |AddSyncedVisit| to fill the fake server-side database of synced visits.
// Use |SetupFakeResponse| to influence whether the requests should succeed
// or fail, and with which error code.
//
// Note: The behavior of this class is only defined for some WebHistoryService
// queries. If needed, improve FakeRequest::GetResponseBody() to simulate
// responses for other queries.
//
// TODO(msramek): This class might need its own set of tests.
class FakeWebHistoryService : public history::WebHistoryService {
 public:
  FakeWebHistoryService(
      OAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  ~FakeWebHistoryService() override;

  // Sets up the behavior of the fake response returned when calling
  // |WebHistoryService::QueryHistory|; whether it will succeed, and with
  // which response code.
  void SetupFakeResponse(bool emulate_success, int emulate_response_code);

  // Adds a fake visit.
  void AddSyncedVisit(std::string url, base::Time timestamp);

  // Clears all fake visits.
  void ClearSyncedVisits();

  // Counts the number of visits within a certain time range.
  int GetNumberOfVisitsBetween(const base::Time& begin, const base::Time& end);

  // Get and set the fake state of web and app activity.
  bool IsWebAndAppActivityEnabled();
  void SetWebAndAppActivityEnabled(bool enabled);

  // Get and set the fake state of other forms of browsing history.
  bool AreOtherFormsOfBrowsingHistoryPresent();
  void SetOtherFormsOfBrowsingHistoryPresent(bool present);

 private:
  base::Time GetTimeForKeyInQuery(const GURL& url, const std::string& key);

  // WebHistoryService:
  Request* CreateRequest(const GURL& url,
                         const CompletionCallback& callback,
                         const net::PartialNetworkTrafficAnnotationTag&
                             partial_traffic_annotation) override;

  // Parameters for the fake request.
  bool emulate_success_;
  int emulate_response_code_;

  // States of serverside corpora.
  bool web_and_app_activity_enabled_;
  bool other_forms_of_browsing_history_present_;

  // Fake visits storage.
  typedef std::pair<std::string, base::Time> Visit;
  std::vector<Visit> visits_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebHistoryService);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_FAKE_WEB_HISTORY_SERVICE_H_
