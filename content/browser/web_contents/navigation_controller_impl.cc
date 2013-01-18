// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/navigation_controller_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"  // Temporary
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"  // Temporary
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/debug_urls.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/glue_serialize.h"

namespace content {
namespace {

const int kInvalidateAll = 0xFFFFFFFF;

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationControllerImpl* nav_controller,
                         bool from_front,
                         int count) {
  PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_LIST_PRUNED,
      Source<NavigationController>(nav_controller),
      Details<PrunedDetails>(&details));
}

// Ensure the given NavigationEntry has a valid state, so that WebKit does not
// get confused if we navigate back to it.
//
// An empty state is treated as a new navigation by WebKit, which would mean
// losing the navigation entries and generating a new navigation entry after
// this one. We don't want that. To avoid this we create a valid state which
// WebKit will not treat as a new navigation.
void SetContentStateIfEmpty(NavigationEntryImpl* entry) {
  if (entry->GetContentState().empty()) {
    entry->SetContentState(
        webkit_glue::CreateHistoryStateForURL(entry->GetURL()));
  }
}

NavigationEntryImpl::RestoreType ControllerRestoreTypeToEntryType(
    NavigationController::RestoreType type) {
  switch (type) {
    case NavigationController::RESTORE_CURRENT_SESSION:
      return NavigationEntryImpl::RESTORE_CURRENT_SESSION;
    case NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY:
      return NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY;
    case NavigationController::RESTORE_LAST_SESSION_CRASHED:
      return NavigationEntryImpl::RESTORE_LAST_SESSION_CRASHED;
  }
  NOTREACHED();
  return NavigationEntryImpl::RESTORE_CURRENT_SESSION;
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<linked_ptr<NavigationEntryImpl> >* entries,
    NavigationController::RestoreType type) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->SetTransitionType(PAGE_TRANSITION_RELOAD);
    (*entries)[i]->set_restore_type(ControllerRestoreTypeToEntryType(type));
    // NOTE(darin): This code is only needed for backwards compat.
    SetContentStateIfEmpty((*entries)[i].get());
  }
}

// See NavigationController::IsURLInPageNavigation for how this works and why.
bool AreURLsInPageNavigation(const GURL& existing_url,
                             const GURL& new_url,
                             bool renderer_says_in_page) {
  if (existing_url == new_url)
    return renderer_says_in_page;

  if (!new_url.has_ref()) {
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

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(const NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

}  // namespace

// NavigationControllerImpl ----------------------------------------------------

const size_t kMaxEntryCountForTestingNotSet = -1;

// static
size_t NavigationControllerImpl::max_entry_count_for_testing_ =
    kMaxEntryCountForTestingNotSet;

 // Should Reload check for post data? The default is true, but is set to false
// when testing.
static bool g_check_for_repost = true;

// static
NavigationEntry* NavigationController::CreateNavigationEntry(
      const GURL& url,
      const Referrer& referrer,
      PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, browser_context, &reverse_on_redirect);

  NavigationEntryImpl* entry = new NavigationEntryImpl(
      NULL,  // The site instance for tabs is sent on navigation
             // (WebContents::GetSiteInstance).
      -1,
      loaded_url,
      referrer,
      string16(),
      transition,
      is_renderer_initiated);
  entry->SetVirtualURL(url);
  entry->set_user_typed_url(url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return entry;
}

// static
void NavigationController::DisablePromptOnRepost() {
  g_check_for_repost = false;
}

base::Time NavigationControllerImpl::TimeSmoother::GetSmoothedTime(
    base::Time t) {
  // If |t| is between the water marks, we're in a run of duplicates
  // or just getting out of it, so increase the high-water mark to get
  // a time that probably hasn't been used before and return it.
  if (low_water_mark_ <= t && t <= high_water_mark_) {
    high_water_mark_ += base::TimeDelta::FromMicroseconds(1);
    return high_water_mark_;
  }

  // Otherwise, we're clear of the last duplicate run, so reset the
  // water marks.
  low_water_mark_ = high_water_mark_ = t;
  return t;
}

NavigationControllerImpl::NavigationControllerImpl(
    WebContentsImpl* web_contents,
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      web_contents_(web_contents),
      max_restored_page_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_manager_(this)),
      needs_reload_(false),
      is_initial_navigation_(true),
      pending_reload_(NO_RELOAD),
      get_timestamp_callback_(base::Bind(&base::Time::Now)),
      screenshot_count_(0) {
  DCHECK(browser_context_);
}

NavigationControllerImpl::~NavigationControllerImpl() {
  DiscardNonCommittedEntriesInternal();
}

WebContents* NavigationControllerImpl::GetWebContents() const {
  return web_contents_;
}

BrowserContext* NavigationControllerImpl::GetBrowserContext() const {
  return browser_context_;
}

void NavigationControllerImpl::SetBrowserContext(
    BrowserContext* browser_context) {
  browser_context_ = browser_context;
}

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<NavigationEntry*>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));

  needs_reload_ = true;
  for (size_t i = 0; i < entries->size(); ++i) {
    NavigationEntryImpl* entry =
        NavigationEntryImpl::FromNavigationEntry((*entries)[i]);
    entries_.push_back(linked_ptr<NavigationEntryImpl>(entry));
  }
  entries->clear();

  // And finish the restore.
  FinishRestore(selected_navigation, type);
}

void NavigationControllerImpl::Reload(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD);
}
void NavigationControllerImpl::ReloadIgnoringCache(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_IGNORING_CACHE);
}
void NavigationControllerImpl::ReloadOriginalRequestURL(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_ORIGINAL_REQUEST_URL);
}

void NavigationControllerImpl::ReloadInternal(bool check_for_repost,
                                              ReloadType reload_type) {
  if (transient_entry_index_ != -1) {
    // If an interstitial is showing, treat a reload as a navigation to the
    // transient entry's URL.
    NavigationEntryImpl* active_entry =
        NavigationEntryImpl::FromNavigationEntry(GetActiveEntry());
    if (!active_entry)
      return;
    LoadURL(active_entry->GetURL(),
            Referrer(),
            PAGE_TRANSITION_RELOAD,
            active_entry->extra_headers());
    return;
  }

  DiscardNonCommittedEntriesInternal();
  int current_index = GetCurrentEntryIndex();
  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (current_index == -1) {
    return;
  }

  if (g_check_for_repost && check_for_repost &&
      GetEntryAtIndex(current_index)->GetHasPostData()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    NotificationService::current()->Notify(
        NOTIFICATION_REPOST_WARNING_SHOWN,
        Source<NavigationController>(this),
        NotificationService::NoDetails());

    pending_reload_ = reload_type;
    web_contents_->Activate();
    web_contents_->GetDelegate()->ShowRepostFormWarningDialog(web_contents_);
  } else {
    DiscardNonCommittedEntriesInternal();

    NavigationEntryImpl* entry = entries_[current_index].get();
    SiteInstanceImpl* site_instance = entry->site_instance();
    DCHECK(site_instance);

    // If we are reloading an entry that no longer belongs to the current
    // site instance (for example, refreshing a page for just installed app),
    // the reload must happen in a new process.
    // The new entry must have a new page_id and site instance, so it behaves
    // as new navigation (which happens to clear forward history).
    // Tabs that are discarded due to low memory conditions may not have a site
    // instance, and should not be treated as a cross-site reload.
    if (site_instance &&
        site_instance->HasWrongProcessForURL(entry->GetURL())) {
      // Create a navigation entry that resembles the current one, but do not
      // copy page id, site instance, content state, or timestamp.
      NavigationEntryImpl* nav_entry = NavigationEntryImpl::FromNavigationEntry(
          CreateNavigationEntry(
              entry->GetURL(), entry->GetReferrer(), entry->GetTransitionType(),
              false, entry->extra_headers(), browser_context_));

      // Mark the reload type as NO_RELOAD, so navigation will not be considered
      // a reload in the renderer.
      reload_type = NavigationController::NO_RELOAD;

      nav_entry->set_should_replace_entry(true);
      pending_entry_ = nav_entry;
    } else {
      pending_entry_index_ = current_index;

      // The title of the page being reloaded might have been removed in the
      // meanwhile, so we need to revert to the default title upon reload and
      // invalidate the previously cached title (SetTitle will do both).
      // See Chromium issue 96041.
      entries_[pending_entry_index_]->SetTitle(string16());

      entries_[pending_entry_index_]->SetTransitionType(PAGE_TRANSITION_RELOAD);
    }

    NavigateToPendingEntry(reload_type);
  }
}

void NavigationControllerImpl::CancelPendingReload() {
  DCHECK(pending_reload_ != NO_RELOAD);
  pending_reload_ = NO_RELOAD;
}

void NavigationControllerImpl::ContinuePendingReload() {
  if (pending_reload_ == NO_RELOAD) {
    NOTREACHED();
  } else {
    ReloadInternal(false, pending_reload_);
    pending_reload_ = NO_RELOAD;
  }
}

bool NavigationControllerImpl::IsInitialNavigation() {
  return is_initial_navigation_;
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryWithPageID(
  SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationControllerImpl::LoadEntry(NavigationEntryImpl* entry) {
  // Don't navigate to URLs disabled by policy. This prevents showing the URL
  // on the Omnibar when it is also going to be blocked by
  // ChildProcessSecurityPolicy::CanRequestURL.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsDisabledScheme(entry->GetURL().scheme()) ||
      policy->IsDisabledScheme(entry->GetVirtualURL().scheme())) {
    VLOG(1) << "URL not loaded because the scheme is blocked by policy: "
            << entry->GetURL();
    delete entry;
    return;
  }

  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      Details<NavigationEntry>(entry));
  NavigateToPendingEntry(NO_RELOAD);
}

NavigationEntry* NavigationControllerImpl::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntry* NavigationControllerImpl::GetVisibleEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  // Only return the pending_entry for new (non-history), browser-initiated
  // navigations, in order to prevent URL spoof attacks.
  // Ideally we would also show the pending entry's URL for new renderer-
  // initiated navigations with no last committed entry (e.g., a link opening
  // in a new tab), but an attacker can insert content into the about:blank
  // page while the pending URL loads in that case.
  if (pending_entry_ &&
      pending_entry_->GetPageID() == -1 &&
      !pending_entry_->is_renderer_initiated())
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationControllerImpl::GetCurrentEntryIndex() const {
  if (transient_entry_index_ != -1)
    return transient_entry_index_;
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntry* NavigationControllerImpl::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() const {
  const std::string& mime_type = web_contents_->GetContentsMimeType();
  bool is_viewable_mime_type = net::IsSupportedNonImageMimeType(mime_type) &&
      !net::IsSupportedMediaMimeType(mime_type);
  NavigationEntry* active_entry = GetActiveEntry();
  return active_entry && !active_entry->IsViewSourceMode() &&
      is_viewable_mime_type && !web_contents_->GetInterstitialPage();
}

int NavigationControllerImpl::GetLastCommittedEntryIndex() const {
  return last_committed_entry_index_;
}

int NavigationControllerImpl::GetEntryCount() const {
  DCHECK(entries_.size() <= max_entry_count());
  return static_cast<int>(entries_.size());
}

NavigationEntry* NavigationControllerImpl::GetEntryAtIndex(
    int index) const {
  return entries_.at(index).get();
}

NavigationEntry* NavigationControllerImpl::GetEntryAtOffset(
    int offset) const {
  int index = GetIndexForOffset(offset);
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
}

int NavigationControllerImpl::GetIndexForOffset(int offset) const {
  return GetCurrentEntryIndex() + offset;
}

void NavigationControllerImpl::TakeScreenshot() {
  static bool overscroll_enabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableOverscrollHistoryNavigation);
  if (!overscroll_enabled)
    return;

  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry());
  if (!entry)
    return;

  RenderViewHost* render_view_host = web_contents_->GetRenderViewHost();
  content::RenderWidgetHostView* view = render_view_host->GetView();
  if (!view)
    return;

  if (!take_screenshot_callback_.is_null())
    take_screenshot_callback_.Run(render_view_host);

  skia::PlatformBitmap* temp_bitmap = new skia::PlatformBitmap;
  render_view_host->CopyFromBackingStore(gfx::Rect(),
      view->GetViewBounds().size(),
      base::Bind(&NavigationControllerImpl::OnScreenshotTaken,
                 base::Unretained(this),
                 entry->GetUniqueID(),
                 base::Owned(temp_bitmap)),
      temp_bitmap);
}

void NavigationControllerImpl::OnScreenshotTaken(
    int unique_id,
    skia::PlatformBitmap* bitmap,
    bool success) {
  NavigationEntryImpl* entry = NULL;
  for (NavigationEntries::iterator i = entries_.begin();
       i != entries_.end();
       ++i) {
    if ((*i)->GetUniqueID() == unique_id) {
      entry = (*i).get();
      break;
    }
  }

  if (!entry) {
    LOG(ERROR) << "Invalid entry with unique id: " << unique_id;
    return;
  }

  if (!success) {
    LOG(ERROR) << "Taking snapshot was unsuccessful for "
               << unique_id;
    ClearScreenshot(entry);
    return;
  }

  std::vector<unsigned char> data;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap->GetBitmap(), true, &data)) {
    if (!entry->screenshot())
      ++screenshot_count_;
    entry->SetScreenshotPNGData(data);
    PurgeScreenshotsIfNecessary();
  } else {
    ClearScreenshot(entry);
  }
  CHECK_GE(screenshot_count_, 0);
}

void NavigationControllerImpl::ClearScreenshot(NavigationEntryImpl* entry) {
  if (entry->screenshot()) {
    --screenshot_count_;
    entry->SetScreenshotPNGData(std::vector<unsigned char>());
  }
}

void NavigationControllerImpl::PurgeScreenshotsIfNecessary() {
  // Allow only a certain number of entries to keep screenshots.
  const int kMaxScreenshots = 10;
  if (screenshot_count_ < kMaxScreenshots)
    return;

  const int current = GetCurrentEntryIndex();
  const int num_entries = GetEntryCount();
  int available_slots = kMaxScreenshots;
  if (NavigationEntryImpl::FromNavigationEntry(
          GetEntryAtIndex(current))->screenshot())
    --available_slots;

  // Keep screenshots closer to the current navigation entry, and purge the ones
  // that are farther away from it. So in each step, look at the entries at
  // each offset on both the back and forward history, and start counting them
  // to make sure that the correct number of screenshots are kept in memory.
  // Note that it is possible for some entries to be missing screenshots (e.g.
  // when taking the screenshot failed for some reason). So there may be a state
  // where there are a lot of entries in the back history, but none of them has
  // any screenshot. In such cases, keep the screenshots for |kMaxScreenshots|
  // entries in the forward history list.
  int back = current - 1;
  int forward = current + 1;
  while (available_slots > 0 && (back >= 0 || forward < num_entries)) {
    if (back >= 0) {
      NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
          GetEntryAtIndex(back));
      if (entry->screenshot())
        --available_slots;
      --back;
    }

    if (available_slots > 0 && forward < num_entries) {
      NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
          GetEntryAtIndex(forward));
      if (entry->screenshot())
        --available_slots;
      ++forward;
    }
  }

  // Purge any screenshot at |back| or lower indices, and |forward| or higher
  // indices.

  while (screenshot_count_ > kMaxScreenshots && back >= 0) {
    NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
        GetEntryAtIndex(back));
    ClearScreenshot(entry);
    --back;
  }

  while (screenshot_count_ > kMaxScreenshots && forward < num_entries) {
    NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
        GetEntryAtIndex(forward));
    ClearScreenshot(entry);
    ++forward;
  }
  CHECK_GE(screenshot_count_, 0);
  CHECK_LE(screenshot_count_, kMaxScreenshots);
}

bool NavigationControllerImpl::CanGoBack() const {
  return entries_.size() > 1 && GetCurrentEntryIndex() > 0;
}

bool NavigationControllerImpl::CanGoForward() const {
  int index = GetCurrentEntryIndex();
  return index >= 0 && index < (static_cast<int>(entries_.size()) - 1);
}

bool NavigationControllerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationControllerImpl::GoBack() {
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index - 1;
  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
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

  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoToIndex(int index) {
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

  DiscardNonCommittedEntries();

  pending_entry_index_ = index;
  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset));
}

void NavigationControllerImpl::RemoveEntryAtIndex(int index) {
  if (index == last_committed_entry_index_)
    return;

  RemoveEntryAtIndexInternal(index);
}

void NavigationControllerImpl::UpdateVirtualURLToURL(
    NavigationEntryImpl* entry, const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandlerImpl::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->GetVirtualURL(), browser_context_)) {
    entry->SetVirtualURL(new_virtual_url);
  }
}

void NavigationControllerImpl::AddTransientEntry(NavigationEntryImpl* entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(
      entries_.begin() + index, linked_ptr<NavigationEntryImpl>(entry));
  transient_entry_index_ = index;
  web_contents_->NotifyNavigationStateChanged(kInvalidateAll);
}

void NavigationControllerImpl::LoadURL(
    const GURL& url,
    const Referrer& referrer,
    PageTransition transition,
    const std::string& extra_headers) {
  LoadURLParams params(url);
  params.referrer = referrer;
  params.transition_type = transition;
  params.extra_headers = extra_headers;
  LoadURLWithParams(params);
}

void NavigationControllerImpl::LoadURLWithParams(const LoadURLParams& params) {
  if (HandleDebugURL(params.url, params.transition_type))
    return;

  // Checks based on params.load_type.
  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_BROWSER_INITIATED_HTTP_POST:
      if (!params.url.SchemeIs(chrome::kHttpScheme) &&
          !params.url.SchemeIs(chrome::kHttpsScheme)) {
        NOTREACHED() << "Http post load must use http(s) scheme.";
        return;
      }
      break;
    case LOAD_TYPE_DATA:
      if (!params.url.SchemeIs(chrome::kDataScheme)) {
        NOTREACHED() << "Data load must use data scheme.";
        return;
      }
      break;
    default:
      NOTREACHED();
      break;
  };

  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  bool override = false;
  switch (params.override_user_agent) {
    case UA_OVERRIDE_INHERIT:
      override = ShouldKeepOverride(GetLastCommittedEntry());
      break;
    case UA_OVERRIDE_TRUE:
      override = true;
      break;
    case UA_OVERRIDE_FALSE:
      override = false;
      break;
    default:
      NOTREACHED();
      break;
  }

  NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      CreateNavigationEntry(
          params.url,
          params.referrer,
          params.transition_type,
          params.is_renderer_initiated,
          params.extra_headers,
          browser_context_));
  if (params.is_cross_site_redirect)
    entry->set_should_replace_entry(true);
  entry->SetIsOverridingUserAgent(override);
  entry->set_transferred_global_request_id(
      params.transferred_global_request_id);

  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_BROWSER_INITIATED_HTTP_POST:
      entry->SetHasPostData(true);
      entry->SetBrowserInitiatedPostData(
          params.browser_initiated_post_data);
      break;
    case LOAD_TYPE_DATA:
      entry->SetBaseURLForDataURL(params.base_url_for_data_url);
      entry->SetVirtualURL(params.virtual_url_for_data_url);
      entry->SetCanLoadLocalResources(params.can_load_local_resources);
      break;
    default:
      NOTREACHED();
      break;
  };

  LoadEntry(entry);
}

void NavigationControllerImpl::DocumentLoadedInFrame() {
  is_initial_navigation_ = false;
}

bool NavigationControllerImpl::RendererDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params,
    LoadCommittedDetails* details) {
  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->GetURL();
    details->previous_entry_index = GetLastCommittedEntryIndex();
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // If we have a pending entry at this point, it should have a SiteInstance.
  // Restored entries start out with a null SiteInstance, but we should have
  // assigned one in NavigateToPendingEntry.
  DCHECK(pending_entry_index_ == -1 || pending_entry_->site_instance());

  // If we are doing a cross-site reload, we need to replace the existing
  // navigation entry, not add another entry to the history. This has the side
  // effect of removing forward browsing history, if such existed.
  // Or if we are doing a cross-site redirect navigation,
  // we will do a similar thing.
  details->did_replace_entry =
      pending_entry_ && pending_entry_->should_replace_entry();

  // is_in_page must be computed before the entry gets committed.
  details->is_in_page = IsURLInPageNavigation(
      params.url, params.was_within_same_page);

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(params);

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(params, details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_EXISTING_PAGE:
      RendererDidNavigateToExistingPage(params);
      break;
    case NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(params);
      break;
    case NAVIGATION_TYPE_IN_PAGE:
      RendererDidNavigateInPage(params, &details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(params);
      break;
    case NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(params))
        return false;
      break;
    case NAVIGATION_TYPE_NAV_IGNORE:
      // If a pending navigation was in progress, this canceled it.  We should
      // discard it and make sure it is removed from the URL bar.  After that,
      // there is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      if (pending_entry_) {
        DiscardNonCommittedEntries();
        web_contents_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
      }
      return false;
    default:
      NOTREACHED();
  }

  // At this point, we know that the navigation has just completed, so
  // record the time.
  //
  // TODO(akalin): Use "sane time" as described in
  // http://www.chromium.org/developers/design-documents/sane-time .
  base::Time timestamp =
      time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
  DVLOG(1) << "Navigation finished at (smoothed) timestamp "
           << timestamp.ToInternalValue();

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(!params.content_state.empty());
  NavigationEntryImpl* active_entry =
      NavigationEntryImpl::FromNavigationEntry(GetActiveEntry());
  active_entry->SetTimestamp(timestamp);
  active_entry->SetContentState(params.content_state);
  // No longer needed since content state will hold the post data if any.
  active_entry->SetBrowserInitiatedPostData(NULL);

  // Once committed, we do not need to track if the entry was initiated by
  // the renderer.
  active_entry->set_is_renderer_initiated(false);

  // The active entry's SiteInstance should match our SiteInstance.
  DCHECK(active_entry->site_instance() == web_contents_->GetSiteInstance());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame =
      PageTransitionIsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->http_status_code = params.http_status_code;
  NotifyNavigationEntryCommitted(details);

  return true;
}

NavigationType NavigationControllerImpl::ClassifyNavigation(
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
    return NAVIGATION_TYPE_NAV_IGNORE;
  }

  if (params.page_id > web_contents_->GetMaxPageID()) {
    // Greater page IDs than we've ever seen before are new pages. We may or may
    // not have a pending entry for the page, and this may or may not be the
    // main frame.
    if (PageTransitionIsMainFrame(params.transition))
      return NAVIGATION_TYPE_NEW_PAGE;

    // When this is a new subframe navigation, we should have a committed page
    // for which it's a suframe in. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NAVIGATION_TYPE_NAV_IGNORE;

    // Valid subframe navigation.
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithPageID(
      web_contents_->GetSiteInstance(),
      params.page_id);
  if (existing_entry_index == -1) {
    // The page was not found. It could have been pruned because of the limit on
    // back/forward entries (not likely since we'll usually tell it to navigate
    // to such entries). It could also mean that the renderer is smoking crack.
    NOTREACHED();

    // Because the unknown entry has committed, we risk showing the wrong URL in
    // release builds. Instead, we'll kill the renderer process to be safe.
    LOG(ERROR) << "terminating renderer for bad navigation: " << params.url;
    RecordAction(UserMetricsAction("BadMessageTerminate_NC"));

    // Temporary code so we can get more information.  Format:
    //  http://url/foo.html#page1#max3#frame1#ids:2_Nx,1_1x,3_2
    std::string temp = params.url.spec();
    temp.append("#page");
    temp.append(base::IntToString(params.page_id));
    temp.append("#max");
    temp.append(base::IntToString(web_contents_->GetMaxPageID()));
    temp.append("#frame");
    temp.append(base::IntToString(params.frame_id));
    temp.append("#ids");
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
      // Append entry metadata (e.g., 3_7x):
      //  3: page_id
      //  7: SiteInstance ID, or N for null
      //  x: appended if not from the current SiteInstance
      temp.append(base::IntToString(entries_[i]->GetPageID()));
      temp.append("_");
      if (entries_[i]->site_instance())
        temp.append(base::IntToString(entries_[i]->site_instance()->GetId()));
      else
        temp.append("N");
      if (entries_[i]->site_instance() != web_contents_->GetSiteInstance())
        temp.append("x");
      temp.append(",");
    }
    GURL url(temp);
    static_cast<RenderViewHostImpl*>(
        web_contents_->GetRenderViewHost())->Send(
            new ViewMsg_TempCrashWithData(url));
    return NAVIGATION_TYPE_NAV_IGNORE;
  }
  NavigationEntryImpl* existing_entry = entries_[existing_entry_index].get();

  if (!PageTransitionIsMainFrame(params.transition)) {
    // All manual subframes would get new IDs and were handled above, so we
    // know this is auto. Since the current page was found in the navigation
    // entry list, we're guaranteed to have a last committed entry.
    DCHECK(GetLastCommittedEntry());
    return NAVIGATION_TYPE_AUTO_SUBFRAME;
  }

  // Anything below here we know is a main frame navigation.
  if (pending_entry_ &&
      existing_entry != pending_entry_ &&
      pending_entry_->GetPageID() == -1 &&
      existing_entry == GetLastCommittedEntry()) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebKit will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). If this matches the last committed entry, we want to just ignore
    // the pending entry and go back to where we were (the "existing entry").
    return NAVIGATION_TYPE_SAME_PAGE;
  }

  // Any toplevel navigations with the same base (minus the reference fragment)
  // are in-page navigations. We weeded out subframe navigations above. Most of
  // the time this doesn't matter since WebKit doesn't tell us about subframe
  // navigations that don't actually navigate, but it can happen when there is
  // an encoding override (it always sends a navigation request).
  if (AreURLsInPageNavigation(existing_entry->GetURL(), params.url,
                              params.was_within_same_page)) {
    return NAVIGATION_TYPE_IN_PAGE;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NAVIGATION_TYPE_EXISTING_PAGE;
}

bool NavigationControllerImpl::IsRedirect(
  const ViewHostMsg_FrameNavigate_Params& params) {
  // For main frame transition, we judge by params.transition.
  // Otherwise, by params.redirects.
  if (PageTransitionIsMainFrame(params.transition)) {
    return PageTransitionIsRedirect(params.transition);
  }
  return params.redirects.size() > 1;
}

void NavigationControllerImpl::RendererDidNavigateToNewPage(
   const ViewHostMsg_FrameNavigate_Params& params, bool replace_entry) {
  NavigationEntryImpl* new_entry;
  bool update_virtual_url;
  if (pending_entry_) {
    // TODO(brettw) this assumes that the pending entry is appropriate for the
    // new page that was just loaded. I don't think this is necessarily the
    // case! We should have some more tracking to know for sure.
    new_entry = new NavigationEntryImpl(*pending_entry_);

    // Don't use the page type from the pending entry. Some interstitial page
    // may have set the type to interstitial. Once we commit, however, the page
    // type must always be normal.
    new_entry->set_page_type(PAGE_TYPE_NORMAL);
    update_virtual_url = new_entry->update_virtual_url_with_url();
  } else {
    new_entry = new NavigationEntryImpl;

    // Find out whether the new entry needs to update its virtual URL on URL
    // change and set up the entry accordingly. This is needed to correctly
    // update the virtual URL when replaceState is called after a pushState.
    GURL url = params.url;
    bool needs_update = false;
    BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
        &url, browser_context_, &needs_update);
    new_entry->set_update_virtual_url_with_url(needs_update);

    // When navigating to a new page, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    update_virtual_url = needs_update;
  }

  new_entry->SetURL(params.url);
  if (update_virtual_url)
    UpdateVirtualURLToURL(new_entry, params.url);
  new_entry->SetReferrer(params.referrer);
  new_entry->SetPageID(params.page_id);
  new_entry->SetTransitionType(params.transition);
  new_entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(web_contents_->GetSiteInstance()));
  new_entry->SetHasPostData(params.is_post);
  new_entry->SetPostID(params.post_id);
  new_entry->SetOriginalRequestURL(params.original_request_url);
  new_entry->SetIsOverridingUserAgent(params.is_overriding_user_agent);

  InsertOrReplaceEntry(new_entry, replace_entry);
}

void NavigationControllerImpl::RendererDidNavigateToExistingPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(PageTransitionIsMainFrame(params.transition));

  // This is a back/forward navigation. The existing page for the ID is
  // guaranteed to exist by ClassifyNavigation, and we just need to update it
  // with new information from the renderer.
  int entry_index = GetEntryIndexWithPageID(web_contents_->GetSiteInstance(),
                                            params.page_id);
  DCHECK(entry_index >= 0 &&
         entry_index < static_cast<int>(entries_.size()));
  NavigationEntryImpl* entry = entries_[entry_index].get();

  // The URL may have changed due to redirects.
  entry->SetURL(params.url);
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (PageTransitionIsRedirect(params.transition))
    entry->GetFavicon() = FaviconStatus();

  // The site instance will normally be the same except during session restore,
  // when no site instance will be assigned.
  DCHECK(entry->site_instance() == NULL ||
         entry->site_instance() == web_contents_->GetSiteInstance());
  entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(web_contents_->GetSiteInstance()));

  entry->SetHasPostData(params.is_post);
  entry->SetPostID(params.post_id);

  // The entry we found in the list might be pending if the user hit
  // back/forward/reload. This load should commit it (since it's already in the
  // list, we can just discard the pending pointer).  We should also discard the
  // pending entry if it corresponds to a different navigation, since that one
  // is now likely canceled.  If it is not canceled, we will treat it as a new
  // navigation when it arrives, which is also ok.
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(web_contents_->GetSiteInstance(), params.page_id);
}

void NavigationControllerImpl::RendererDidNavigateToSamePage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // This mode implies we have a pending entry that's the same as an existing
  // entry for this page ID. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntryImpl* existing_entry = GetEntryWithPageID(
      web_contents_->GetSiteInstance(), params.page_id);

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  existing_entry->set_unique_id(pending_entry_->GetUniqueID());

  // The URL may have changed due to redirects.
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);
  existing_entry->SetURL(params.url);

  DiscardNonCommittedEntries();
}

void NavigationControllerImpl::RendererDidNavigateInPage(
    const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry) {
  DCHECK(PageTransitionIsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntryImpl* existing_entry = GetEntryWithPageID(
      web_contents_->GetSiteInstance(), params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).  We'll update the URL of the existing
  // entry without pruning the forward history.
  existing_entry->SetURL(params.url);
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);

  // This replaces the existing entry since the page ID didn't change.
  *did_replace_entry = true;

  DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(web_contents_->GetSiteInstance(), params.page_id);
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransitionStripQualifier(params.transition) ==
      PAGE_TRANSITION_AUTO_SUBFRAME) {
    // This is not user-initiated. Ignore.
    return;
  }

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";
  NavigationEntryImpl* new_entry = new NavigationEntryImpl(
      *NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry()));
  new_entry->SetPageID(params.page_id);
  InsertOrReplaceEntry(new_entry, false);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // Handle the case where we're navigating back/forward to a previous subframe
  // navigation entry. This is case "2." in NAV_AUTO_SUBFRAME comment in the
  // header file. In case "1." this will be a NOP.
  int entry_index = GetEntryIndexWithPageID(
      web_contents_->GetSiteInstance(),
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

int NavigationControllerImpl::GetIndexOfEntry(
    const NavigationEntryImpl* entry) const {
  const NavigationEntries::const_iterator i(std::find(
      entries_.begin(),
      entries_.end(),
      entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

bool NavigationControllerImpl::IsURLInPageNavigation(
    const GURL& url, bool renderer_says_in_page) const {
  NavigationEntry* last_committed = GetLastCommittedEntry();
  return last_committed && AreURLsInPageNavigation(
      last_committed->GetURL(), url, renderer_says_in_page);
}

void NavigationControllerImpl::CopyStateFrom(
    const NavigationController& temp) {
  const NavigationControllerImpl& source =
      static_cast<const NavigationControllerImpl&>(temp);
  // Verify that we look new.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());

  if (source.GetEntryCount() == 0)
    return;  // Nothing new to do.

  needs_reload_ = true;
  InsertEntriesFrom(source, source.GetEntryCount());

  for (SessionStorageNamespaceMap::const_iterator it =
           source.session_storage_namespace_map_.begin();
       it != source.session_storage_namespace_map_.end();
       ++it) {
    SessionStorageNamespaceImpl* source_namespace =
        static_cast<SessionStorageNamespaceImpl*>(it->second.get());
    session_storage_namespace_map_.insert(
        make_pair(it->first, source_namespace->Clone()));
  }

  FinishRestore(source.last_committed_entry_index_, RESTORE_CURRENT_SESSION);

  // Copy the max page id map from the old tab to the new tab.  This ensures
  // that new and existing navigations in the tab's current SiteInstances
  // are identified properly.
  web_contents_->CopyMaxPageIDsFrom(source.web_contents());
}

void NavigationControllerImpl::CopyStateFromAndPrune(
    NavigationController* temp) {
  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);
  // The SiteInstance and page_id of the last committed entry needs to be
  // remembered at this point, in case there is only one committed entry
  // and it is pruned.  We use a scoped_refptr to ensure the SiteInstance
  // can't be freed during this time period.
  NavigationEntryImpl* last_committed =
      NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry());
  scoped_refptr<SiteInstance> site_instance(
      last_committed ? last_committed->site_instance() : NULL);
  int32 minimum_page_id = last_committed ? last_committed->GetPageID() : -1;
  int32 max_page_id = last_committed ?
      web_contents_->GetMaxPageIDForSiteInstance(site_instance.get()) : -1;

  // This code is intended for use when the last entry is the active entry.
  DCHECK(
      (transient_entry_index_ != -1 &&
       transient_entry_index_ == GetEntryCount() - 1) ||
      (pending_entry_ && (pending_entry_index_ == -1 ||
                          pending_entry_index_ == GetEntryCount() - 1)) ||
      (!pending_entry_ && last_committed_entry_index_ == GetEntryCount() - 1));

  // Remove all the entries leaving the active entry.
  PruneAllButActive();

  // We now have zero or one entries.  Ensure that adding the entries from
  // source won't put us over the limit.
  DCHECK(GetEntryCount() == 0 || GetEntryCount() == 1);
  if (GetEntryCount() > 0)
    source->PruneOldestEntryIfFull();

  // Insert the entries from source. Don't use source->GetCurrentEntryIndex as
  // we don't want to copy over the transient entry.
  int max_source_index = source->pending_entry_index_ != -1 ?
      source->pending_entry_index_ : source->last_committed_entry_index_;
  if (max_source_index == -1)
    max_source_index = source->GetEntryCount();
  else
    max_source_index++;
  InsertEntriesFrom(*source, max_source_index);

  // Adjust indices such that the last entry and pending are at the end now.
  last_committed_entry_index_ = GetEntryCount() - 1;
  if (pending_entry_index_ != -1)
    pending_entry_index_ = GetEntryCount() - 1;
  if (transient_entry_index_ != -1) {
    // There's a transient entry. In this case we want the last committed to
    // point to the previous entry.
    transient_entry_index_ = GetEntryCount() - 1;
    if (last_committed_entry_index_ != -1)
      last_committed_entry_index_--;
  }

  web_contents_->SetHistoryLengthAndPrune(site_instance.get(),
                                          max_source_index,
                                          minimum_page_id);

  // Copy the max page id map from the old tab to the new tab.  This ensures
  // that new and existing navigations in the tab's current SiteInstances
  // are identified properly.
  web_contents_->CopyMaxPageIDsFrom(source->web_contents());

  // If there is a last committed entry, be sure to include it in the new
  // max page ID map.
  if (max_page_id > -1) {
    web_contents_->UpdateMaxPageIDForSiteInstance(site_instance.get(),
                                                  max_page_id);
  }
}

void NavigationControllerImpl::PruneAllButActive() {
  if (transient_entry_index_ != -1) {
    // There is a transient entry. Prune up to it.
    DCHECK_EQ(GetEntryCount() - 1, transient_entry_index_);
    entries_.erase(entries_.begin(), entries_.begin() + transient_entry_index_);
    transient_entry_index_ = 0;
    last_committed_entry_index_ = -1;
    pending_entry_index_ = -1;
  } else if (!pending_entry_) {
    // There's no pending entry. Leave the last entry (if there is one).
    if (!GetEntryCount())
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

  if (web_contents_->GetInterstitialPage()) {
    // Normally the interstitial page hides itself if the user doesn't proceeed.
    // This would result in showing a NavigationEntry we just removed. Set this
    // so the interstitial triggers a reload if the user doesn't proceed.
    static_cast<InterstitialPageImpl*>(web_contents_->GetInterstitialPage())->
        set_reload_on_dont_proceed(true);
  }
}

void NavigationControllerImpl::SetSessionStorageNamespace(
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (!session_storage_namespace)
    return;

  // We can't overwrite an existing SessionStorage without violating spec.
  // Attempts to do so may give a tab access to another tab's session storage
  // so die hard on an error.
  bool successful_insert = session_storage_namespace_map_.insert(
      make_pair(partition_id,
                static_cast<SessionStorageNamespaceImpl*>(
                    session_storage_namespace)))
          .second;
  CHECK(successful_insert) << "Cannot replace existing SessionStorageNamespace";
}

void NavigationControllerImpl::SetMaxRestoredPageID(int32 max_id) {
  max_restored_page_id_ = max_id;
}

int32 NavigationControllerImpl::GetMaxRestoredPageID() const {
  return max_restored_page_id_;
}

SessionStorageNamespace*
NavigationControllerImpl::GetSessionStorageNamespace(SiteInstance* instance) {
  std::string partition_id;
  if (instance) {
    // TODO(ajwong): When GetDefaultSessionStorageNamespace() goes away, remove
    // this if statement so |instance| must not be NULL.
    partition_id =
        GetContentClient()->browser()->GetStoragePartitionIdForSite(
            browser_context_, instance->GetSiteURL());
  }

  SessionStorageNamespaceMap::const_iterator it =
      session_storage_namespace_map_.find(partition_id);
  if (it != session_storage_namespace_map_.end())
    return it->second.get();

  // Create one if no one has accessed session storage for this partition yet.
  //
  // TODO(ajwong): Should this use the |partition_id| directly rather than
  // re-lookup via |instance|?  http://crbug.com/142685
  StoragePartition* partition =
              BrowserContext::GetStoragePartition(browser_context_, instance);
  SessionStorageNamespaceImpl* session_storage_namespace =
      new SessionStorageNamespaceImpl(
          static_cast<DOMStorageContextImpl*>(
              partition->GetDOMStorageContext()));
  session_storage_namespace_map_[partition_id] = session_storage_namespace;

  return session_storage_namespace;
}

SessionStorageNamespace*
NavigationControllerImpl::GetDefaultSessionStorageNamespace() {
  // TODO(ajwong): Remove if statement in GetSessionStorageNamespace().
  return GetSessionStorageNamespace(NULL);
}

const SessionStorageNamespaceMap&
NavigationControllerImpl::GetSessionStorageNamespaceMap() const {
  return session_storage_namespace_map_;
}

bool NavigationControllerImpl::NeedsReload() const {
  return needs_reload_;
}

void NavigationControllerImpl::RemoveEntryAtIndexInternal(int index) {
  DCHECK(index < GetEntryCount());
  DCHECK(index != last_committed_entry_index_);

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

void NavigationControllerImpl::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    web_contents_->NotifyNavigationStateChanged(kInvalidateAll);
  }
}

NavigationEntry* NavigationControllerImpl::GetPendingEntry() const {
  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() const {
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(NavigationEntryImpl* entry,
                                                    bool replace) {
  DCHECK(entry->GetTransitionType() != PAGE_TRANSITION_AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntryImpl* const pending_entry =
      (pending_entry_index_ == -1) ?
          pending_entry_ : entries_[pending_entry_index_].get();
  if (pending_entry)
    entry->set_unique_id(pending_entry->GetUniqueID());

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

  PruneOldestEntryIfFull();

  entries_.push_back(linked_ptr<NavigationEntryImpl>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  // This is a new page ID, so we need everybody to know about it.
  web_contents_->UpdateMaxPageID(entry->GetPageID());
}

void NavigationControllerImpl::PruneOldestEntryIfFull() {
  if (entries_.size() >= max_entry_count()) {
    DCHECK_EQ(max_entry_count(), entries_.size());
    DCHECK(last_committed_entry_index_ > 0);
    RemoveEntryAtIndex(0);
    NotifyPrunedEntries(this, true, 1);
  }
}

void NavigationControllerImpl::NavigateToPendingEntry(ReloadType reload_type) {
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
          NavigationEntryImpl::RESTORE_NONE) &&
      (entries_[pending_entry_index_]->GetTransitionType() &
          PAGE_TRANSITION_FORWARD_BACK)) {
    web_contents_->Stop();

    // If an interstitial page is showing, we want to close it to get back
    // to what was showing before.
    if (web_contents_->GetInterstitialPage())
      web_contents_->GetInterstitialPage()->DontProceed();

    DiscardNonCommittedEntries();
    return;
  }

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.  Unblock (and disable) it to allow this
  // navigation to succeed.  The interstitial will stay visible until the
  // resulting DidNavigate.
  if (web_contents_->GetInterstitialPage()) {
    static_cast<InterstitialPageImpl*>(web_contents_->GetInterstitialPage())->
        CancelForNavigation();
  }

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (!web_contents_->NavigateToPendingEntry(reload_type))
    DiscardNonCommittedEntries();

  // If the entry is being restored and doesn't have a SiteInstance yet, fill
  // it in now that we know. This allows us to find the entry when it commits.
  // This works for browser-initiated navigations. We handle renderer-initiated
  // navigations to restored entries in WebContentsImpl::OnGoToEntryAtOffset.
  if (pending_entry_ && !pending_entry_->site_instance() &&
      pending_entry_->restore_type() != NavigationEntryImpl::RESTORE_NONE) {
    pending_entry_->set_site_instance(static_cast<SiteInstanceImpl*>(
        web_contents_->GetPendingSiteInstance()));
    pending_entry_->set_restore_type(NavigationEntryImpl::RESTORE_NONE);
  }
}

void NavigationControllerImpl::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetActiveEntry();
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);

  // We need to notify the ssl_manager_ before the web_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(notification_details);

  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  web_contents_->NotifyNavigationStateChanged(kInvalidateAll);

  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      notification_details);
}

// static
size_t NavigationControllerImpl::max_entry_count() {
  if (max_entry_count_for_testing_ != kMaxEntryCountForTestingNotSet)
     return max_entry_count_for_testing_;
  return kMaxSessionHistoryEntries;
}

void NavigationControllerImpl::SetActive(bool is_active) {
  if (is_active && needs_reload_)
    LoadIfNecessary();
}

void NavigationControllerImpl::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  pending_entry_index_ = last_committed_entry_index_;
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::NotifyEntryChanged(const NavigationEntry* entry,
                                                  int index) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = index;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_CHANGED,
      Source<NavigationController>(this),
      Details<EntryChangedDetails>(&det));
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  SetMaxRestoredPageID(static_cast<int32>(GetEntryCount()));

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntriesInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;

  DiscardTransientEntry();
}

void NavigationControllerImpl::DiscardTransientEntry() {
  if (transient_entry_index_ == -1)
    return;
  entries_.erase(entries_.begin() + transient_entry_index_);
  if (last_committed_entry_index_ > transient_entry_index_)
    last_committed_entry_index_--;
  transient_entry_index_ = -1;
}

int NavigationControllerImpl::GetEntryIndexWithPageID(
    SiteInstance* instance, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->site_instance() == instance) &&
        (entries_[i]->GetPageID() == page_id))
      return i;
  }
  return -1;
}

NavigationEntry* NavigationControllerImpl::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return NULL;
  return entries_[transient_entry_index_].get();
}

void NavigationControllerImpl::InsertEntriesFrom(
    const NavigationControllerImpl& source,
    int max_index) {
  DCHECK_LE(max_index, source.GetEntryCount());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages
    if (source.entries_[i].get()->GetPageType() !=
        PAGE_TYPE_INTERSTITIAL) {
      entries_.insert(entries_.begin() + insert_index++,
                      linked_ptr<NavigationEntryImpl>(
                          new NavigationEntryImpl(*source.entries_[i])));
    }
  }
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::Callback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

void NavigationControllerImpl::SetTakeScreenshotCallbackForTest(
    const base::Callback<void(RenderViewHost*)>& take_screenshot_callback) {
  take_screenshot_callback_ = take_screenshot_callback;
}

}  // namespace content
