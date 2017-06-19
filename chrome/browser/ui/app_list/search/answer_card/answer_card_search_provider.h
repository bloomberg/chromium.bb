// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_contents.h"
#include "ui/app_list/search_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {
class AppListModel;
}

namespace net {
class HttpResponseHeaders;
}

namespace app_list {

// Search provider for the answer card.
class AnswerCardSearchProvider : public SearchProvider,
                                 public AnswerCardContents::Delegate {
 public:
  AnswerCardSearchProvider(Profile* profile,
                           app_list::AppListModel* model,
                           AppListControllerDelegate* list_controller,
                           std::unique_ptr<AnswerCardContents> contents);

  ~AnswerCardSearchProvider() override;

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override {}

  // AnswerCardContents::Delegate overrides:
  void UpdatePreferredSize(const gfx::Size& pref_size) override;
  content::WebContents* OpenURLFromTab(
      const content::OpenURLParams& params) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;

 private:
  bool IsCardSizeOk() const;
  void RecordReceivedAnswerFinalResult();
  void OnResultAvailable(bool is_available);
  bool ParseResponseHeaders(const net::HttpResponseHeaders* headers);

  // Unowned pointer to the associated profile.
  Profile* const profile_;

  // Unowned pointer to app list model.
  app_list::AppListModel* const model_;

  // Unowned pointer to app list controller.
  AppListControllerDelegate* const list_controller_;

  // The source of answer card contents.
  const std::unique_ptr<AnswerCardContents> contents_;

  // Whether have received a server response for the current query string, and
  // the response contains an answer.
  bool received_answer_ = false;

  // If valid, URL of the answer server. Otherwise, search answers are disabled.
  GURL answer_server_url_;

  // URL of the current answer server request.
  GURL current_request_url_;

  // Time when the current server request started.
  base::TimeTicks server_request_start_time_;

  // Time when the current server response loaded.
  base::TimeTicks answer_loaded_time_;

  // When in the dark run mode, indicates whether we mimic that the server
  // response contains an answer.
  bool dark_run_received_answer_ = false;

  // Url to open when the user clicks at the result.
  std::string result_url_;

  // Title of the result.
  std::string result_title_;

  // Current preferred size of the contents.
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_
