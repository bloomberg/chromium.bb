// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/navigation_controller.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "grit/app_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const int kInvalidateAllButShelves =
    0xFFFFFFFF & ~TabContents::INVALIDATE_BOOKMARK_BAR;

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationController* nav_controller,
                         bool from_front,
                         int count) {
  NavigationController::PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  NotificationService::current()->Notify(
      NotificationType::NAV_LIST_PRUNED,
      Source<NavigationController>(nav_controller),
      Details<NavigationController::PrunedDetails>(&details));
}

// Ensure the given NavigationEntry has a valid state, so that WebKit does not
// get confused if we navigate back to it.
//
// An empty state is treated as a new navigation by WebKit, which would mean
// losing the navigation entries and generating a new navigation entry after
// this one. We don't want that. To avoid this we create a valid state which
// WebKit will not treat as a new navigation.
void SetContentStateIfEmpty(NavigationEntry* entry) {
  if (entry->content_state().empty()) {
    entry->set_content_state(
        webkit_glue::CreateHistoryStateForURL(entry->url()));
  }
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<linked_ptr<NavigationEntry> >* entries,
    bool from_last_session) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->set_transition_type(PageTransition::RELOAD);
    (*entries)[i]->set_restore_type(from_last_session ?
        NavigationEntry::RESTORE_LAST_SESSION :
        NavigationEntry::RESTORE_CURRENT_SESSION);
    // NOTE(darin): This code is only needed for backwards compat.
    SetContentStateIfEmpty((*entries)[i].get());
  }
}

// See NavigationController::IsURLInPageNavigation for how this works and why.
bool AreURLsInPageNavigation(const GURL& existing_url, const GURL& new_url) {
  if (existing_url == new_url || !new_url.has_ref()) {
    // TODO(jcampan): what about when navigating back from a ref URL to the top
    // non ref URL? Nothing is loaded in that case but we return false here.
    // The user could also navigate from the ref URL to the non ref URL by
    // entering the non ref URL in the location bar or through a bookmark, in
    // which case there would be a load.  I am not sure if the non-load/load
    // scenarios can be differentiated with the TransitionType.
    return false;
  }

  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      new_url.ReplaceComponents(replacements);
}

}  // namespace

// NavigationController ---------------------------------------------------

// static
size_t NavigationController::max_entry_count_ =
    chrome::kMaxSessionHistoryEntries;

// static
bool NavigationController::check_for_repost_ = true;

NavigationController::NavigationController(
    TabContents* contents,
    Profile* profile,
    SessionStorageNamespace* session_storage_namespace)
    : profile_(profile),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      tab_contents_(contents),
      max_restored_page_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_manager_(this)),
      needs_reload_(false),
      session_storage_namespace_(session_storage_namespace),
      pending_reload_(NO_RELOAD) {
  DCHECK(profile_);
  if (!session_storage_namespace_)
    session_storage_namespace_ = new SessionStorageNamespace(profile_);
}

NavigationController::~NavigationController() {
  DiscardNonCommittedEntriesInternal();

  NotificationService::current()->Notify(
      NotificationType::TAB_CLOSED,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
}

void NavigationController::RestoreFromState(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(entry_count() == 0 && !pending_entry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(navigations.size()));

  // Populate entries_ from the supplied TabNavigations.
  needs_reload_ = true;
  CreateNavigationEntriesFromTabNavigations(navigations, &entries_);

  // And finish the restore.
  FinishRestore(selected_navigation, from_last_session);
}

void NavigationController::Reload(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD);
}
void NavigationController::ReloadIgnoringCache(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_IGNORING_CACHE);
}

void NavigationController::ReloadInternal(bool check_for_repost,
                                          ReloadType reload_type) {
  // Reloading a transient entry does nothing.
  if (transient_entry_index_ != -1)
    return;

  DiscardNonCommittedEntriesInternal();
  int current_index = GetCurrentEntryIndex();
  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (current_index == -1) {
    return;
  }

  if (check_for_repost_ && check_for_repost &&
      GetEntryAtIndex(current_index)->has_post_data()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    NotificationService::current()->Notify(
        NotificationType::REPOST_WARNING_SHOWN,
        Source<NavigationController>(this),
        NotificationService::NoDetails());

    pending_reload_ = reload_type;
    tab_contents_->Activate();
    tab_contents_->delegate()->ShowRepostFormWarningDialog(tab_contents_);
  } else {
    DiscardNonCommittedEntriesInternal();

    pending_entry_index_ = current_index;
    entries_[pending_entry_index_]->set_transition_type(PageTransition::RELOAD);
    NavigateToPendingEntry(reload_type);
  }
}

void NavigationController::CancelPendingReload() {
  DCHECK(pending_reload_ != NO_RELOAD);
  pending_reload_ = NO_RELOAD;
}

void NavigationController::ContinuePendingReload() {
  if (pending_reload_ == NO_RELOAD) {
    NOTREACHED();
  } else {
    ReloadInternal(false, pending_reload_);
    pending_reload_ = NO_RELOAD;
  }
}

bool NavigationController::IsInitialNavigation() {
  return last_document_loaded_.is_null();
}

// static
NavigationEntry* NavigationController::CreateNavigationEntry(
    const GURL& url, const GURL& referrer, PageTransition::Type transition,
    Profile* profile) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandler::RewriteURLIfNecessary(
      &loaded_url, profile, &reverse_on_redirect);

  NavigationEntry* entry = new NavigationEntry(
      NULL,  // The site instance for tabs is sent on navigation
             // (TabContents::GetSiteInstance).
      -1,
      loaded_url,
      referrer,
      string16(),
      transition);
  entry->set_virtual_url(url);
  entry->set_user_typed_url(url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  if (url.SchemeIsFile()) {
    // Use the filename as the title, not the full path.
    // We need to call FormatUrl() to perform URL de-escaping;
    // it's a bit ugly to grab the filename out of the resulting string.
    std::string languages =
        profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
    std::wstring formatted = UTF16ToWideHack(net::FormatUrl(url, languages));
    std::wstring filename =
        FilePath::FromWStringHack(formatted).BaseName().ToWStringHack();
    entry->set_title(WideToUTF16Hack(filename));
  }
  return entry;
}

NavigationEntry* NavigationController::GetEntryWithPageID(
    SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationController::LoadEntry(NavigationEntry* entry) {
  // Handle non-navigational URLs that popup dialogs and such, these should not
  // actually navigate.
  if (HandleNonNavigationAboutURL(entry->url()))
    return;

  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  NotificationService::current()->Notify(
      NotificationType::NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
  NavigateToPendingEntry(NO_RELOAD);
}

NavigationEntry* NavigationController::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationController::GetCurrentEntryIndex() const {
  if (transient_entry_index_ != -1)
    return transient_entry_index_;
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntry* NavigationController::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationController::CanViewSource() const {
  bool is_supported_mime_type = net::IsSupportedNonImageMimeType(
      tab_contents_->contents_mime_type().c_str());
  NavigationEntry* active_entry = GetActiveEntry();
  return active_entry && !active_entry->IsViewSourceMode() &&
    is_supported_mime_type;
}

NavigationEntry* NavigationController::GetEntryAtOffset(int offset) const {
  int index = (transient_entry_index_ != -1) ?
                  transient_entry_index_ + offset :
                  last_committed_entry_index_ + offset;
  if (index < 0 || index >= entry_count())
    return NULL;

  return entries_[index].get();
}

bool NavigationController::CanGoBack() const {
  return entries_.size() > 1 && GetCurrentEntryIndex() > 0;
}

bool NavigationController::CanGoForward() const {
  int index = GetCurrentEntryIndex();
  return index >= 0 && index < (static_cast<int>(entries_.size()) - 1);
}

void NavigationController::GoBack() {
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // If an interstitial page is showing, going back is equivalent to hiding the
  // interstitial.
  if (tab_contents_->interstitial_page()) {
    tab_contents_->interstitial_page()->DontProceed();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index - 1;
  entries_[pending_entry_index_]->set_transition_type(
      entries_[pending_entry_index_]->transition_type() |
      PageTransition::FORWARD_BACK);
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationController::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.  Unblock (and disable) it to allow this
  // navigation to succeed.  The interstitial will stay visible until the
  // resulting DidNavigate.
  if (tab_contents_->interstitial_page()) {
    tab_contents_->interstitial_page()->CancelForNavigation();
  }

  bool transient = (transient_entry_index_ != -1);

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index;
  // If there was a transient entry, we removed it making the current index
  // the next page.
  if (!transient)
    pending_entry_index_++;

  entries_[pending_entry_index_]->set_transition_type(
      entries_[pending_entry_index_]->transition_type() |
      PageTransition::FORWARD_BACK);
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationController::GoToIndex(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  if (transient_entry_index_ != -1) {
    if (index == transient_entry_index_) {
      // Nothing to do when navigating to the transient.
      return;
    }
    if (index > transient_entry_index_) {
      // Removing the transient is goint to shift all entries by 1.
      index--;
    }
  }

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.
  if (tab_contents_->interstitial_page()) {
    if (index == GetCurrentEntryIndex() - 1) {
      // Going back one entry is equivalent to hiding the interstitial.
      tab_contents_->interstitial_page()->DontProceed();
      return;
    } else {
      // Unblock the renderer (and disable the interstitial) to allow this
      // navigation to succeed.  The interstitial will stay visible until the
      // resulting DidNavigate.
      tab_contents_->interstitial_page()->CancelForNavigation();
    }
  }

  DiscardNonCommittedEntries();

  pending_entry_index_ = index;
  entries_[pending_entry_index_]->set_transition_type(
      entries_[pending_entry_index_]->transition_type() |
      PageTransition::FORWARD_BACK);
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationController::GoToOffset(int offset) {
  int index = (transient_entry_index_ != -1) ?
                  transient_entry_index_ + offset :
                  last_committed_entry_index_ + offset;
  if (index < 0 || index >= entry_count())
    return;

  GoToIndex(index);
}

void NavigationController::RemoveEntryAtIndex(int index,
                                              const GURL& default_url) {
  int size = static_cast<int>(entries_.size());
  DCHECK(index < size);

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);

  if (last_committed_entry_index_ == index) {
    last_committed_entry_index_--;
    // We removed the currently shown entry, so we have to load something else.
    if (last_committed_entry_index_ != -1) {
      pending_entry_index_ = last_committed_entry_index_;
      NavigateToPendingEntry(NO_RELOAD);
    } else {
      // If there is nothing to show, show a default page.
      LoadURL(default_url.is_empty() ? GURL("about:blank") : default_url,
              GURL(), PageTransition::START_PAGE);
    }
  } else if (last_committed_entry_index_ > index) {
    last_committed_entry_index_--;
  }
}

void NavigationController::UpdateVirtualURLToURL(
    NavigationEntry* entry, const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandler::ReverseURLRewrite(
          &new_virtual_url, entry->virtual_url(), profile_)) {
    entry->set_virtual_url(new_virtual_url);
  }
}

void NavigationController::AddTransientEntry(NavigationEntry* entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(entries_.begin() + index, linked_ptr<NavigationEntry>(entry));
  transient_entry_index_ = index;
  tab_contents_->NotifyNavigationStateChanged(kInvalidateAllButShelves);
}

void NavigationController::LoadURL(const GURL& url, const GURL& referrer,
                                   PageTransition::Type transition) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, referrer, transition,
                                                 profile_);

  LoadEntry(entry);
}

void NavigationController::DocumentLoadedInFrame() {
  last_document_loaded_ = base::TimeTicks::Now();
}

bool NavigationController::RendererDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params,
    int extra_invalidate_flags,
    LoadCommittedDetails* details) {

  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->url();
    details->previous_entry_index = last_committed_entry_index();
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // Assign the current site instance to any pending entry, so we can find it
  // later by calling GetEntryIndexWithPageID. We only care about this if the
  // pending entry is an existing navigation and not a new one (or else we
  // wouldn't care about finding it with GetEntryIndexWithPageID).
  //
  // TODO(brettw) this seems slightly bogus as we don't really know if the
  // pending entry is what this navigation is for. There is a similar TODO
  // w.r.t. the pending entry in RendererDidNavigateToNewPage.
  if (pending_entry_index_ >= 0) {
    pending_entry_->set_site_instance(tab_contents_->GetSiteInstance());
    pending_entry_->set_restore_type(NavigationEntry::RESTORE_NONE);
  }

  // is_in_page must be computed before the entry gets committed.
  details->is_in_page = IsURLInPageNavigation(params.url);

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(params);

  switch (details->type) {
    case NavigationType::NEW_PAGE:
      RendererDidNavigateToNewPage(params, &(details->did_replace_entry));
      break;
    case NavigationType::EXISTING_PAGE:
      RendererDidNavigateToExistingPage(params);
      break;
    case NavigationType::SAME_PAGE:
      RendererDidNavigateToSamePage(params);
      break;
    case NavigationType::IN_PAGE:
      RendererDidNavigateInPage(params, &(details->did_replace_entry));
      break;
    case NavigationType::NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(params);
      break;
    case NavigationType::AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(params))
        return false;
      break;
    case NavigationType::NAV_IGNORE:
      // There is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      return false;
    default:
      NOTREACHED();
  }

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  SetContentStateIfEmpty(GetActiveEntry());

  // WebKit doesn't set the "auto" transition on meta refreshes properly (bug
  // 1051891) so we manually set it for redirects which we normally treat as
  // "non-user-gestures" where we want to update stuff after navigations.
  //
  // Note that the redirect check also checks for a pending entry to
  // differentiate real redirects from browser initiated navigations to a
  // redirected entry. This happens when you hit back to go to a page that was
  // the destination of a redirect, we don't want to treat it as a redirect
  // even though that's what its transition will be. See bug 1117048.
  //
  // TODO(brettw) write a test for this complicated logic.
  details->is_auto = (PageTransition::IsRedirect(params.transition) &&
                      !pending_entry()) ||
      params.gesture == NavigationGestureAuto;

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = GetActiveEntry();
  details->is_main_frame = PageTransition::IsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->is_content_filtered = params.is_content_filtered;
  details->http_status_code = params.http_status_code;
  NotifyNavigationEntryCommitted(details, extra_invalidate_flags);

  return true;
}

NavigationType::Type NavigationController::ClassifyNavigation(
    const ViewHostMsg_FrameNavigate_Params& params) const {
  if (params.page_id == -1) {
    // The renderer generates the page IDs, and so if it gives us the invalid
    // page ID (-1) we know it didn't actually navigate. This happens in a few
    // cases:
    //
    // - If a page makes a popup navigated to about blank, and then writes
    //   stuff like a subframe navigated to a real page. We'll get the commit
    //   for the subframe, but there won't be any commit for the outer page.
    //
    // - We were also getting these for failed loads (for example, bug 21849).
    //   The guess is that we get a "load commit" for the alternate error page,
    //   but that doesn't affect the page ID, so we get the "old" one, which
    //   could be invalid. This can also happen for a cross-site transition
    //   that causes us to swap processes. Then the error page load will be in
    //   a new process with no page IDs ever assigned (and hence a -1 value),
    //   yet the navigation controller still might have previous pages in its
    //   list.
    //
    // In these cases, there's nothing we can do with them, so ignore.
    return NavigationType::NAV_IGNORE;
  }

  if (params.page_id > tab_contents_->GetMaxPageID()) {
    // Greater page IDs than we've ever seen before are new pages. We may or may
    // not have a pending entry for the page, and this may or may not be the
    // main frame.
    if (PageTransition::IsMainFrame(params.transition))
      return NavigationType::NEW_PAGE;

    // When this is a new subframe navigation, we should have a committed page
    // for which it's a suframe in. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NavigationType::NAV_IGNORE;

    // Valid subframe navigation.
    return NavigationType::NEW_SUBFRAME;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);
  if (existing_entry_index == -1) {
    // The page was not found. It could have been pruned because of the limit on
    // back/forward entries (not likely since we'll usually tell it to navigate
    // to such entries). It could also mean that the renderer is smoking crack.
    NOTREACHED();
    return NavigationType::NAV_IGNORE;
  }
  NavigationEntry* existing_entry = entries_[existing_entry_index].get();

  if (!PageTransition::IsMainFrame(params.transition)) {
    // All manual subframes would get new IDs and were handled above, so we
    // know this is auto. Since the current page was found in the navigation
    // entry list, we're guaranteed to have a last committed entry.
    DCHECK(GetLastCommittedEntry());
    return NavigationType::AUTO_SUBFRAME;
  }

  // Anything below here we know is a main frame navigation.
  if (pending_entry_ &&
      existing_entry != pending_entry_ &&
      pending_entry_->page_id() == -1) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebKit will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). In this case, we want to just ignore the pending entry and go
    // back to where we were (the "existing entry").
    return NavigationType::SAME_PAGE;
  }

  // Any toplevel navigations with the same base (minus the reference fragment)
  // are in-page navigations. We weeded out subframe navigations above. Most of
  // the time this doesn't matter since WebKit doesn't tell us about subframe
  // navigations that don't actually navigate, but it can happen when there is
  // an encoding override (it always sends a navigation request).
  if (AreURLsInPageNavigation(existing_entry->url(), params.url))
    return NavigationType::IN_PAGE;

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NavigationType::EXISTING_PAGE;
}

bool NavigationController::IsRedirect(
  const ViewHostMsg_FrameNavigate_Params& params) {
  // For main frame transition, we judge by params.transition.
  // Otherwise, by params.redirects.
  if (PageTransition::IsMainFrame(params.transition)) {
    return PageTransition::IsRedirect(params.transition);
  }
  return params.redirects.size() > 1;
}

void NavigationController::CreateNavigationEntriesFromTabNavigations(
    const std::vector<TabNavigation>& navigations,
    std::vector<linked_ptr<NavigationEntry> >* entries) {
  // Create a NavigationEntry for each of the navigations.
  int page_id = 0;
  for (std::vector<TabNavigation>::const_iterator i =
           navigations.begin(); i != navigations.end(); ++i, ++page_id) {
    linked_ptr<NavigationEntry> entry(i->ToNavigationEntry(page_id, profile_));
    entries->push_back(entry);
  }
}

void NavigationController::RendererDidNavigateToNewPage(
    const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry) {
  NavigationEntry* new_entry;
  if (pending_entry_) {
    // TODO(brettw) this assumes that the pending entry is appropriate for the
    // new page that was just loaded. I don't think this is necessarily the
    // case! We should have some more tracking to know for sure. This goes along
    // with a similar TODO at the top of RendererDidNavigate where we blindly
    // set the site instance on the pending entry.
    new_entry = new NavigationEntry(*pending_entry_);

    // Don't use the page type from the pending entry. Some interstitial page
    // may have set the type to interstitial. Once we commit, however, the page
    // type must always be normal.
    new_entry->set_page_type(NORMAL_PAGE);
  } else {
    new_entry = new NavigationEntry;
  }

  new_entry->set_url(params.url);
  if (new_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(new_entry, params.url);
  new_entry->set_referrer(params.referrer);
  new_entry->set_page_id(params.page_id);
  new_entry->set_transition_type(params.transition);
  new_entry->set_site_instance(tab_contents_->GetSiteInstance());
  new_entry->set_has_post_data(params.is_post);

  InsertOrReplaceEntry(new_entry, *did_replace_entry);
}

void NavigationController::RendererDidNavigateToExistingPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(PageTransition::IsMainFrame(params.transition));

  // This is a back/forward navigation. The existing page for the ID is
  // guaranteed to exist by ClassifyNavigation, and we just need to update it
  // with new information from the renderer.
  int entry_index = GetEntryIndexWithPageID(tab_contents_->GetSiteInstance(),
                                            params.page_id);
  DCHECK(entry_index >= 0 &&
         entry_index < static_cast<int>(entries_.size()));
  NavigationEntry* entry = entries_[entry_index].get();

  // The URL may have changed due to redirects. The site instance will normally
  // be the same except during session restore, when no site instance will be
  // assigned.
  entry->set_url(params.url);
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);
  DCHECK(entry->site_instance() == NULL ||
         entry->site_instance() == tab_contents_->GetSiteInstance());
  entry->set_site_instance(tab_contents_->GetSiteInstance());

  entry->set_has_post_data(params.is_post);

  // The entry we found in the list might be pending if the user hit
  // back/forward/reload. This load should commit it (since it's already in the
  // list, we can just discard the pending pointer).
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  if (entry == pending_entry_)
    DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(tab_contents_->GetSiteInstance(), params.page_id);
}

void NavigationController::RendererDidNavigateToSamePage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // This mode implies we have a pending entry that's the same as an existing
  // entry for this page ID. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  existing_entry->set_unique_id(pending_entry_->unique_id());

  // The URL may have changed due to redirects.
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);
  existing_entry->set_url(params.url);

  DiscardNonCommittedEntries();
}

void NavigationController::RendererDidNavigateInPage(
    const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry) {
  DCHECK(PageTransition::IsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).
  NavigationEntry* new_entry = new NavigationEntry(*existing_entry);
  new_entry->set_page_id(params.page_id);
  if (new_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(new_entry, params.url);
  new_entry->set_url(params.url);

  // This replaces the existing entry since the page ID didn't change.
  *did_replace_entry = true;
  InsertOrReplaceEntry(new_entry, true);
}

void NavigationController::RendererDidNavigateNewSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransition::StripQualifier(params.transition) ==
      PageTransition::AUTO_SUBFRAME) {
    // This is not user-initiated. Ignore.
    return;
  }

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";
  NavigationEntry* new_entry = new NavigationEntry(*GetLastCommittedEntry());
  new_entry->set_page_id(params.page_id);
  InsertOrReplaceEntry(new_entry, false);
}

bool NavigationController::RendererDidNavigateAutoSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // Handle the case where we're navigating back/forward to a previous subframe
  // navigation entry. This is case "2." in NAV_AUTO_SUBFRAME comment in the
  // header file. In case "1." this will be a NOP.
  int entry_index = GetEntryIndexWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);
  if (entry_index < 0 ||
      entry_index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return false;
  }

  // Update the current navigation entry in case we're going back/forward.
  if (entry_index != last_committed_entry_index_) {
    last_committed_entry_index_ = entry_index;
    return true;
  }
  return false;
}

// TODO(brettw) I think this function is unnecessary.
void NavigationController::CommitPendingEntry() {
  DiscardTransientEntry();

  if (!pending_entry())
    return;  // Nothing to do.

  // Need to save the previous URL for the notification.
  LoadCommittedDetails details;
  if (GetLastCommittedEntry()) {
    details.previous_url = GetLastCommittedEntry()->url();
    details.previous_entry_index = last_committed_entry_index();
  } else {
    details.previous_entry_index = -1;
  }

  if (pending_entry_index_ >= 0) {
    // This is a previous navigation (back/forward) that we're just now
    // committing. Just mark it as committed.
    details.type = NavigationType::EXISTING_PAGE;
    int new_entry_index = pending_entry_index_;
    DiscardNonCommittedEntriesInternal();

    // Mark that entry as committed.
    last_committed_entry_index_ = new_entry_index;
  } else {
    // This is a new navigation. It's easiest to just copy the entry and insert
    // it new again, since InsertOrReplaceEntry expects to take ownership and
    // also discard the pending entry. We also need to synthesize a page ID. We
    // can only do this because this function will only be called by our custom
    // TabContents types. For TabContents, the IDs are generated by the
    // renderer, so we can't do this.
    details.type = NavigationType::NEW_PAGE;
    pending_entry_->set_page_id(tab_contents_->GetMaxPageID() + 1);
    tab_contents_->UpdateMaxPageID(pending_entry_->page_id());
    InsertOrReplaceEntry(new NavigationEntry(*pending_entry_), false);
  }

  // Broadcast the notification of the navigation.
  details.entry = GetActiveEntry();
  details.is_auto = false;
  details.is_in_page = AreURLsInPageNavigation(details.previous_url,
                                               details.entry->url());
  details.is_main_frame = true;
  NotifyNavigationEntryCommitted(&details, 0);
}

int NavigationController::GetIndexOfEntry(
    const NavigationEntry* entry) const {
  const NavigationEntries::const_iterator i(std::find(
      entries_.begin(),
      entries_.end(),
      entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

bool NavigationController::IsURLInPageNavigation(const GURL& url) const {
  NavigationEntry* last_committed = GetLastCommittedEntry();
  if (!last_committed)
    return false;
  return AreURLsInPageNavigation(last_committed->url(), url);
}

void NavigationController::CopyStateFrom(const NavigationController& source) {
  // Verify that we look new.
  DCHECK(entry_count() == 0 && !pending_entry());

  if (source.entry_count() == 0)
    return;  // Nothing new to do.

  needs_reload_ = true;
  InsertEntriesFrom(source, source.entry_count());

  session_storage_namespace_ = source.session_storage_namespace_->Clone();

  FinishRestore(source.last_committed_entry_index_, false);
}

void NavigationController::CopyStateFromAndPrune(NavigationController* source) {
  // This code is intended for use when the last entry is the active entry.
  DCHECK((transient_entry_index_ != -1 &&
          transient_entry_index_ == entry_count() - 1) ||
         (pending_entry_ && (pending_entry_index_ == -1 ||
                             pending_entry_index_ == entry_count() - 1)) ||
         (!pending_entry_ && last_committed_entry_index_ == entry_count() - 1));

  // Remove all the entries leaving the active entry.
  PruneAllButActive();

  // Insert the entries from source. Don't use source->GetCurrentEntryIndex as
  // we don't want to copy over the transient entry.
  int max_source_index = source->pending_entry_index_ != -1 ?
      source->pending_entry_index_ : source->last_committed_entry_index_;
  if (max_source_index == -1)
    max_source_index = source->entry_count();
  else
    max_source_index++;
  InsertEntriesFrom(*source, max_source_index);

  // Adjust indices such that the last entry and pending are at the end now.
  last_committed_entry_index_ = entry_count() - 1;
  if (pending_entry_index_ != -1)
    pending_entry_index_ = entry_count() - 1;
  if (transient_entry_index_ != -1) {
    // There's a transient entry. In this case we want the last committed to
    // point to the previous entry.
    transient_entry_index_ = entry_count() - 1;
    if (last_committed_entry_index_ != -1)
      last_committed_entry_index_--;
  }
}

void NavigationController::PruneAllButActive() {
  if (transient_entry_index_ != -1) {
    // There is a transient entry. Prune up to it.
    DCHECK_EQ(entry_count() - 1, transient_entry_index_);
    entries_.erase(entries_.begin(), entries_.begin() + transient_entry_index_);
    transient_entry_index_ = 0;
    last_committed_entry_index_ = -1;
    pending_entry_index_ = -1;
  } else if (!pending_entry_) {
    // There's no pending entry. Leave the last entry (if there is one).
    if (!entry_count())
      return;

    DCHECK(last_committed_entry_index_ >= 0);
    entries_.erase(entries_.begin(),
                   entries_.begin() + last_committed_entry_index_);
    entries_.erase(entries_.begin() + 1, entries_.end());
    last_committed_entry_index_ = 0;
  } else if (pending_entry_index_ != -1) {
    entries_.erase(entries_.begin(), entries_.begin() + pending_entry_index_);
    entries_.erase(entries_.begin() + 1, entries_.end());
    pending_entry_index_ = 0;
    last_committed_entry_index_ = 0;
  } else {
    // There is a pending_entry, but it's not in entries_.
    pending_entry_index_ = -1;
    last_committed_entry_index_ = -1;
    entries_.clear();
  }

  if (tab_contents_->interstitial_page()) {
    // Normally the interstitial page hides itself if the user doesn't proceeed.
    // This would result in showing a NavigationEntry we just removed. Set this
    // so the interstitial triggers a reload if the user doesn't proceed.
    tab_contents_->interstitial_page()->set_reload_on_dont_proceed(true);
  }
}

void NavigationController::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    tab_contents_->NotifyNavigationStateChanged(kInvalidateAllButShelves);
  }
}

void NavigationController::InsertOrReplaceEntry(NavigationEntry* entry,
                                                bool replace) {
  DCHECK(entry->transition_type() != PageTransition::AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntry* const pending_entry = (pending_entry_index_ == -1) ?
      pending_entry_ : entries_[pending_entry_index_].get();
  if (pending_entry)
    entry->set_unique_id(pending_entry->unique_id());

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  if (current_size > 0) {
    // Prune any entries which are in front of the current entry.
    // Also prune the current entry if we are to replace the current entry.
    int prune_up_to = replace ? last_committed_entry_index_ - 1
                              : last_committed_entry_index_;
    int num_pruned = 0;
    while (prune_up_to < (current_size - 1)) {
      num_pruned++;
      entries_.pop_back();
      current_size--;
    }
    if (num_pruned > 0)  // Only notify if we did prune something.
      NotifyPrunedEntries(this, false, num_pruned);
  }

  if (entries_.size() >= max_entry_count_) {
    RemoveEntryAtIndex(0, GURL());
    NotifyPrunedEntries(this, true, 1);
  }

  entries_.push_back(linked_ptr<NavigationEntry>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  // This is a new page ID, so we need everybody to know about it.
  tab_contents_->UpdateMaxPageID(entry->page_id());
}

void NavigationController::SetWindowID(const SessionID& id) {
  window_id_ = id;
  NotificationService::current()->Notify(NotificationType::TAB_PARENTED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

void NavigationController::NavigateToPendingEntry(ReloadType reload_type) {
  needs_reload_ = false;

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (!tab_contents_->NavigateToPendingEntry(reload_type))
    DiscardNonCommittedEntries();
}

void NavigationController::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details,
    int extra_invalidate_flags) {
  details->entry = GetActiveEntry();
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);

  // We need to notify the ssl_manager_ before the tab_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(notification_details);

  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  tab_contents_->NotifyNavigationStateChanged(
      kInvalidateAllButShelves | extra_invalidate_flags);

  NotificationService::current()->Notify(
      NotificationType::NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      notification_details);
}

// static
void NavigationController::DisablePromptOnRepost() {
  check_for_repost_ = false;
}

void NavigationController::SetActive(bool is_active) {
  if (is_active && needs_reload_)
    LoadIfNecessary();
}

void NavigationController::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  pending_entry_index_ = last_committed_entry_index_;
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationController::NotifyEntryChanged(const NavigationEntry* entry,
                                              int index) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = index;
  NotificationService::current()->Notify(NotificationType::NAV_ENTRY_CHANGED,
                                         Source<NavigationController>(this),
                                         Details<EntryChangedDetails>(&det));
}

void NavigationController::FinishRestore(int selected_index,
                                         bool from_last_session) {
  DCHECK(selected_index >= 0 && selected_index < entry_count());
  ConfigureEntriesForRestore(&entries_, from_last_session);

  set_max_restored_page_id(static_cast<int32>(entry_count()));

  last_committed_entry_index_ = selected_index;
}

void NavigationController::DiscardNonCommittedEntriesInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;

  DiscardTransientEntry();
}

void NavigationController::DiscardTransientEntry() {
  if (transient_entry_index_ == -1)
    return;
  entries_.erase(entries_.begin() + transient_entry_index_);
  if (last_committed_entry_index_ > transient_entry_index_)
    last_committed_entry_index_--;
  transient_entry_index_ = -1;
}

int NavigationController::GetEntryIndexWithPageID(
    SiteInstance* instance, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->site_instance() == instance) &&
        (entries_[i]->page_id() == page_id))
      return i;
  }
  return -1;
}

NavigationEntry* NavigationController::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return NULL;
  return entries_[transient_entry_index_].get();
}

void NavigationController::InsertEntriesFrom(
    const NavigationController& source,
    int max_index) {
  DCHECK_LE(max_index, source.entry_count());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages
    if (source.entries_[i].get()->page_type() != INTERSTITIAL_PAGE) {
      entries_.insert(entries_.begin() + insert_index++,
                      linked_ptr<NavigationEntry>(
                          new NavigationEntry(*source.entries_[i])));
    }
  }
}
