// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/escape.h"
#include "third_party/icu/public/common/unicode/normalizer2.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

// An artificial delay (in milliseconds) we introduce before telling the Instant
// page about the new omnibox bounds, in cases where the bounds shrink. This is
// to avoid the page jumping up/down very fast in response to bounds changes.
const int kUpdateBoundsDelayMS = 1000;

// The maximum number of times we'll load a non-Instant-supporting search engine
// before we give up and blacklist it for the rest of the browsing session.
const int kMaxInstantSupportFailures = 10;

// For reporting events of interest.
enum InstantControllerEvent {
  INSTANT_CONTROLLER_EVENT_URL_ADDED_TO_BLACKLIST = 0,
  INSTANT_CONTROLLER_EVENT_URL_REMOVED_FROM_BLACKLIST = 1,
  INSTANT_CONTROLLER_EVENT_URL_BLOCKED_BY_BLACKLIST = 2,
  INSTANT_CONTROLLER_EVENT_MAX = 3,
};

void RecordEventHistogram(InstantControllerEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Instant.InstantControllerEvent",
                            event,
                            INSTANT_CONTROLLER_EVENT_MAX);
}

void AddSessionStorageHistogram(bool extended_enabled,
                                const content::WebContents* tab1,
                                const content::WebContents* tab2) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      std::string("Instant.SessionStorageNamespace") +
          (extended_enabled ? "_Extended" : "_Instant"),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  const content::SessionStorageNamespaceMap& session_storage_map1 =
      tab1->GetController().GetSessionStorageNamespaceMap();
  const content::SessionStorageNamespaceMap& session_storage_map2 =
      tab2->GetController().GetSessionStorageNamespaceMap();
  bool is_session_storage_the_same =
      session_storage_map1.size() == session_storage_map2.size();
  if (is_session_storage_the_same) {
    // The size is the same, so let's check that all entries match.
    for (content::SessionStorageNamespaceMap::const_iterator
             it1 = session_storage_map1.begin(),
             it2 = session_storage_map2.begin();
         it1 != session_storage_map1.end() && it2 != session_storage_map2.end();
         ++it1, ++it2) {
      if (it1->first != it2->first || it1->second != it2->second) {
        is_session_storage_the_same = false;
        break;
      }
    }
  }
  histogram->AddBoolean(is_session_storage_the_same);
}

string16 Normalize(const string16& str) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer =
      icu::Normalizer2::getInstance(NULL, "nfkc_cf", UNORM2_COMPOSE, status);
  if (normalizer == NULL || U_FAILURE(status))
    return str;
  icu::UnicodeString norm_str(normalizer->normalize(
      icu::UnicodeString(FALSE, str.c_str(), str.size()), status));
  if (U_FAILURE(status))
    return str;
  return string16(norm_str.getBuffer(), norm_str.length());
}

bool NormalizeAndStripPrefix(string16* text, const string16& prefix) {
  string16 norm_prefix = Normalize(prefix);
  string16 norm_text = Normalize(*text);
  if (norm_prefix.size() <= norm_text.size() &&
      norm_text.compare(0, norm_prefix.size(), norm_prefix) == 0) {
    *text = norm_text.erase(0, norm_prefix.size());
    return true;
  }
  return false;
}

// For TOOLKIT_VIEWS, the top level widget is always focused. If the focus
// change originated in views determine the child Widget from the view that is
// being focused.
gfx::NativeView GetViewGainingFocus(gfx::NativeView view_gaining_focus) {
#if defined(TOOLKIT_VIEWS)
  views::Widget* widget = view_gaining_focus ?
      views::Widget::GetWidgetForNativeView(view_gaining_focus) : NULL;
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    if (focus_manager && focus_manager->is_changing_focus() &&
        focus_manager->GetFocusedView() &&
        focus_manager->GetFocusedView()->GetWidget())
      return focus_manager->GetFocusedView()->GetWidget()->GetNativeView();
  }
#endif
  return view_gaining_focus;
}

// Returns true if |view| is the top-level contents view or a child view in the
// view hierarchy of |contents|.
bool IsViewInContents(gfx::NativeView view, content::WebContents* contents) {
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  if (!view || !rwhv)
    return false;

  gfx::NativeView tab_view = contents->GetView()->GetNativeView();
  if (view == rwhv->GetNativeView() || view == tab_view)
    return true;

  // Walk up the view hierarchy to determine if the view is a subview of the
  // WebContents view (such as a windowed plugin or http auth dialog).
  while (view) {
    view = platform_util::GetParent(view);
    if (view == tab_view)
      return true;
  }

  return false;
}

bool IsFullHeight(const InstantOverlayModel& model) {
  return model.height() == 100 && model.height_units() == INSTANT_SIZE_PERCENT;
}

bool IsContentsFrom(const InstantPage* page,
                    const content::WebContents* contents) {
  return page && (page->contents() == contents);
}

// Adds a transient NavigationEntry to the supplied |contents|'s
// NavigationController if the page's URL has not already been updated with the
// supplied |search_terms|. Sets the |search_terms| on the transient entry for
// search terms extraction to work correctly.
void EnsureSearchTermsAreSet(content::WebContents* contents,
                             const string16& search_terms) {
  content::NavigationController* controller = &contents->GetController();

  // If search terms are already correct or there is already a transient entry
  // (there shouldn't be), bail out early.
  if (chrome::search::GetSearchTerms(contents) == search_terms ||
      controller->GetTransientEntry())
    return;

  const content::NavigationEntry* active_entry = controller->GetActiveEntry();
  content::NavigationEntry* transient = controller->CreateNavigationEntry(
      active_entry->GetURL(),
      active_entry->GetReferrer(),
      active_entry->GetTransitionType(),
      false,
      std::string(),
      contents->GetBrowserContext());
  transient->SetExtraData(chrome::search::kInstantExtendedSearchTermsKey,
                          search_terms);
  controller->SetTransientEntry(transient);

  chrome::search::SearchTabHelper::FromWebContents(contents)->
      NavigationEntryUpdated();
}

bool GetURLForMostVisitedItemID(Profile* profile,
                                InstantRestrictedID most_visited_item_id,
                                GURL* url) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return false;

  InstantMostVisitedItem item;
  if (!instant_service->GetMostVisitedItemForID(most_visited_item_id, &item))
    return false;

  *url = item.url;
  return true;
}

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return my_url.host() == other_url.host() &&
         my_url.port() == other_url.port() &&
         my_url.path() == other_url.path() &&
         (my_url.scheme() == other_url.scheme() ||
          (my_url.SchemeIs(chrome::kHttpsScheme) &&
           other_url.SchemeIs(chrome::kHttpScheme)));
}

}  // namespace

InstantController::InstantController(chrome::BrowserInstantController* browser,
                                     bool extended_enabled)
    : browser_(browser),
      extended_enabled_(extended_enabled),
      instant_enabled_(false),
      use_local_overlay_only_(true),
      model_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      last_omnibox_text_has_inline_autocompletion_(false),
      last_verbatim_(false),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      last_match_was_search_(false),
      omnibox_focus_state_(OMNIBOX_FOCUS_NONE),
      omnibox_bounds_(-1, -1, 0, 0),
      allow_overlay_to_show_search_suggestions_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {

  // When the InstantController lives, the InstantService should live.
  // InstantService sets up profile-level facilities such as the ThemeSource for
  // the NTP.
  InstantServiceFactory::GetForProfile(browser_->profile());
}

InstantController::~InstantController() {
}

bool InstantController::Update(const AutocompleteMatch& match,
                               const string16& user_text,
                               const string16& full_text,
                               size_t selection_start,
                               size_t selection_end,
                               bool verbatim,
                               bool user_input_in_progress,
                               bool omnibox_popup_is_open,
                               bool escape_pressed,
                               bool is_keyword_search) {
  if (!extended_enabled_ && !instant_enabled_)
    return false;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "Update: %s user_text='%s' full_text='%s' selection_start=%d "
      "selection_end=%d verbatim=%d typing=%d popup=%d escape_pressed=%d "
      "is_keyword_search=%d",
      AutocompleteMatch::TypeToString(match.type).c_str(),
      UTF16ToUTF8(user_text).c_str(), UTF16ToUTF8(full_text).c_str(),
      static_cast<int>(selection_start), static_cast<int>(selection_end),
      verbatim, user_input_in_progress, omnibox_popup_is_open, escape_pressed,
      is_keyword_search));

  // TODO(dhollowa): Complete keyword match UI.  For now just hide suggestions.
  // http://crbug.com/153932.  Note, this early escape is happens prior to the
  // DCHECKs below because |user_text| and |full_text| have different semantics
  // when keyword search is in effect.
  if (is_keyword_search) {
    if (instant_tab_)
      instant_tab_->Update(string16(), 0, 0, true);
    else
      HideOverlay();
    last_match_was_search_ = false;
    return false;
  }

  // Ignore spurious updates when the omnibox is blurred; otherwise click
  // targets on the page may vanish before a click event arrives.
  if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    return false;

  // If the popup is open, the user has to be typing.
  DCHECK(!omnibox_popup_is_open || user_input_in_progress);

  // If the popup is closed, there should be no inline autocompletion.
  DCHECK(omnibox_popup_is_open || user_text.empty() || user_text == full_text)
      << user_text << "|" << full_text;

  // If there's no text in the omnibox, the user can't have typed any.
  DCHECK(!full_text.empty() || user_text.empty()) << user_text;

  // If the user isn't typing, and the popup is closed, there can't be any
  // user-typed text.
  DCHECK(user_input_in_progress || omnibox_popup_is_open || user_text.empty())
      << user_text;

  // The overlay is being clicked and will commit soon. Don't change anything.
  // TODO(sreeram): Add a browser test for this.
  if (overlay_ && overlay_->is_pointer_down_from_activate())
    return false;

  // In non-extended mode, SearchModeChanged() is never called, so fake it. The
  // mode is set to "disallow suggestions" here, so that if one of the early
  // "return false" conditions is hit, suggestions will be disallowed. If the
  // query is sent to the overlay, the mode is set to "allow" further below.
  if (!extended_enabled_)
    search_mode_.mode = chrome::search::Mode::MODE_DEFAULT;

  last_match_was_search_ = AutocompleteMatch::IsSearchType(match.type) &&
                           !user_text.empty();

  // In non extended mode, Instant is disabled for URLs and keyword mode.
  if (!extended_enabled_ &&
      (!last_match_was_search_ ||
       match.type == AutocompleteMatch::SEARCH_OTHER_ENGINE)) {
    HideOverlay();
    return false;
  }

  // If we have an |instant_tab_| use it, else ensure we have an overlay that is
  // current or is using the local overlay.
  if (!instant_tab_ && !(overlay_ && overlay_->IsUsingLocalOverlay()) &&
      !EnsureOverlayIsCurrent(false)) {
    HideOverlay();
    return false;
  }

  if (extended_enabled_) {
    if (!omnibox_popup_is_open) {
      if (!user_input_in_progress) {
        // If the user isn't typing and the omnibox popup is closed, it means a
        // regular navigation, tab-switch or the user hitting Escape.
        if (instant_tab_) {
          // The user is on a search results page. It may be showing results for
          // a partial query the user typed before they hit Escape. Send the
          // omnibox text to the page to restore the original results.
          //
          // In a tab switch, |instant_tab_| won't have updated yet, so it may
          // be pointing to the previous tab (which was a search results page).
          // Ensure we don't send the omnibox text to a random webpage (the new
          // tab), by comparing the old and new WebContents.
          if (escape_pressed &&
              instant_tab_->contents() == browser_->GetActiveWebContents()) {
            instant_tab_->Submit(full_text);
          }
        } else if (!full_text.empty()) {
          // If |full_text| is empty, the user is on the NTP. The overlay may
          // be showing custom NTP content; hide only if that's not the case.
          HideOverlay();
        }
      } else if (full_text.empty()) {
        // The user is typing, and backspaced away all omnibox text. Clear
        // |last_omnibox_text_| so that we don't attempt to set suggestions.
        last_omnibox_text_.clear();
        last_user_text_.clear();
        last_suggestion_ = InstantSuggestion();
        if (instant_tab_) {
          // On a search results page, tell it to clear old results.
          instant_tab_->Update(string16(), 0, 0, true);
        } else if (search_mode_.is_origin_ntp()) {
          // On the NTP, tell the overlay to clear old results. Don't hide the
          // overlay so it can show a blank page or logo if it wants.
          overlay_->Update(string16(), 0, 0, true);
        } else {
          HideOverlay();
        }
      } else {
        // The user switched to a tab with partial text already in the omnibox.
        HideOverlay();

        // The new tab may or may not be a search results page; we don't know
        // since SearchModeChanged() hasn't been called yet. If it later turns
        // out to be, we should store |full_text| now, so that if the user hits
        // Enter, we'll send the correct query to instant_tab_->Submit(). If the
        // partial text is not a query (|last_match_was_search_| is false), we
        // won't Submit(), so no need to worry about that.
        last_omnibox_text_ = full_text;
        last_user_text_ = user_text;
        last_suggestion_ = InstantSuggestion();
      }
      return false;
    } else if (full_text.empty()) {
      // The user typed a solitary "?". Same as the backspace case above.
      last_omnibox_text_.clear();
      last_user_text_.clear();
      last_suggestion_ = InstantSuggestion();
      if (instant_tab_)
        instant_tab_->Update(string16(), 0, 0, true);
      else if (search_mode_.is_origin_ntp())
        overlay_->Update(string16(), 0, 0, true);
      else
        HideOverlay();
      return false;
    }
  } else if (!omnibox_popup_is_open || full_text.empty()) {
    // In the non-extended case, hide the overlay as long as the user isn't
    // actively typing a non-empty query.
    HideOverlay();
    return false;
  }

  last_omnibox_text_has_inline_autocompletion_ = user_text != full_text;

  // If the user continues typing the same query as the suggested text is
  // showing, reuse the suggestion (but only for INSTANT_COMPLETE_NEVER).
  bool reused_suggestion = false;
  if (last_suggestion_.behavior == INSTANT_COMPLETE_NEVER &&
      !last_omnibox_text_has_inline_autocompletion_) {
    if (StartsWith(last_omnibox_text_, full_text, false)) {
      // The user is backspacing away characters.
      last_suggestion_.text.insert(0, last_omnibox_text_, full_text.size(),
          last_omnibox_text_.size() - full_text.size());
      reused_suggestion = true;
    } else if (StartsWith(full_text, last_omnibox_text_, false)) {
      // The user is typing forward. Normalize any added characters.
      reused_suggestion = NormalizeAndStripPrefix(&last_suggestion_.text,
          string16(full_text, last_omnibox_text_.size()));
    }
  }
  if (!reused_suggestion)
    last_suggestion_ = InstantSuggestion();

  last_omnibox_text_ = full_text;
  last_user_text_ = user_text;

  if (!extended_enabled_) {
    // In non-extended mode, the query is verbatim if there's any selection
    // (including inline autocompletion) or if the cursor is not at the end.
    verbatim = verbatim || selection_start != selection_end ||
                           selection_start != full_text.size();
  }
  last_verbatim_ = verbatim;

  last_transition_type_ = match.transition;
  url_for_history_ = match.destination_url;

  // Allow search suggestions. In extended mode, SearchModeChanged() will set
  // this, but it's not called in non-extended mode, so fake it.
  if (!extended_enabled_)
    search_mode_.mode = chrome::search::Mode::MODE_SEARCH_SUGGESTIONS;

  if (instant_tab_) {
    // If we have an |instant_tab_| but it doesn't support Instant yet, sever
    // the connection to it so we use the overlay instead. This ensures that the
    // user interaction will be responsive and handles cases where
    // |instant_tab_| never responds about whether it supports Instant.
    if (instant_tab_->supports_instant())
      instant_tab_->Update(user_text, selection_start, selection_end, verbatim);
    else
      instant_tab_.reset();
  }

  if (!instant_tab_) {
    if (first_interaction_time_.is_null())
      first_interaction_time_ = base::Time::Now();
    allow_overlay_to_show_search_suggestions_ = true;

    // For extended mode, if the loader is not ready at this point, switch over
    // to a backup loader.
    if (extended_enabled_ && !overlay_->supports_instant() &&
        !overlay_->IsUsingLocalOverlay() && browser_->GetActiveWebContents()) {
      CreateOverlay(chrome::kChromeSearchLocalOmniboxPopupURL,
                    browser_->GetActiveWebContents());
    }

    overlay_->Update(extended_enabled_ ? user_text : full_text,
                     selection_start, selection_end, verbatim);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());

  // We don't have new suggestions yet, but we can either reuse the existing
  // suggestion or reset the existing "gray text".
  browser_->SetInstantSuggestion(last_suggestion_);

  return true;
}

scoped_ptr<content::WebContents> InstantController::ReleaseNTPContents() {
  if (!extended_enabled_)
    return scoped_ptr<content::WebContents>(NULL);

  LOG_INSTANT_DEBUG_EVENT(this, "ReleaseNTPContents");

  // Don't use the Instant NTP if it doesn't match the current Instant URL.
  std::string instant_url;
  if (!GetInstantURL(browser_->profile(), false, &instant_url) ||
      (ntp_ &&
       !MatchesOriginAndPath(GURL(ntp_->instant_url()), GURL(instant_url)))) {
    ntp_.reset();
  }

  scoped_ptr<content::WebContents> ntp_contents;
  if (ntp_)
    ntp_contents = ntp_->ReleaseContents();

  // Override the blacklist on an explicit user action.
  ResetNTP(true);
  return ntp_contents.Pass();
}

// TODO(tonyg): This method only fires when the omnibox bounds change. It also
// needs to fire when the overlay bounds change (e.g.: open/close info bar).
void InstantController::SetPopupBounds(const gfx::Rect& bounds) {
  if (!extended_enabled_ && !instant_enabled_)
    return;

  if (popup_bounds_ == bounds)
    return;

  popup_bounds_ = bounds;
  if (popup_bounds_.height() > last_popup_bounds_.height()) {
    update_bounds_timer_.Stop();
    SendPopupBoundsToPage();
  } else if (!update_bounds_timer_.IsRunning()) {
    update_bounds_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kUpdateBoundsDelayMS), this,
        &InstantController::SendPopupBoundsToPage);
  }
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (!extended_enabled_ || omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (overlay_)
    overlay_->SetOmniboxBounds(omnibox_bounds_);
  if (ntp_)
    ntp_->SetOmniboxBounds(omnibox_bounds_);
  if (instant_tab_)
    instant_tab_->SetOmniboxBounds(omnibox_bounds_);
}

void InstantController::HandleAutocompleteResults(
    const std::vector<AutocompleteProvider*>& providers) {
  if (!extended_enabled_)
    return;

  if (!instant_tab_ && !overlay_)
    return;

  // The omnibox sends suggestions when its possibly imaginary popup closes
  // as it stops autocomplete. Ignore these.
  if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    return;

  DVLOG(1) << "AutocompleteResults:";
  std::vector<InstantAutocompleteResult> results;
  for (ACProviders::const_iterator provider = providers.begin();
       provider != providers.end(); ++provider) {
    const bool from_search_provider =
        (*provider)->type() == AutocompleteProvider::TYPE_SEARCH;
    // Unless we are talking to the local overlay, skip SearchProvider, since
    // it only echoes suggestions.
    if (from_search_provider &&
        (instant_tab_ || !overlay_->IsUsingLocalOverlay()))
      continue;
    // Only send autocomplete results when all the providers are done.
    if (!(*provider)->done()) {
      DVLOG(1) << "Waiting for " << (*provider)->GetName();
      return;
    }
    for (ACMatches::const_iterator match = (*provider)->matches().begin();
         match != (*provider)->matches().end(); ++match) {
      InstantAutocompleteResult result;
      result.provider = UTF8ToUTF16((*provider)->GetName());
      result.type = UTF8ToUTF16(AutocompleteMatch::TypeToString(match->type));
      result.description = match->description;
      result.destination_url = UTF8ToUTF16(match->destination_url.spec());
      if (from_search_provider)
        result.search_query = match->contents;
      result.transition = match->transition;
      result.relevance = match->relevance;
      DVLOG(1) << "    " << result.relevance << " " << result.type << " "
               << result.provider << " " << result.destination_url << " '"
               << result.description << "' '" << result.search_query << "' "
               << result.transition;
      results.push_back(result);
    }
  }
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "HandleAutocompleteResults: total_results=%d",
      static_cast<int>(results.size())));

  if (instant_tab_)
    instant_tab_->SendAutocompleteResults(results);
  else
    overlay_->SendAutocompleteResults(results);
}

bool InstantController::OnUpOrDownKeyPressed(int count) {
  if (!extended_enabled_)
    return false;

  if (!instant_tab_ && !overlay_)
    return false;

  if (instant_tab_)
    instant_tab_->UpOrDownKeyPressed(count);
  else
    overlay_->UpOrDownKeyPressed(count);

  return true;
}

void InstantController::OnCancel(const AutocompleteMatch& match,
                                 const string16& user_text,
                                 const string16& full_text) {
  if (!extended_enabled_)
    return;

  if (!instant_tab_ && !overlay_)
    return;

  // We manually reset the state here since the JS is not expected to do it.
  // TODO(sreeram): Handle the case where user_text is now a URL
  last_match_was_search_ = AutocompleteMatch::IsSearchType(match.type) &&
                           !full_text.empty();
  last_omnibox_text_ = full_text;
  last_user_text_ = user_text;
  last_suggestion_ = InstantSuggestion();

  if (instant_tab_)
    instant_tab_->CancelSelection(full_text);
  else
    overlay_->CancelSelection(full_text);
}

content::WebContents* InstantController::GetOverlayContents() const {
  return overlay_ ? overlay_->contents() : NULL;
}

bool InstantController::IsOverlayingSearchResults() const {
  return model_.mode().is_search_suggestions() && IsFullHeight(model_) &&
         (last_match_was_search_ ||
          last_suggestion_.behavior == INSTANT_COMPLETE_NEVER);
}

bool InstantController::CommitIfPossible(InstantCommitType type) {
  if (!extended_enabled_ && !instant_enabled_)
    return false;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "CommitIfPossible: type=%d last_omnibox_text_='%s' "
      "last_match_was_search_=%d instant_tab_=%d", type,
      UTF16ToUTF8(last_omnibox_text_).c_str(), last_match_was_search_,
      instant_tab_ != NULL));

  // If we are on an already committed search results page, send a submit event
  // to the page, but otherwise, nothing else to do.
  if (instant_tab_) {
    if (type == INSTANT_COMMIT_PRESSED_ENTER &&
        (last_match_was_search_ ||
         last_suggestion_.behavior == INSTANT_COMPLETE_NEVER)) {
      last_suggestion_.text.clear();
      instant_tab_->Submit(last_omnibox_text_);
      instant_tab_->contents()->GetView()->Focus();
      EnsureSearchTermsAreSet(instant_tab_->contents(), last_omnibox_text_);
      return true;
    }
    return false;
  }

  // If the overlay is not showing at all, don't commit it.
  if (!model_.mode().is_search_suggestions())
    return false;

  // If the overlay is showing at full height (with results), commit it.
  // If it's showing at parial height, commit if it's navigating.
  if (!IsOverlayingSearchResults() && type != INSTANT_COMMIT_NAVIGATED)
    return false;

  // There may re-entrance here, from the call to browser_->CommitInstant below,
  // which can cause a TabDeactivated notification which gets back here.
  // In this case, overlay_->ReleaseContents() was called already.
  if (!GetOverlayContents())
    return false;

  // Never commit the local overlay.
  if (overlay_->IsUsingLocalOverlay())
    return false;

  if (type == INSTANT_COMMIT_FOCUS_LOST) {
    // Extended mode doesn't need or use the Cancel message.
    if (!extended_enabled_)
      overlay_->Cancel(last_omnibox_text_);
  } else if (type != INSTANT_COMMIT_NAVIGATED) {
    overlay_->Submit(last_omnibox_text_);
  }

  scoped_ptr<content::WebContents> overlay = overlay_->ReleaseContents();

  // If the overlay page has navigated since the last Update(), we need to add
  // the navigation to history ourselves. Else, the page will navigate after
  // commit, and it will be added to history in the usual manner.
  const history::HistoryAddPageArgs& last_navigation =
      overlay_->last_navigation();
  if (!last_navigation.url.is_empty()) {
    content::NavigationEntry* entry = overlay->GetController().GetActiveEntry();

    // The last navigation should be the same as the active entry if the overlay
    // is in search mode. During navigation, the active entry could have
    // changed since DidCommitProvisionalLoadForFrame is called after the entry
    // is changed.
    // TODO(shishir): Should we commit the last navigation for
    // INSTANT_COMMIT_NAVIGATED.
    DCHECK(type == INSTANT_COMMIT_NAVIGATED ||
           last_navigation.url == entry->GetURL());

    // Add the page to history.
    HistoryTabHelper* history_tab_helper =
        HistoryTabHelper::FromWebContents(overlay.get());
    history_tab_helper->UpdateHistoryForNavigation(last_navigation);

    // Update the page title.
    history_tab_helper->UpdateHistoryPageTitle(*entry);
  }

  // Add a fake history entry with a non-Instant search URL, so that search
  // terms extraction (for autocomplete history matches) works.
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(overlay->GetBrowserContext()),
      Profile::EXPLICIT_ACCESS);
  if (history) {
    history->AddPage(url_for_history_, base::Time::Now(), NULL, 0, GURL(),
                     history::RedirectList(), last_transition_type_,
                     history::SOURCE_BROWSED, false);
  }

  if (type == INSTANT_COMMIT_PRESSED_ALT_ENTER) {
    overlay->GetController().PruneAllButActive();
  } else {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    AddSessionStorageHistogram(extended_enabled_, active_tab, overlay.get());
    overlay->GetController().CopyStateFromAndPrune(
        &active_tab->GetController());
  }

  if (extended_enabled_) {
    // Adjust the search terms shown in the omnibox for this query. Hitting
    // ENTER searches for what the user typed, so use last_omnibox_text_.
    // Clicking on the overlay commits what is currently showing, so add in the
    // gray text in that case.
    if (type == INSTANT_COMMIT_FOCUS_LOST &&
        last_suggestion_.behavior == INSTANT_COMPLETE_NEVER) {
      // Update |last_omnibox_text_| so that the controller commits the proper
      // query if the user focuses the omnibox and presses Enter.
      last_omnibox_text_ += last_suggestion_.text;
    }

    EnsureSearchTermsAreSet(overlay.get(), last_omnibox_text_);
  }

  // Save notification source before we release the overlay.
  content::Source<content::WebContents> notification_source(overlay.get());

  browser_->CommitInstant(overlay.Pass(),
                          type == INSTANT_COMMIT_PRESSED_ALT_ENTER);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      notification_source,
      content::NotificationService::NoDetails());

  // Hide explicitly. See comments in HideOverlay() for why.
  model_.SetOverlayState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);

  // Delay deletion as we could've gotten here from an InstantOverlay method.
  MessageLoop::current()->DeleteSoon(FROM_HERE, overlay_.release());

  // Try to create another overlay immediately so that it is ready for the next
  // user interaction.
  EnsureOverlayIsCurrent(false);

  LOG_INSTANT_DEBUG_EVENT(this, "Committed");
  return true;
}

void InstantController::OmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason,
    gfx::NativeView view_gaining_focus) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "OmniboxFocusChanged: %d to %d for reason %d", omnibox_focus_state_,
      state, reason));

  OmniboxFocusState old_focus_state = omnibox_focus_state_;
  omnibox_focus_state_ = state;
  if (!extended_enabled_ && !instant_enabled_)
    return;

  // Tell the page if the key capture mode changed unless the focus state
  // changed because of TYPING. This is because in that case, the browser hasn't
  // really stopped capturing key strokes.
  //
  // (More practically, if we don't do this check, the page would receive
  // onkeycapturechange before the corresponding onchange, and the page would
  // have no way of telling whether the keycapturechange happened because of
  // some actual user action or just because they started typing.)
  if (extended_enabled_ && GetOverlayContents() &&
      reason != OMNIBOX_FOCUS_CHANGE_TYPING) {
    const bool is_key_capture_enabled =
        omnibox_focus_state_ == OMNIBOX_FOCUS_INVISIBLE;
    if (overlay_)
      overlay_->KeyCaptureChanged(is_key_capture_enabled);
    if (instant_tab_)
      instant_tab_->KeyCaptureChanged(is_key_capture_enabled);
  }

  // If focus went from outside the omnibox to the omnibox, preload the default
  // search engine, in anticipation of the user typing a query. If the reverse
  // happened, commit or discard the overlay.
  if (state != OMNIBOX_FOCUS_NONE && old_focus_state == OMNIBOX_FOCUS_NONE) {
    // On explicit user actions, ignore the Instant blacklist.
    EnsureOverlayIsCurrent(reason == OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  } else if (state == OMNIBOX_FOCUS_NONE &&
             old_focus_state != OMNIBOX_FOCUS_NONE) {
    OmniboxLostFocus(view_gaining_focus);
  }
}

void InstantController::SearchModeChanged(
    const chrome::search::Mode& old_mode,
    const chrome::search::Mode& new_mode) {
  if (!extended_enabled_)
    return;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SearchModeChanged: [origin:mode] %d:%d to %d:%d", old_mode.origin,
      old_mode.mode, new_mode.origin, new_mode.mode));

  search_mode_ = new_mode;
  if (!new_mode.is_search_suggestions())
    HideOverlay();

  ResetInstantTab();
}

void InstantController::ActiveTabChanged() {
  if (!extended_enabled_ && !instant_enabled_)
    return;

  LOG_INSTANT_DEBUG_EVENT(this, "ActiveTabChanged");

  // When switching tabs, always hide the overlay.
  HideOverlay();

  if (extended_enabled_)
    ResetInstantTab();
}

void InstantController::TabDeactivated(content::WebContents* contents) {
  LOG_INSTANT_DEBUG_EVENT(this, "TabDeactivated");
  if (extended_enabled_ && !contents->IsBeingDestroyed())
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
}

void InstantController::SetInstantEnabled(bool instant_enabled,
                                          bool use_local_overlay_only) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SetInstantEnabled: instant_enabled=%d, use_local_overlay_only=%d",
      instant_enabled, use_local_overlay_only));

  // Non extended mode does not care about |use_local_overlay_only|.
  if (instant_enabled == instant_enabled_ &&
      (!extended_enabled_ ||
       use_local_overlay_only == use_local_overlay_only_)) {
    return;
  }

  instant_enabled_ = instant_enabled;
  use_local_overlay_only_ = use_local_overlay_only;
  HideInternal();
  overlay_.reset();
  if (extended_enabled_ || instant_enabled_)
    EnsureOverlayIsCurrent(false);
  if (extended_enabled_)
    ResetNTP(false);
  if (instant_tab_)
    instant_tab_->SetDisplayInstantResults(instant_enabled_);
}

void InstantController::ThemeChanged(const ThemeBackgroundInfo& theme_info) {
  if (!extended_enabled_)
    return;

  if (overlay_)
    overlay_->SendThemeBackgroundInfo(theme_info);
  if (ntp_)
    ntp_->SendThemeBackgroundInfo(theme_info);
  if (instant_tab_)
    instant_tab_->SendThemeBackgroundInfo(theme_info);
}

void InstantController::SwappedOverlayContents() {
  model_.SetOverlayContents(GetOverlayContents());
}

void InstantController::FocusedOverlayContents() {
#if defined(USE_AURA)
  // On aura the omnibox only receives a focus lost if we initiate the focus
  // change. This does that.
  if (!model_.mode().is_default())
    browser_->InstantOverlayFocused();
#endif
}

void InstantController::ReloadOverlayIfStale() {
  // The local overlay is never stale.
  if (overlay_ && overlay_->IsUsingLocalOverlay())
    return;

  // If the overlay is showing or the omnibox has focus, don't delete the
  // overlay. It will get refreshed the next time the overlay is hidden or the
  // omnibox loses focus.
  if ((!overlay_ || overlay_->is_stale()) &&
      omnibox_focus_state_ == OMNIBOX_FOCUS_NONE &&
      model_.mode().is_default()) {
    overlay_.reset();
    EnsureOverlayIsCurrent(false);
  }
}

void InstantController::OverlayLoadCompletedMainFrame() {
  if (overlay_->supports_instant())
    return;
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_->profile());
  content::WebContents* contents = overlay_->contents();
  DCHECK(contents);
  if (instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID())) {
    return;
  }
  InstantSupportDetermined(contents, false);
}

void InstantController::LogDebugEvent(const std::string& info) const {
  DVLOG(1) << info;

  debug_events_.push_front(std::make_pair(
      base::Time::Now().ToInternalValue(), info));
  static const size_t kMaxDebugEventSize = 2000;
  if (debug_events_.size() > kMaxDebugEventSize)
    debug_events_.pop_back();
}

void InstantController::ClearDebugEvents() {
  debug_events_.clear();
}

void InstantController::DeleteMostVisitedItem(
    InstantRestrictedID most_visited_item_id) {
  history::TopSites* top_sites = browser_->profile()->GetTopSites();
  if (!top_sites)
    return;

  GURL url;
  if (GetURLForMostVisitedItemID(browser_->profile(),
                                 most_visited_item_id, &url))
    top_sites->AddBlacklistedURL(url);
}

void InstantController::UndoMostVisitedDeletion(
    InstantRestrictedID most_visited_item_id) {
  history::TopSites* top_sites = browser_->profile()->GetTopSites();
  if (!top_sites)
    return;

  GURL url;
  if (GetURLForMostVisitedItemID(browser_->profile(),
                                 most_visited_item_id, &url))
    top_sites->RemoveBlacklistedURL(url);
}

void InstantController::UndoAllMostVisitedDeletions() {
  history::TopSites* top_sites = browser_->profile()->GetTopSites();
  if (!top_sites)
    return;

  top_sites->ClearBlacklistedURLs();
}

void InstantController::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);
  RequestMostVisitedItems();
}

// TODO(shishir): We assume that the WebContent's current RenderViewHost is the
// RenderViewHost being created which is not always true. Fix this.
void InstantController::InstantPageRenderViewCreated(
    const content::WebContents* contents) {
  if (!extended_enabled_)
    return;

  // Update theme info so that the page picks it up.
  browser_->UpdateThemeInfo();

  // Ensure the searchbox API has the correct initial state.
  if (IsContentsFrom(overlay(), contents)) {
    overlay_->SetDisplayInstantResults(instant_enabled_);
    overlay_->KeyCaptureChanged(
        omnibox_focus_state_ == OMNIBOX_FOCUS_INVISIBLE);
    overlay_->SetOmniboxBounds(omnibox_bounds_);
    overlay_->InitializeFonts();
  } else if (IsContentsFrom(ntp(), contents)) {
    ntp_->SetDisplayInstantResults(instant_enabled_);
    ntp_->SetOmniboxBounds(omnibox_bounds_);
    ntp_->InitializeFonts();
  } else {
    NOTREACHED();
  }
  StartListeningToMostVisitedChanges();
}

void InstantController::InstantSupportDetermined(
    const content::WebContents* contents,
    bool supports_instant) {
  if (IsContentsFrom(instant_tab(), contents)) {
    if (!supports_instant)
      MessageLoop::current()->DeleteSoon(FROM_HERE, instant_tab_.release());

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  } else if (IsContentsFrom(ntp(), contents)) {
    if (supports_instant)
      RemoveFromBlacklist(ntp_->instant_url());
    else
      BlacklistAndResetNTP();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());

  } else if (IsContentsFrom(overlay(), contents)) {
    if (supports_instant)
      RemoveFromBlacklist(overlay_->instant_url());
    else
      BlacklistAndResetOverlay();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_OVERLAY_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  }
}

void InstantController::InstantPageRenderViewGone(
    const content::WebContents* contents) {
  if (IsContentsFrom(overlay(), contents))
    BlacklistAndResetOverlay();
  else if (IsContentsFrom(ntp(), contents))
    BlacklistAndResetNTP();
  else
    NOTREACHED();
}

void InstantController::InstantPageAboutToNavigateMainFrame(
    const content::WebContents* contents,
    const GURL& url) {
  DCHECK(IsContentsFrom(overlay(), contents));

  // If the page does not yet support Instant, we allow redirects and other
  // navigations to go through since the Instant URL can redirect - e.g. to
  // country specific pages.
  if (!overlay_->supports_instant())
    return;

  GURL instant_url(overlay_->instant_url());

  // If we are navigating to the Instant URL, do nothing.
  if (url == instant_url)
    return;

  // Commit the navigation if either:
  //  - The page is in NTP mode (so it could only navigate on a user click) or
  //  - The page is not in NTP mode and we are navigating to a URL with a
  //    different host or path than the Instant URL. This enables the instant
  //    page when it is showing search results to change the query parameters
  //    and fragments of the URL without it navigating.
  if (model_.mode().is_ntp() ||
      (url.host() != instant_url.host() || url.path() != instant_url.path())) {
    CommitIfPossible(INSTANT_COMMIT_NAVIGATED);
  }
}

void InstantController::SetSuggestions(
    const content::WebContents* contents,
    const std::vector<InstantSuggestion>& suggestions) {
  LOG_INSTANT_DEBUG_EVENT(this, "SetSuggestions");

  // Ignore if the message is from an unexpected source.
  if (IsContentsFrom(ntp(), contents))
    return;
  if (instant_tab_ && !IsContentsFrom(instant_tab(), contents))
    return;
  if (IsContentsFrom(overlay(), contents) &&
      !allow_overlay_to_show_search_suggestions_)
    return;

  InstantSuggestion suggestion;
  if (!suggestions.empty())
    suggestion = suggestions[0];

  if (instant_tab_ &&
      (search_mode_.is_search_results() || search_mode_.is_ntp()) &&
      suggestion.behavior == INSTANT_COMPLETE_REPLACE) {
    // Update |last_omnibox_text_| so that the controller commits the proper
    // query if the user focuses the omnibox and presses Enter.
    last_omnibox_text_ = suggestion.text;
    last_suggestion_ = InstantSuggestion();
    last_match_was_search_ = suggestion.type == INSTANT_SUGGESTION_SEARCH;
    // This means a committed page in state search called setValue(). We should
    // update the omnibox to reflect what the search page says.
    browser_->SetInstantSuggestion(suggestion);
    return;
  }

  // Ignore if we are not currently accepting search suggestions.
  if (!search_mode_.is_search_suggestions() || last_omnibox_text_.empty())
    return;

  if (suggestion.behavior == INSTANT_COMPLETE_REPLACE) {
    // We don't get an Update() when changing the omnibox due to a REPLACE
    // suggestion (so that we don't inadvertently cause the overlay to change
    // what it's showing, as the user arrows up/down through the page-provided
    // suggestions). So, update these state variables here.
    last_omnibox_text_ = suggestion.text;
    last_suggestion_ = InstantSuggestion();
    last_match_was_search_ = suggestion.type == INSTANT_SUGGESTION_SEARCH;
    LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
        "ReplaceSuggestion text='%s' type=%d",
        UTF16ToUTF8(suggestion.text).c_str(), suggestion.type));
    browser_->SetInstantSuggestion(suggestion);
  } else {
    if (FixSuggestion(&suggestion)) {
      last_suggestion_ = suggestion;
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "SetInstantSuggestion: text='%s' behavior=%d",
          UTF16ToUTF8(suggestion.text).c_str(),
          suggestion.behavior));
      browser_->SetInstantSuggestion(suggestion);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_INSTANT_SET_SUGGESTION,
          content::Source<InstantController>(this),
          content::NotificationService::NoDetails());
    } else {
      last_suggestion_ = InstantSuggestion();
    }
  }

  // Extended mode pages will call ShowOverlay() when they are ready.
  if (!extended_enabled_)
    ShowOverlay(100, INSTANT_SIZE_PERCENT);
}

void InstantController::ShowInstantOverlay(const content::WebContents* contents,
                                           int height,
                                           InstantSizeUnits units) {
  if (extended_enabled_ && IsContentsFrom(overlay(), contents))
    ShowOverlay(height, units);
}

void InstantController::FocusOmnibox(const content::WebContents* contents) {
  if (!extended_enabled_)
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));
  browser_->FocusOmnibox(true);
}

void InstantController::StartCapturingKeyStrokes(
    const content::WebContents* contents) {
  if (!extended_enabled_)
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));
  browser_->FocusOmnibox(false);
}

void InstantController::StopCapturingKeyStrokes(
    content::WebContents* contents) {
  // Nothing to do if omnibox doesn't have invisible focus.
  if (!extended_enabled_ || omnibox_focus_state_ != OMNIBOX_FOCUS_INVISIBLE)
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));
  contents->GetView()->Focus();
}

void InstantController::NavigateToURL(const content::WebContents* contents,
                                      const GURL& url,
                                      content::PageTransition transition,
                                      WindowOpenDisposition disposition) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "NavigateToURL: url='%s'", url.spec().c_str()));

  // TODO(samarth): handle case where contents are no longer "active" (e.g. user
  // has switched tabs).
  if (!extended_enabled_)
    return;
  if (overlay_)
    HideOverlay();

  if (transition == content::PAGE_TRANSITION_AUTO_BOOKMARK) {
    content::RecordAction(
        content::UserMetricsAction("InstantExtended.MostVisitedClicked"));
  }
  browser_->OpenURL(url, transition, disposition);
}

void InstantController::OmniboxLostFocus(gfx::NativeView view_gaining_focus) {
  // If the overlay is showing custom NTP content, don't hide it, commit it
  // (no matter where the user clicked) or try to recreate it.
  if (model_.mode().is_ntp())
    return;

  if (model_.mode().is_default()) {
    // Correct search terms if the user clicked on the committed results page
    // while showing an autocomplete suggestion
    if (instant_tab_ && !last_suggestion_.text.empty() &&
        last_suggestion_.behavior == INSTANT_COMPLETE_NEVER &&
        IsViewInContents(GetViewGainingFocus(view_gaining_focus),
                         instant_tab_->contents())) {
      // Commit the omnibox's suggested grey text as if the user had typed it.
      browser_->CommitSuggestedText(true);

      // Update the state so that next query from hitting Enter from the
      // omnibox is correct.
      last_omnibox_text_ += last_suggestion_.text;
      last_suggestion_ = InstantSuggestion();
    }
    // If the overlay is not showing at all, recreate it if it's stale.
    ReloadOverlayIfStale();
    MaybeSwitchToRemoteOverlay();
    return;
  }

  // The overlay is showing search suggestions. If GetOverlayContents() is NULL,
  // we are in the commit path. Don't do anything.
  if (!GetOverlayContents())
    return;

#if defined(OS_MACOSX)
  // TODO(sreeram): See if Mac really needs this special treatment.
  if (!overlay_->is_pointer_down_from_activate())
    HideOverlay();
#else
  if (IsFullHeight(model_))
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
  else if (!IsViewInContents(GetViewGainingFocus(view_gaining_focus),
                             overlay_->contents()))
    HideOverlay();
#endif
}

void InstantController::ResetNTP(bool ignore_blacklist) {
  ntp_.reset();
  std::string instant_url;
  if (!GetInstantURL(browser_->profile(), ignore_blacklist, &instant_url)) {
    // TODO(sreeram|samarth): use local ntp here once available.
    return;
  }
  ntp_.reset(new InstantNTP(this, instant_url));
  ntp_->InitContents(browser_->profile(), browser_->GetActiveWebContents(),
                     base::Bind(&InstantController::ResetNTP,
                                base::Unretained(this), false));
}

bool InstantController::EnsureOverlayIsCurrent(bool ignore_blacklist) {
  // If there's no active tab, the browser is closing.
  const content::WebContents* active_tab = browser_->GetActiveWebContents();
  if (!active_tab)
    return false;

  Profile* profile = Profile::FromBrowserContext(
      active_tab->GetBrowserContext());
  std::string instant_url;
  if (!GetInstantURL(profile, ignore_blacklist, &instant_url)) {
    // If we are in extended mode, fallback to the local overlay.
    if (extended_enabled_)
      instant_url = chrome::kChromeSearchLocalOmniboxPopupURL;
    else
      return false;
  }

  if (!overlay_ || overlay_->instant_url() != instant_url)
    CreateOverlay(instant_url, active_tab);

  return true;
}

void InstantController::CreateOverlay(const std::string& instant_url,
                                      const content::WebContents* active_tab) {
  HideInternal();
  overlay_.reset(new InstantOverlay(this, instant_url));
  overlay_->InitContents(browser_->profile(), active_tab);
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "CreateOverlay: instant_url='%s'", instant_url.c_str()));
}

void InstantController::MaybeSwitchToRemoteOverlay() {
  if (!overlay_ || omnibox_focus_state_ != OMNIBOX_FOCUS_NONE ||
      !model_.mode().is_default()) {
    return;
  }

  EnsureOverlayIsCurrent(false);
}

void InstantController::ResetInstantTab() {
  // Do not wire up the InstantTab if Instant should only use local overlays, to
  // prevent it from sending data to the page.
  if (!search_mode_.is_origin_default() && !use_local_overlay_only_) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_ || active_tab != instant_tab_->contents()) {
      instant_tab_.reset(new InstantTab(this));
      instant_tab_->Init(active_tab);
      // Update theme info for this tab.
      browser_->UpdateThemeInfo();
      instant_tab_->SetDisplayInstantResults(instant_enabled_);
      instant_tab_->SetOmniboxBounds(omnibox_bounds_);
      instant_tab_->InitializeFonts();
      StartListeningToMostVisitedChanges();
      instant_tab_->KeyCaptureChanged(
          omnibox_focus_state_ == OMNIBOX_FOCUS_INVISIBLE);
    }

    // Hide the |overlay_| since we are now using |instant_tab_| instead.
    HideOverlay();
  } else {
    instant_tab_.reset();
  }
}

void InstantController::HideOverlay() {
  HideInternal();
  ReloadOverlayIfStale();
  MaybeSwitchToRemoteOverlay();
}

void InstantController::HideInternal() {
  LOG_INSTANT_DEBUG_EVENT(this, "Hide");

  // If GetOverlayContents() returns NULL, either we're already in the desired
  // MODE_DEFAULT state, or we're in the commit path. For the latter, don't
  // change the state just yet; else we may hide the overlay unnecessarily.
  // Instead, the state will be set correctly after the commit is done.
  if (GetOverlayContents()) {
    model_.SetOverlayState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);
    allow_overlay_to_show_search_suggestions_ = false;

    // Send a message asking the overlay to clear out old results.
    overlay_->Update(string16(), 0, 0, true);
  }

  // Clear the first interaction timestamp for later use.
  first_interaction_time_ = base::Time();
}

void InstantController::ShowOverlay(int height, InstantSizeUnits units) {
  // If we are on a committed search results page, the |overlay_| is not in use.
  if (instant_tab_)
    return;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "Show: height=%d units=%d", height, units));

  // Must have updated omnibox after the last HideOverlay() to show suggestions.
  if (!allow_overlay_to_show_search_suggestions_)
    return;

  // The page is trying to hide itself. Hide explicitly (i.e., don't use
  // HideOverlay()) so that it can change its mind.
  if (height == 0) {
    model_.SetOverlayState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);
    return;
  }

  // If the overlay is being shown for the first time since the user started
  // typing, record a histogram value.
  if (!first_interaction_time_.is_null() && model_.mode().is_default()) {
    base::TimeDelta delta = base::Time::Now() - first_interaction_time_;
    UMA_HISTOGRAM_TIMES("Instant.TimeToFirstShow", delta);
  }

  // Show at 100% height except in the following cases:
  // - The local overlay (omnibox popup) is being loaded.
  // - Instant is disabled. The page needs to be able to show only a dropdown.
  // - The page is over a website other than search or an NTP, and is not
  //   already showing at 100% height.
  if (overlay_->IsUsingLocalOverlay() || !instant_enabled_ ||
      (search_mode_.is_origin_default() && !IsFullHeight(model_)))
    model_.SetOverlayState(search_mode_, height, units);
  else
    model_.SetOverlayState(search_mode_, 100, INSTANT_SIZE_PERCENT);

  // If the overlay is being shown at full height and the omnibox is not
  // focused, commit right away.
  if (IsFullHeight(model_) && omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
}

void InstantController::SendPopupBoundsToPage() {
  if (last_popup_bounds_ == popup_bounds_ || !overlay_ ||
      overlay_->is_pointer_down_from_activate())
    return;

  last_popup_bounds_ = popup_bounds_;
  gfx::Rect overlay_bounds = browser_->GetInstantBounds();
  gfx::Rect intersection = gfx::IntersectRects(popup_bounds_, overlay_bounds);

  // Translate into window coordinates.
  if (!intersection.IsEmpty()) {
    intersection.Offset(-overlay_bounds.origin().x(),
                        -overlay_bounds.origin().y());
  }

  // In the current Chrome UI, these must always be true so they sanity check
  // the above operations. In a future UI, these may be removed or adjusted.
  // There is no point in sanity-checking |intersection.y()| because the omnibox
  // can be placed anywhere vertically relative to the overlay (for example, in
  // Mac fullscreen mode, the omnibox is fully enclosed by the overlay bounds).
  DCHECK_LE(0, intersection.x());
  DCHECK_LE(0, intersection.width());
  DCHECK_LE(0, intersection.height());

  overlay_->SetPopupBounds(intersection);
}

bool InstantController::GetInstantURL(Profile* profile,
                                      bool ignore_blacklist,
                                      std::string* instant_url) const {
  DCHECK(profile);
  instant_url->clear();

  if (extended_enabled_ && use_local_overlay_only_) {
    *instant_url = chrome::kChromeSearchLocalOmniboxPopupURL;
    return true;
  }

  const GURL instant_url_obj =
      chrome::search::GetInstantURL(profile, omnibox_bounds_.x());
  if (!instant_url_obj.is_valid())
    return false;

  *instant_url = instant_url_obj.spec();

  if (!ignore_blacklist) {
    std::map<std::string, int>::const_iterator iter =
        blacklisted_urls_.find(*instant_url);
    if (iter != blacklisted_urls_.end() &&
        iter->second > kMaxInstantSupportFailures) {
      RecordEventHistogram(INSTANT_CONTROLLER_EVENT_URL_BLOCKED_BY_BLACKLIST);
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "GetInstantURL: Instant URL blacklisted: url=%s",
          instant_url->c_str()));
      return false;
    }
  }

  return true;
}

void InstantController::BlacklistAndResetNTP() {
  ++blacklisted_urls_[ntp_->instant_url()];
  RecordEventHistogram(INSTANT_CONTROLLER_EVENT_URL_ADDED_TO_BLACKLIST);
  delete ntp_->ReleaseContents().release();
  MessageLoop::current()->DeleteSoon(FROM_HERE, ntp_.release());
  ResetNTP(false);
}

void InstantController::BlacklistAndResetOverlay() {
  ++blacklisted_urls_[overlay_->instant_url()];
  RecordEventHistogram(INSTANT_CONTROLLER_EVENT_URL_ADDED_TO_BLACKLIST);
  HideInternal();
  delete overlay_->ReleaseContents().release();
  MessageLoop::current()->DeleteSoon(FROM_HERE, overlay_.release());
  EnsureOverlayIsCurrent(false);
}

void InstantController::RemoveFromBlacklist(const std::string& url) {
  if (blacklisted_urls_.erase(url)) {
    RecordEventHistogram(INSTANT_CONTROLLER_EVENT_URL_REMOVED_FROM_BLACKLIST);
  }
}

void InstantController::StartListeningToMostVisitedChanges() {
  history::TopSites* top_sites = browser_->profile()->GetTopSites();
  if (top_sites) {
    if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
      content::Source<history::TopSites>(top_sites))) {
      // TopSites updates itself after a delay. This is especially noticable
      // when your profile is empty. Ask TopSites to update itself when we're
      // about to show the new tab page.
      top_sites->SyncWithHistory();

      RequestMostVisitedItems();

      // Register for notification when TopSites changes.
      registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                     content::Source<history::TopSites>(top_sites));
    } else {
      // We are already registered, so just get and send the most visited data.
      RequestMostVisitedItems();
    }
  }
}

void InstantController::RequestMostVisitedItems() {
  history::TopSites* top_sites = browser_->profile()->GetTopSites();
  if (top_sites) {
    top_sites->GetMostVisitedURLs(
        base::Bind(&InstantController::OnMostVisitedItemsReceived,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void InstantController::OnMostVisitedItemsReceived(
    const history::MostVisitedURLList& data) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_->profile());
  if (!instant_service)
    return;

  std::vector<InstantMostVisitedItem> most_visited_items;
  for (size_t i = 0; i < data.size(); i++) {
    const history::MostVisitedURL& url = data[i];
    InstantMostVisitedItem item;
    item.url = url.url;
    item.title = url.title;
    most_visited_items.push_back(item);
  }

  instant_service->AddMostVisitedItems(most_visited_items);

  std::vector<InstantMostVisitedItemIDPair> items_with_ids;
  instant_service->GetCurrentMostVisitedItems(&items_with_ids);
  SendMostVisitedItems(items_with_ids);
}

void InstantController::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItemIDPair>& items) {
  if (overlay_)
    overlay_->SendMostVisitedItems(items);
  if (ntp_)
    ntp_->SendMostVisitedItems(items);
  if (instant_tab_)
    instant_tab_->SendMostVisitedItems(items);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

bool InstantController::FixSuggestion(InstantSuggestion* suggestion) const {
  // Don't suggest gray text if there already was inline autocompletion.
  // http://crbug.com/162303
  if (suggestion->behavior == INSTANT_COMPLETE_NEVER &&
      last_omnibox_text_has_inline_autocompletion_)
    return false;

  // If the page is trying to set inline autocompletion in verbatim mode,
  // instead try suggesting the exact omnibox text. This makes the omnibox
  // interpret user text as an URL if possible while preventing unwanted
  // autocompletion during backspacing.
  if (suggestion->behavior == INSTANT_COMPLETE_NOW && last_verbatim_)
    suggestion->text = last_omnibox_text_;

  // Suggestion text should be a full URL for URL suggestions, or the
  // completion of a query for query suggestions.
  if (suggestion->type == INSTANT_SUGGESTION_URL) {
    // If the suggestion is not a valid URL, perhaps it's something like
    // "foo.com". Try prefixing "http://". If it still isn't valid, drop it.
    if (!GURL(suggestion->text).is_valid()) {
      suggestion->text.insert(0, ASCIIToUTF16("http://"));
      if (!GURL(suggestion->text).is_valid())
        return false;
    }

    // URL suggestions are only accepted if the query for which the suggestion
    // was generated is the same as |last_user_text_|.
    //
    // Any other URL suggestions--in particular suggestions for old user_text
    // lagging behind a slow IPC--are ignored. See crbug.com/181589.
    //
    // TODO(samarth): Accept stale suggestions if they would be accepted by
    // SearchProvider as an inlinable suggestion. http://crbug.com/191656.
    return suggestion->query == last_user_text_;
  }

  if (suggestion->type == INSTANT_SUGGESTION_SEARCH) {
    if (StartsWith(suggestion->text, last_omnibox_text_, true)) {
      // The user typed an exact prefix of the suggestion.
      suggestion->text.erase(0, last_omnibox_text_.size());
      return true;
    } else if (NormalizeAndStripPrefix(&suggestion->text, last_omnibox_text_)) {
      // Unicode normalize and case-fold the user text and suggestion. If the
      // user text is a prefix, suggest the normalized, case-folded completion
      // for instance, if the user types 'i' and the suggestion is 'INSTANT',
      // suggest 'nstant'. Otherwise, the user text really isn't a prefix, so
      // suggest nothing.
      // TODO(samarth|jered): revisit this logic. http://crbug.com/196572.
      return true;
    }
  }

  return false;
}
