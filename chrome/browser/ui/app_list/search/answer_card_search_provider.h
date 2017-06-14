// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/app_list/search_provider.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {
class AppListModel;
}

namespace net {
class HttpResponseHeaders;
}

namespace views {
class View;
class WebView;
}

namespace app_list {

// Search provider for the answer card.
class AnswerCardSearchProvider : public SearchProvider,
                                 public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  AnswerCardSearchProvider(Profile* profile,
                           app_list::AppListModel* model,
                           AppListControllerDelegate* list_controller);

  ~AnswerCardSearchProvider() override;

  // Returns a pointer to the web view for the web contents managed by this
  // class. The object is owned by this class and has property
  // 'set_owned_by_client()' set.
  views::View* web_view();

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override {}

  // content::WebContentsDelegate overrides:
  void UpdatePreferredSize(content::WebContents* web_contents,
                           const gfx::Size& pref_size) override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

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

  // Web view for the web contents managed by this class.
  const std::unique_ptr<views::WebView> web_view_;

  // Whether have received a server response for the current query string, and
  // the response contains an answer.
  bool received_answer_ = false;

  // Web contents managed by this class.
  const std::unique_ptr<content::WebContents> web_contents_;

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

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_SEARCH_PROVIDER_H_
