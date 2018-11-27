// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_answer_card_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

constexpr char kSearchAnswerHasResult[] = "SearchAnswer-HasResult";
constexpr char kSearchAnswerIssuedQuery[] = "SearchAnswer-IssuedQuery";
constexpr char kSearchAnswerTitle[] = "SearchAnswer-Title";

// Exclude the card native view from event handling.
void ExcludeCardFromEventHandling(gfx::NativeView card_native_view) {
  // |card_native_view| could be null in tests.
  if (!card_native_view)
    return;

  if (!card_native_view->parent()) {
    DLOG(ERROR) << "Card is not attached to the app list view.";
    return;
  }

  // |card_native_view| is brought into View's hierarchy via a NativeViewHost.
  // The window hierarchy looks like this:
  //   widget window -> clipping window -> content_native_view
  // Events should be targeted to the widget window and by-passing the sub tree
  // started at clipping window. Walking up the window hierarchy to find the
  // clipping window and make the cut there.
  aura::Window* top_level = card_native_view->GetToplevelWindow();
  DCHECK(top_level);
  aura::Window* window = card_native_view;
  while (window->parent() != top_level)
    window = window->parent();

  AppListView::ExcludeWindowFromEventHandling(window);
}

bool ParseResponseHeaders(const net::HttpResponseHeaders* headers,
                          std::string* title,
                          std::string* issued_query) {
  if (!headers || headers->response_code() != net::HTTP_OK)
    return false;

  if (!headers->HasHeaderValue(kSearchAnswerHasResult, "true")) {
    DLOG(WARNING) << "Response not an answer card. Expected a value of \"true\""
                  << " for " << kSearchAnswerHasResult << " header.";
    return false;
  }
  if (!headers->GetNormalizedHeader(kSearchAnswerTitle, title) ||
      title->empty()) {
    DLOG(WARNING) << "Ignoring answer card response with no valid "
                  << kSearchAnswerTitle << " header present.";
    return false;
  }
  if (!headers->GetNormalizedHeader(kSearchAnswerIssuedQuery, issued_query) ||
      issued_query->empty()) {
    DLOG(WARNING) << "Ignoring answer card response with no valid "
                  << kSearchAnswerIssuedQuery << " header present.";
    return false;
  }

  return true;
}

}  // namespace

// Container of the search answer view.
class SearchResultAnswerCardView::SearchAnswerContainerView
    : public SearchResultBaseView,
      public content::NavigableContentsObserver {
 public:
  SearchAnswerContainerView(SearchResultContainerView* container,
                            AppListViewDelegate* view_delegate)
      : container_(container), view_delegate_(view_delegate) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // Center the card horizontally in the container. Padding is set on the
    // server.
    auto answer_container_layout =
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
    answer_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    SetLayoutManager(std::move(answer_container_layout));

    view_delegate_->GetNavigableContentsFactory(
        mojo::MakeRequest(&contents_factory_));

    auto params = content::mojom::NavigableContentsParams::New();
    params->enable_view_auto_resize = true;
    params->suppress_navigations = true;
    contents_ = std::make_unique<content::NavigableContents>(
        contents_factory_.get(), std::move(params));
    contents_->AddObserver(this);
  }

  ~SearchAnswerContainerView() override {
    contents_->RemoveObserver(this);
    if (search_result_)
      search_result_->RemoveObserver(this);
  }

  bool has_valid_answer_card() const {
    return is_current_navigation_valid_answer_card_;
  }

  void HideCard() {
    OnVisibilityChanged(false /* is_visible */);
    RemoveAllChildViews(false /* delete_children */);
    SetPreferredSize(gfx::Size{0, 0});

    // Force any future result changes to initiate another navigation.
    is_current_navigation_valid_answer_card_ = false;
  }

  void SetSearchResult(SearchResult* search_result) {
    // Remove the card contents from the UI temporarily while we attempt to
    // navigate it to the new query URL.
    base::Optional<GURL> previous_url;
    if (search_result_) {
      previous_url = search_result_->query_url();
      search_result_->RemoveObserver(this);
      search_result_ = nullptr;
    }

    if (!search_result || !search_result->query_url()) {
      HideCard();
      return;
    }

    search_result_ = search_result;
    search_result_->AddObserver(this);

    if (search_result_->query_url() == previous_url &&
        is_current_navigation_valid_answer_card_) {
      // The new search result is for a query URL identical to the previous one,
      // so we don't bother hiding or navigating the existing card contents.
      return;
    }

    // We hide the view while navigating its contents. Once navigation is
    // finished and we (possibly) have a valid answer card response, the
    // contents view will be re-parented to this container.
    HideCard();

    base::RecordAction(base::UserMetricsAction("SearchAnswer_UserInteraction"));

    contents_->Navigate(*search_result_->query_url());
  }

  void OnVisibilityChanged(bool is_visible) {
    if (is_visible && !last_shown_time_) {
      last_shown_time_ = base::Time::Now();
    } else if (last_shown_time_) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SearchAnswer.AnswerVisibleTime",
                                 base::Time::Now() - *last_shown_time_);
      last_shown_time_.reset();
    }
  }

  // views::Button overrides:
  const char* GetClassName() const override {
    return "SearchAnswerContainerView";
  }

  void OnBlur() override { SetBackgroundHighlighted(false); }

  void OnFocus() override {
    ScrollRectToVisible(GetLocalBounds());
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
    SetBackgroundHighlighted(true);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_SPACE) {
      // Shouldn't eat Space; we want Space to go to the search box.
      return false;
    }

    return Button::OnKeyPressed(event);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Default button role is atomic for ChromeVox, so assign a generic
    // container role to allow accessibility focus to get into this view.
    node_data->role = ax::mojom::Role::kGenericContainer;
    node_data->SetName(GetAccessibleName());
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (background_highlighted())
      canvas->FillRect(GetContentsBounds(), kAnswerCardSelectedColor);
  }

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == this);
    if (search_result_) {
      RecordSearchResultOpenSource(search_result_, view_delegate_->GetModel(),
                                   view_delegate_->GetSearchModel());
      view_delegate_->OpenSearchResult(search_result_->id(), event.flags());
    }
  }

  // SearchResultObserver overrides:
  void OnResultDestroying() override {
    HideCard();
    search_result_ = nullptr;
  }

 private:
  // content::NavigableContentsObserver overrides:
  void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const net::HttpResponseHeaders* response_headers) override {
    if (!is_main_frame)
      return;

    is_current_navigation_valid_answer_card_ = false;
    if (is_error_page)
      return;

    std::string title;
    if (!ParseResponseHeaders(response_headers, &title, &answer_card_query_))
      return;

    SetAccessibleName(base::UTF8ToUTF16(title));

    // TODO(https://crbug.com/894893): Consider where, how to record some
    // SearchAnswer metrics regarding navigation results and timing.

    is_current_navigation_valid_answer_card_ = true;
    answer_card_url_ = url;
  }

  void DidStopLoading() override {
    if (!is_current_navigation_valid_answer_card_)
      return;

    OnVisibilityChanged(true /* is_visible */);
    views::View* content_view = contents_->GetView()->view();
    if (!has_children()) {
      AddChildView(content_view);
      ExcludeCardFromEventHandling(contents_->GetView()->native_view());

      if (search_result_->equivalent_result_id().has_value()) {
        view_delegate_->GetSearchModel()->DeleteResultById(
            search_result_->equivalent_result_id().value());
      }
    }
    SetPreferredSize(content_view->GetPreferredSize());
    container_->Update();
  }

  void DidAutoResizeView(const gfx::Size& new_size) override {
    SetPreferredSize(new_size);
    contents_->GetView()->view()->SetPreferredSize(new_size);
    container_->Update();
  }

  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override {
    if (!from_user_gesture)
      return;

    // NOTE: We shouldn't ever hit this path since all user gestures targeting
    // the content area should be intercepted by the Button which overlaps its
    // display region, and answer cards are generally not expected to elicit
    // scripted navigations. This action is recorded here to verify these
    // expectations.
    base::RecordAction(base::UserMetricsAction("SearchAnswer_OpenedUrl"));
  }

  SearchResultContainerView* const container_;  // Not owned.
  AppListViewDelegate* const view_delegate_;    // Not owned.
  SearchResult* search_result_ = nullptr;       // Not owned.
  content::mojom::NavigableContentsFactoryPtr contents_factory_;
  std::unique_ptr<content::NavigableContents> contents_;

  bool is_current_navigation_valid_answer_card_ = false;
  GURL answer_card_url_;
  std::string answer_card_query_;

  // Tracks the last time this view was made visible, if still visible.
  base::Optional<base::Time> last_shown_time_;

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerContainerView);
};

SearchResultAnswerCardView::SearchResultAnswerCardView(
    AppListViewDelegate* view_delegate)
    : search_answer_container_view_(
          new SearchAnswerContainerView(this, view_delegate)) {
  AddChildView(search_answer_container_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

SearchResultAnswerCardView::~SearchResultAnswerCardView() {}

const char* SearchResultAnswerCardView::GetClassName() const {
  return "SearchResultAnswerCardView";
}

int SearchResultAnswerCardView::GetYSize() {
  return num_results();
}

int SearchResultAnswerCardView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kCard, /*excludes=*/{}, 1);
  SearchResult* top_result =
      display_results.empty() ? nullptr : display_results.front();

  const bool has_valid_answer_card =
      search_answer_container_view_->has_valid_answer_card();
  search_answer_container_view_->SetSearchResult(top_result);
  parent()->SetVisible(has_valid_answer_card);

  set_container_score(
      has_valid_answer_card && top_result ? top_result->display_score() : 0);
  if (top_result)
    top_result->set_is_visible(has_valid_answer_card);

  return has_valid_answer_card ? 1 : 0;
}

bool SearchResultAnswerCardView::OnKeyPressed(const ui::KeyEvent& event) {
  if (search_answer_container_view_->OnKeyPressed(event))
    return true;

  return SearchResultContainerView::OnKeyPressed(event);
}

SearchResultBaseView* SearchResultAnswerCardView::GetFirstResultView() {
  return num_results() <= 0 ? nullptr : search_answer_container_view_;
}

views::View* SearchResultAnswerCardView::GetSearchAnswerContainerViewForTest()
    const {
  return search_answer_container_view_;
}

// static
scoped_refptr<net::HttpResponseHeaders>
SearchResultAnswerCardView::CreateAnswerCardResponseHeadersForTest(
    const std::string& query,
    const std::string& title) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader(base::StrCat({kSearchAnswerHasResult, ": true"}));
  headers->AddHeader(base::StrCat({kSearchAnswerTitle, ": ", title.c_str()}));
  headers->AddHeader(
      base::StrCat({kSearchAnswerIssuedQuery, ": ", query.c_str()}));
  return headers;
}

}  // namespace app_list
