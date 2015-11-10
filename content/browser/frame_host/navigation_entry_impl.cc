// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_entry_impl.h"

#include <queue>

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "content/common/navigation_params.h"
#include "content/common/page_state_serialization.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/text_elider.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

namespace content {

int NavigationEntryImpl::kInvalidBindings = -1;

NavigationEntryImpl::TreeNode::TreeNode(FrameNavigationEntry* frame_entry)
    : frame_entry(frame_entry) {
}

NavigationEntryImpl::TreeNode::~TreeNode() {
}

bool NavigationEntryImpl::TreeNode::MatchesFrame(
    FrameTreeNode* frame_tree_node) const {
  if (frame_tree_node->frame_tree_node_id() ==
      frame_entry->frame_tree_node_id())
    return true;

  // For now, we set the root FNE's FrameTreeNode ID to -1.
  return frame_tree_node->IsMainFrame() &&
         frame_entry->frame_tree_node_id() == -1;
}

scoped_ptr<NavigationEntryImpl::TreeNode>
NavigationEntryImpl::TreeNode::CloneAndReplace(
    FrameTreeNode* frame_tree_node,
    FrameNavigationEntry* frame_navigation_entry) const {
  if (frame_tree_node && MatchesFrame(frame_tree_node)) {
    // Replace this node in the cloned tree and prune its children.
    return make_scoped_ptr(
        new NavigationEntryImpl::TreeNode(frame_navigation_entry));
  }

  // Clone the tree using a copy of the FrameNavigationEntry, without sharing.
  // TODO(creis): Share FNEs unless it's for another tab.
  scoped_ptr<NavigationEntryImpl::TreeNode> copy(
      new NavigationEntryImpl::TreeNode(frame_entry->Clone()));

  // Recursively clone the children.
  for (auto& child : children) {
    copy->children.push_back(
        child->CloneAndReplace(frame_tree_node, frame_navigation_entry));
  }

  return copy.Pass();
}

scoped_ptr<NavigationEntry> NavigationEntry::Create() {
  return make_scoped_ptr(new NavigationEntryImpl()).Pass();
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

const NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    const NavigationEntry* entry) {
  return static_cast<const NavigationEntryImpl*>(entry);
}

scoped_ptr<NavigationEntryImpl> NavigationEntryImpl::FromNavigationEntry(
    scoped_ptr<NavigationEntry> entry) {
  return make_scoped_ptr(FromNavigationEntry(entry.release()));
}

NavigationEntryImpl::NavigationEntryImpl()
    : NavigationEntryImpl(nullptr, -1, GURL(), Referrer(), base::string16(),
                          ui::PAGE_TRANSITION_LINK, false) {
}

NavigationEntryImpl::NavigationEntryImpl(SiteInstanceImpl* instance,
                                         int page_id,
                                         const GURL& url,
                                         const Referrer& referrer,
                                         const base::string16& title,
                                         ui::PageTransition transition_type,
                                         bool is_renderer_initiated)
    : frame_tree_(new TreeNode(
          new FrameNavigationEntry(-1, "", -1, -1, instance, url, referrer))),
      unique_id_(GetUniqueIDInConstructor()),
      bindings_(kInvalidBindings),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      post_id_(-1),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1) {
#if defined(OS_ANDROID)
  has_user_gesture_ = false;
#endif
}

NavigationEntryImpl::~NavigationEntryImpl() {
}

int NavigationEntryImpl::GetUniqueID() const {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() const {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() const {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() const {
  return base_url_for_data_url_;
}

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() const {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationEntryImpl::GetTitle() const {
  return title_;
}

void NavigationEntryImpl::SetPageState(const PageState& state) {
  frame_tree_->frame_entry->set_page_state(state);

  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // Also get the root ISN and DSN out of the PageState.
    ExplodedPageState exploded_state;
    if (!DecodePageState(state.ToEncodedData(), &exploded_state))
      return;
    frame_tree_->frame_entry->set_item_sequence_number(
        exploded_state.top.item_sequence_number);
    frame_tree_->frame_entry->set_document_sequence_number(
        exploded_state.top.document_sequence_number);
  }
}

const PageState& NavigationEntryImpl::GetPageState() const {
  return frame_tree_->frame_entry->page_state();
}

void NavigationEntryImpl::SetPageID(int page_id) {
  page_id_ = page_id;
}

int32 NavigationEntryImpl::GetPageID() const {
  return page_id_;
}

void NavigationEntryImpl::set_site_instance(SiteInstanceImpl* site_instance) {
  // TODO(creis): Update all callers and remove this method.
  frame_tree_->frame_entry->set_site_instance(site_instance);
}

void NavigationEntryImpl::set_source_site_instance(
    SiteInstanceImpl* source_site_instance) {
  source_site_instance_ = source_site_instance;
}

void NavigationEntryImpl::SetBindings(int bindings) {
  // Ensure this is set to a valid value, and that it stays the same once set.
  CHECK_NE(bindings, kInvalidBindings);
  CHECK(bindings_ == kInvalidBindings || bindings_ == bindings);
  bindings_ = bindings;
}

const base::string16& NavigationEntryImpl::GetTitleForDisplay(
    const std::string& languages) const {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  base::string16 title;
  if (!virtual_url_.is_empty()) {
    title = url_formatter::FormatUrl(virtual_url_, languages);
  } else if (!GetURL().is_empty()) {
    title = url_formatter::FormatUrl(GetURL(), languages);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    base::string16::size_type refpos = title.find('#');
    base::string16::size_type querypos = title.find('?');
    base::string16::size_type lastpos;
    if (refpos == base::string16::npos)
      lastpos = querypos;
    else if (querypos == base::string16::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    base::string16::size_type slashpos = title.rfind('/', lastpos);
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);
  }

  gfx::ElideString(title, kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() const {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() const {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  has_post_data_ = has_post_data;
}

bool NavigationEntryImpl::GetHasPostData() const {
  return has_post_data_;
}

void NavigationEntryImpl::SetPostID(int64 post_id) {
  post_id_ = post_id;
}

int64 NavigationEntryImpl::GetPostID() const {
  return post_id_;
}

void NavigationEntryImpl::SetBrowserInitiatedPostData(
    const base::RefCountedMemory* data) {
  browser_initiated_post_data_ = data;
}

const base::RefCountedMemory*
NavigationEntryImpl::GetBrowserInitiatedPostData() const {
  return browser_initiated_post_data_.get();
}


const FaviconStatus& NavigationEntryImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

const SSLStatus& NavigationEntryImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() const {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override) {
  is_overriding_user_agent_ = override;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() const {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() const {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  redirect_chain_ = redirect_chain;
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() const {
  return redirect_chain_;
}

bool NavigationEntryImpl::IsRestored() const {
  return restore_type_ != RESTORE_NONE;
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() const {
  return can_load_local_resources_;
}

void NavigationEntryImpl::SetExtraData(const std::string& key,
                                       const base::string16& data) {
  extra_data_[key] = data;
}

bool NavigationEntryImpl::GetExtraData(const std::string& key,
                                       base::string16* data) const {
  std::map<std::string, base::string16>::const_iterator iter =
      extra_data_.find(key);
  if (iter == extra_data_.end())
    return false;
  *data = iter->second;
  return true;
}

void NavigationEntryImpl::ClearExtraData(const std::string& key) {
  extra_data_.erase(key);
}

scoped_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  return NavigationEntryImpl::CloneAndReplace(nullptr, nullptr);
}

scoped_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    FrameTreeNode* frame_tree_node,
    FrameNavigationEntry* frame_navigation_entry) const {
  scoped_ptr<NavigationEntryImpl> copy =
      make_scoped_ptr(new NavigationEntryImpl());

  // TODO(creis): Only share the same FrameNavigationEntries if cloning within
  // the same tab.
  copy->frame_tree_ =
      frame_tree_->CloneAndReplace(frame_tree_node, frame_navigation_entry);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
  copy->bindings_ = bindings_;
  copy->page_type_ = page_type_;
  copy->virtual_url_ = virtual_url_;
  copy->update_virtual_url_with_url_ = update_virtual_url_with_url_;
  copy->title_ = title_;
  copy->favicon_ = favicon_;
  copy->page_id_ = page_id_;
  copy->ssl_ = ssl_;
  copy->transition_type_ = transition_type_;
  copy->user_typed_url_ = user_typed_url_;
  copy->has_post_data_ = has_post_data_;
  copy->post_id_ = post_id_;
  copy->restore_type_ = restore_type_;
  copy->original_request_url_ = original_request_url_;
  copy->is_overriding_user_agent_ = is_overriding_user_agent_;
  copy->timestamp_ = timestamp_;
  copy->http_status_code_ = http_status_code_;
  // ResetForCommit: browser_initiated_post_data_
  copy->screenshot_ = screenshot_;
  copy->extra_headers_ = extra_headers_;
  // ResetForCommit: source_site_instance_
  copy->base_url_for_data_url_ = base_url_for_data_url_;
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: transferred_global_request_id_
  // ResetForCommit: should_replace_entry_
  copy->redirect_chain_ = redirect_chain_;
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  // ResetForCommit: intent_received_timestamp_
  copy->extra_data_ = extra_data_;

  return copy.Pass();
}

CommonNavigationParams NavigationEntryImpl::ConstructCommonNavigationParams(
    const GURL& dest_url,
    const Referrer& dest_referrer,
    const FrameNavigationEntry& frame_entry,
    FrameMsg_Navigate_Type::Value navigation_type,
    LoFiState lofi_state,
    const base::TimeTicks& navigation_start) const {
  FrameMsg_UILoadMetricsReportType::Value report_type =
      FrameMsg_UILoadMetricsReportType::NO_REPORT;
  base::TimeTicks ui_timestamp = base::TimeTicks();
#if defined(OS_ANDROID)
  if (!intent_received_timestamp().is_null())
    report_type = FrameMsg_UILoadMetricsReportType::REPORT_INTENT;
  ui_timestamp = intent_received_timestamp();
#endif

  return CommonNavigationParams(
      dest_url, dest_referrer, GetTransitionType(), navigation_type,
      !IsViewSourceMode(), should_replace_entry(), ui_timestamp, report_type,
      GetBaseURLForDataURL(), GetHistoryURLForDataURL(), lofi_state,
      navigation_start);
}

StartNavigationParams NavigationEntryImpl::ConstructStartNavigationParams()
    const {
  std::vector<unsigned char> browser_initiated_post_data;
  if (GetBrowserInitiatedPostData()) {
    browser_initiated_post_data.assign(
        GetBrowserInitiatedPostData()->front(),
        GetBrowserInitiatedPostData()->front() +
            GetBrowserInitiatedPostData()->size());
  }

  return StartNavigationParams(GetHasPostData(), extra_headers(),
                               browser_initiated_post_data,
#if defined(OS_ANDROID)
                               has_user_gesture(),
#endif
                               transferred_global_request_id().child_id,
                               transferred_global_request_id().request_id);
}

RequestNavigationParams NavigationEntryImpl::ConstructRequestNavigationParams(
    const FrameNavigationEntry& frame_entry,
    bool is_same_document_history_load,
    bool has_committed_real_load,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length) const {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  std::vector<GURL> redirects;
  if (ui::PageTransitionIsNewNavigation(GetTransitionType())) {
    redirects = GetRedirectChain();
  }

  int pending_offset_to_send = pending_history_list_offset;
  int current_offset_to_send = current_history_list_offset;
  int current_length_to_send = current_history_list_length;
  if (should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    pending_offset_to_send = -1;
    current_offset_to_send = -1;
    current_length_to_send = 0;
  }
  return RequestNavigationParams(
      GetIsOverridingUserAgent(), redirects, GetCanLoadLocalResources(),
      base::Time::Now(), frame_entry.page_state(), GetPageID(), GetUniqueID(),
      is_same_document_history_load, has_committed_real_load,
      intended_as_new_entry, pending_offset_to_send, current_offset_to_send,
      current_length_to_send, should_clear_history_list());
}

void NavigationEntryImpl::ResetForCommit() {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest once
  // PlzNavigate is enabled.
  SetBrowserInitiatedPostData(nullptr);
  set_source_site_instance(nullptr);
  set_is_renderer_initiated(false);
  set_transferred_global_request_id(GlobalRequestID());
  set_should_replace_entry(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(-1);

#if defined(OS_ANDROID)
  // Reset the time stamp so that the metrics are not reported if this entry is
  // loaded again in the future.
  set_intent_received_timestamp(base::TimeTicks());
#endif
}

void NavigationEntryImpl::AddOrUpdateFrameEntry(
    FrameTreeNode* frame_tree_node,
    const std::string& frame_unique_name,
    int64 item_sequence_number,
    int64 document_sequence_number,
    SiteInstanceImpl* site_instance,
    const GURL& url,
    const Referrer& referrer,
    const PageState& page_state) {
  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  DCHECK(frame_tree_node->parent());
  NavigationEntryImpl::TreeNode* parent_node =
      FindFrameEntry(frame_tree_node->parent());
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  int frame_tree_node_id = frame_tree_node->frame_tree_node_id();
  for (TreeNode* child : parent_node->children) {
    if (child->frame_entry->frame_tree_node_id() == frame_tree_node_id) {
      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(frame_unique_name, item_sequence_number,
                                      document_sequence_number, site_instance,
                                      url, referrer, page_state);
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  FrameNavigationEntry* frame_entry = new FrameNavigationEntry(
      frame_tree_node_id, frame_unique_name, item_sequence_number,
      document_sequence_number, site_instance, url, referrer);
  frame_entry->set_page_state(page_state);
  parent_node->children.push_back(
      new NavigationEntryImpl::TreeNode(frame_entry));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = FindFrameEntry(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

void NavigationEntryImpl::SetScreenshotPNGData(
    scoped_refptr<base::RefCountedBytes> png_data) {
  screenshot_ = png_data;
  if (screenshot_.get())
    UMA_HISTOGRAM_MEMORY_KB("Overscroll.ScreenshotSize", screenshot_->size());
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() const {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

NavigationEntryImpl::TreeNode* NavigationEntryImpl::FindFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* node = nullptr;
  std::queue<NavigationEntryImpl::TreeNode*> work_queue;
  work_queue.push(root_node());
  while (!work_queue.empty()) {
    node = work_queue.front();
    work_queue.pop();
    if (node->MatchesFrame(frame_tree_node)) {
      // Only the root TreeNode should have a FTN ID of -1.
      DCHECK(node->frame_entry->frame_tree_node_id() != -1 ||
             node == root_node());
      return node;
    }
    // Enqueue any children and keep looking.
    for (auto& child : node->children)
      work_queue.push(child);
  }
  return nullptr;
}

}  // namespace content
