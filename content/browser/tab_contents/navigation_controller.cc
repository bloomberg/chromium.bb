// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/navigation_controller.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"  // Temporary
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_context.h"
#include "content/browser/browser_url_handler.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/renderer_host/render_view_host.h"  // Temporary
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_constants.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const int kInvalidateAll = 0xFFFFFFFF;

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationController* nav_controller,
                         bool from_front,
                         int count) {
  content::PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_LIST_PRUNED,
      content::Source<NavigationController>(nav_controller),
      content::Details<content::PrunedDetails>(&details));
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
    (*entries)[i]->set_transition_type(content::PAGE_TRANSITION_RELOAD);
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
    content::kMaxSessionHistoryEntries;

// static
bool NavigationController::check_for_repost_ = true;

NavigationController::NavigationController(
    TabContents* contents,
    content::BrowserContext* browser_context,
    SessionStorageNamespace* session_storage_namespace)
    : browser_context_(browser_context),
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
  DCHECK(browser_context_);
  if (!session_storage_namespace_) {
    session_storage_namespace_ = new SessionStorageNamespace(
        browser_context_->GetWebKitContext());
  }
}

NavigationController::~NavigationController() {
  DiscardNonCommittedEntriesInternal();

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CLOSED,
      content::Source<NavigationController>(this),
      content::NotificationService::NoDetails());
}

void NavigationController::Restore(
    int selected_navigation,
    bool from_last_session,
    std::vector<NavigationEntry*>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(entry_count() == 0 && !pending_entry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));

  needs_reload_ = true;
  for (size_t i = 0; i < entries->size(); ++i)
    entries_.push_back(linked_ptr<NavigationEntry>((*entries)[i]));
  entries->clear();

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
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_REPOST_WARNING_SHOWN,
        content::Source<NavigationController>(this),
        content::NotificationService::NoDetails());

    pending_reload_ = reload_type;
    tab_contents_->Activate();
    tab_contents_->delegate()->ShowRepostFormWarningDialog(tab_contents_);
  } else {
    DiscardNonCommittedEntriesInternal();

    pending_entry_index_ = current_index;
    entries_[pending_entry_index_]->set_transition_type(
        content::PAGE_TRANSITION_RELOAD);
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
    const GURL& url, const GURL& referrer, content::PageTransition transition,
    bool is_renderer_initiated, const std::string& extra_headers,
    content::BrowserContext* browser_context) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, browser_context, &reverse_on_redirect);

  NavigationEntry* entry = new NavigationEntry(
      NULL,  // The site instance for tabs is sent on navigation
             // (TabContents::GetSiteInstance).
      -1,
      loaded_url,
      referrer,
      string16(),
      transition,
      is_renderer_initiated);
  entry->set_virtual_url(url);
  entry->set_user_typed_url(url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return entry;
}

NavigationEntry* NavigationController::GetEntryWithPageID(
    SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationController::LoadEntry(NavigationEntry* entry) {
  // Don't navigate to URLs disabled by policy. This prevents showing the URL
  // on the Omnibar when it is also going to be blocked by
  // ChildProcessSecurityPolicy::CanRequestURL.
  ChildProcessSecurityPolicy *policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (policy->IsDisabledScheme(entry->url().scheme()) ||
      policy->IsDisabledScheme(entry->virtual_url().scheme())) {
    VLOG(1) << "URL not loaded because the scheme is blocked by policy: "
            << entry->url();
    delete entry;
    return;
  }

  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::Source<NavigationController>(this),
      content::Details<NavigationEntry>(entry));
  NavigateToPendingEntry(NO_RELOAD);
}

NavigationEntry* NavigationController::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntry* NavigationController::GetVisibleEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  // Only return the pending_entry for new (non-history), browser-initiated
  // navigations, in order to prevent URL spoof attacks.
  // Ideally we would also show the pending entry's URL for new renderer-
  // initiated navigations with no last committed entry (e.g., a link opening
  // in a new tab), but an attacker can insert content into the about:blank
  // page while the pending URL loads in that case.
  if (pending_entry_ &&
      pending_entry_->page_id() == -1 &&
      !pending_entry_->is_renderer_initiated())
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
      content::PageTransitionFromInt(
          entries_[pending_entry_index_]->transition_type() |
          content::PAGE_TRANSITION_FORWARD_BACK));
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
      content::PageTransitionFromInt(
          entries_[pending_entry_index_]->transition_type() |
          content::PAGE_TRANSITION_FORWARD_BACK));
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
      content::PageTransitionFromInt(
          entries_[pending_entry_index_]->transition_type() |
          content::PAGE_TRANSITION_FORWARD_BACK));
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
  bool is_current = index == last_committed_entry_index_;
  RemoveEntryAtIndexInternal(index);
  if (is_current) {
    // We removed the currently shown entry, so we have to load something else.
    if (last_committed_entry_index_ != -1) {
      pending_entry_index_ = last_committed_entry_index_;
      NavigateToPendingEntry(NO_RELOAD);
    } else {
      // If there is nothing to show, show a default page.
      LoadURL(default_url.is_empty() ? GURL("about:blank") : default_url,
              GURL(), content::PAGE_TRANSITION_START_PAGE, std::string());
    }
  }
}

void NavigationController::UpdateVirtualURLToURL(
    NavigationEntry* entry, const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandler::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->virtual_url(), browser_context_)) {
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
  tab_contents_->NotifyNavigationStateChanged(kInvalidateAll);
}

void NavigationController::TransferURL(
    const GURL& url,
    const GURL& referrer,
    content::PageTransition transition,
    const std::string& extra_headers,
    const GlobalRequestID& transferred_global_request_id,
    bool is_renderer_initiated) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, referrer, transition,
                                                 is_renderer_initiated,
                                                 extra_headers,
                                                 browser_context_);
  entry->set_transferred_global_request_id(transferred_global_request_id);

  LoadEntry(entry);
}

void NavigationController::LoadURL(
    const GURL& url,
    const GURL& referrer,
    content::PageTransition transition,
    const std::string& extra_headers) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, referrer, transition,
                                                 false,
                                                 extra_headers,
                                                 browser_context_);

  LoadEntry(entry);
}

void NavigationController::LoadURLFromRenderer(
    const GURL& url,
    const GURL& referrer,
    content::PageTransition transition,
    const std::string& extra_headers) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, referrer, transition,
                                                 true,
                                                 extra_headers,
                                                 browser_context_);

  LoadEntry(entry);
}

void NavigationController::DocumentLoadedInFrame() {
  last_document_loaded_ = base::TimeTicks::Now();
}

bool NavigationController::RendererDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params,
    content::LoadCommittedDetails* details) {

  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->url();
    details->previous_entry_index = last_committed_entry_index();
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // If we have a pending entry at this point, it should have a SiteInstance.
  // Restored entries start out with a null SiteInstance, but we should have
  // assigned one in NavigateToPendingEntry.
  DCHECK(pending_entry_index_ == -1 || pending_entry_->site_instance());

  // is_in_page must be computed before the entry gets committed.
  details->is_in_page = IsURLInPageNavigation(params.url);

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(params);

  switch (details->type) {
    case content::NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(params, &(details->did_replace_entry));
      break;
    case content::NAVIGATION_TYPE_EXISTING_PAGE:
      RendererDidNavigateToExistingPage(params);
      break;
    case content::NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(params);
      break;
    case content::NAVIGATION_TYPE_IN_PAGE:
      RendererDidNavigateInPage(params, &(details->did_replace_entry));
      break;
    case content::NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(params);
      break;
    case content::NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(params))
        return false;
      break;
    case content::NAVIGATION_TYPE_NAV_IGNORE:
      // If a pending navigation was in progress, this canceled it.  We should
      // discard it and make sure it is removed from the URL bar.  After that,
      // there is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      if (pending_entry_) {
        DiscardNonCommittedEntries();
        tab_contents_->NotifyNavigationStateChanged(
            TabContents::INVALIDATE_URL);
      }
      return false;
    default:
      NOTREACHED();
  }

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(!params.content_state.empty());
  NavigationEntry* active_entry = GetActiveEntry();
  active_entry->set_content_state(params.content_state);

  // Once committed, we do not need to track if the entry was initiated by
  // the renderer.
  active_entry->set_is_renderer_initiated(false);

  // The active entry's SiteInstance should match our SiteInstance.
  DCHECK(active_entry->site_instance() == tab_contents_->GetSiteInstance());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame =
      content::PageTransitionIsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->http_status_code = params.http_status_code;
  NotifyNavigationEntryCommitted(details);

  return true;
}

content::NavigationType NavigationController::ClassifyNavigation(
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
    return content::NAVIGATION_TYPE_NAV_IGNORE;
  }

  if (params.page_id > tab_contents_->GetMaxPageID()) {
    // Greater page IDs than we've ever seen before are new pages. We may or may
    // not have a pending entry for the page, and this may or may not be the
    // main frame.
    if (content::PageTransitionIsMainFrame(params.transition))
      return content::NAVIGATION_TYPE_NEW_PAGE;

    // When this is a new subframe navigation, we should have a committed page
    // for which it's a suframe in. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return content::NAVIGATION_TYPE_NAV_IGNORE;

    // Valid subframe navigation.
    return content::NAVIGATION_TYPE_NEW_SUBFRAME;
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

    // Because the unknown entry has committed, we risk showing the wrong URL in
    // release builds. Instead, we'll kill the renderer process to be safe.
    LOG(ERROR) << "terminating renderer for bad navigation: " << params.url;
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_NC"));

    // Temporary code so we can get more information.  Format:
    //  http://url/foo.html#page1#max3#frame1#ids:2_Nx,1_1x,3_2
    std::string temp = params.url.spec();
    temp.append("#page");
    temp.append(base::IntToString(params.page_id));
    temp.append("#max");
    temp.append(base::IntToString(tab_contents_->GetMaxPageID()));
    temp.append("#frame");
    temp.append(base::IntToString(params.frame_id));
    temp.append("#ids");
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
      // Append entry metadata (e.g., 3_7x):
      //  3: page_id
      //  7: SiteInstance ID, or N for null
      //  x: appended if not from the current SiteInstance
      temp.append(base::IntToString(entries_[i]->page_id()));
      temp.append("_");
      if (entries_[i]->site_instance())
        temp.append(base::IntToString(entries_[i]->site_instance()->id()));
      else
        temp.append("N");
      if (entries_[i]->site_instance() != tab_contents_->GetSiteInstance())
        temp.append("x");
      temp.append(",");
    }
    GURL url(temp);
    tab_contents_->render_view_host()->Send(new ViewMsg_TempCrashWithData(url));
    return content::NAVIGATION_TYPE_NAV_IGNORE;


    if (tab_contents_->GetSiteInstance()->HasProcess())
      tab_contents_->GetSiteInstance()->GetProcess()->ReceivedBadMessage();
    return content::NAVIGATION_TYPE_NAV_IGNORE;
  }
  NavigationEntry* existing_entry = entries_[existing_entry_index].get();

  if (!content::PageTransitionIsMainFrame(params.transition)) {
    // All manual subframes would get new IDs and were handled above, so we
    // know this is auto. Since the current page was found in the navigation
    // entry list, we're guaranteed to have a last committed entry.
    DCHECK(GetLastCommittedEntry());
    return content::NAVIGATION_TYPE_AUTO_SUBFRAME;
  }

  // Anything below here we know is a main frame navigation.
  if (pending_entry_ &&
      existing_entry != pending_entry_ &&
      pending_entry_->page_id() == -1 &&
      existing_entry == GetLastCommittedEntry()) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebKit will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). If this matches the last committed entry, we want to just ignore
    // the pending entry and go back to where we were (the "existing entry").
    return content::NAVIGATION_TYPE_SAME_PAGE;
  }

  // Any toplevel navigations with the same base (minus the reference fragment)
  // are in-page navigations. We weeded out subframe navigations above. Most of
  // the time this doesn't matter since WebKit doesn't tell us about subframe
  // navigations that don't actually navigate, but it can happen when there is
  // an encoding override (it always sends a navigation request).
  if (AreURLsInPageNavigation(existing_entry->url(), params.url))
    return content::NAVIGATION_TYPE_IN_PAGE;

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return content::NAVIGATION_TYPE_EXISTING_PAGE;
}

bool NavigationController::IsRedirect(
  const ViewHostMsg_FrameNavigate_Params& params) {
  // For main frame transition, we judge by params.transition.
  // Otherwise, by params.redirects.
  if (content::PageTransitionIsMainFrame(params.transition)) {
    return content::PageTransitionIsRedirect(params.transition);
  }
  return params.redirects.size() > 1;
}

void NavigationController::RendererDidNavigateToNewPage(
    const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry) {
  NavigationEntry* new_entry;
  bool update_virtual_url;
  if (pending_entry_) {
    // TODO(brettw) this assumes that the pending entry is appropriate for the
    // new page that was just loaded. I don't think this is necessarily the
    // case! We should have some more tracking to know for sure.
    new_entry = new NavigationEntry(*pending_entry_);

    // Don't use the page type from the pending entry. Some interstitial page
    // may have set the type to interstitial. Once we commit, however, the page
    // type must always be normal.
    new_entry->set_page_type(content::PAGE_TYPE_NORMAL);
    update_virtual_url = new_entry->update_virtual_url_with_url();
  } else {
    new_entry = new NavigationEntry;
    // When navigating to a new page, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    update_virtual_url = true;
  }

  new_entry->set_url(params.url);
  if (update_virtual_url)
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
  DCHECK(content::PageTransitionIsMainFrame(params.transition));

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
  // list, we can just discard the pending pointer).  We should also discard the
  // pending entry if it corresponds to a different navigation, since that one
  // is now likely canceled.  If it is not canceled, we will treat it as a new
  // navigation when it arrives, which is also ok.
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  if (pending_entry_)
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
  DCHECK(content::PageTransitionIsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).  We'll update the URL of the existing
  // entry without pruning the forward history.
  existing_entry->set_url(params.url);
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);

  // This replaces the existing entry since the page ID didn't change.
  *did_replace_entry = true;

  if (pending_entry_)
    DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(tab_contents_->GetSiteInstance(), params.page_id);
}

void NavigationController::RendererDidNavigateNewSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (content::PageTransitionStripQualifier(params.transition) ==
      content::PAGE_TRANSITION_AUTO_SUBFRAME) {
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
  // The SiteInstance and page_id of the last committed entry needs to be
  // remembered at this point, in case there is only one committed entry
  // and it is pruned.
  NavigationEntry* last_committed = GetLastCommittedEntry();
  SiteInstance* site_instance =
      last_committed ? last_committed->site_instance() : NULL;
  int32 minimum_page_id = last_committed ? last_committed->page_id() : -1;

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

  tab_contents_->SetHistoryLengthAndPrune(site_instance,
                                          max_source_index,
                                          minimum_page_id);
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

void NavigationController::RemoveEntryAtIndexInternal(int index) {
  DCHECK(index < entry_count());

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ == index)
    last_committed_entry_index_--;
  else if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

void NavigationController::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    tab_contents_->NotifyNavigationStateChanged(kInvalidateAll);
  }
}

void NavigationController::InsertOrReplaceEntry(NavigationEntry* entry,
                                                bool replace) {
  DCHECK(entry->transition_type() != content::PAGE_TRANSITION_AUTO_SUBFRAME);

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
    // last_committed_entry_index_ must be updated here since calls to
    // NotifyPrunedEntries() below may re-enter and we must make sure
    // last_committed_entry_index_ is not left in an invalid state.
    if (replace)
      --last_committed_entry_index_;

    int num_pruned = 0;
    while (last_committed_entry_index_ < (current_size - 1)) {
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

void NavigationController::NavigateToPendingEntry(ReloadType reload_type) {
  needs_reload_ = false;

  // If we were navigating to a slow-to-commit page, and the user performs
  // a session history navigation to the last committed page, RenderViewHost
  // will force the throbber to start, but WebKit will essentially ignore the
  // navigation, and won't send a message to stop the throbber. To prevent this
  // from happening, we drop the navigation here and stop the slow-to-commit
  // page from loading (which would normally happen during the navigation).
  if (pending_entry_index_ != -1 &&
      pending_entry_index_ == last_committed_entry_index_ &&
      (entries_[pending_entry_index_]->restore_type() ==
          NavigationEntry::RESTORE_NONE) &&
      (entries_[pending_entry_index_]->transition_type() &
          content::PAGE_TRANSITION_FORWARD_BACK)) {
    tab_contents_->Stop();
    DiscardNonCommittedEntries();
    return;
  }

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (!tab_contents_->NavigateToPendingEntry(reload_type))
    DiscardNonCommittedEntries();

  // If the entry is being restored and doesn't have a SiteInstance yet, fill
  // it in now that we know. This allows us to find the entry when it commits.
  // This works for browser-initiated navigations. We handle renderer-initiated
  // navigations to restored entries in TabContents::OnGoToEntryAtOffset.
  if (pending_entry_ && !pending_entry_->site_instance() &&
      pending_entry_->restore_type() != NavigationEntry::RESTORE_NONE) {
    pending_entry_->set_site_instance(tab_contents_->GetPendingSiteInstance());
    pending_entry_->set_restore_type(NavigationEntry::RESTORE_NONE);
  }
}

void NavigationController::NotifyNavigationEntryCommitted(
    content::LoadCommittedDetails* details) {
  details->entry = GetActiveEntry();
  content::NotificationDetails notification_details =
      content::Details<content::LoadCommittedDetails>(details);

  // We need to notify the ssl_manager_ before the tab_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(notification_details);

  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  tab_contents_->NotifyNavigationStateChanged(kInvalidateAll);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(this),
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
  content::EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = index;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<NavigationController>(this),
      content::Details<content::EntryChangedDetails>(&det));
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
    if (source.entries_[i].get()->page_type() !=
        content::PAGE_TYPE_INTERSTITIAL) {
      entries_.insert(entries_.begin() + insert_index++,
                      linked_ptr<NavigationEntry>(
                          new NavigationEntry(*source.entries_[i])));
    }
  }
}
