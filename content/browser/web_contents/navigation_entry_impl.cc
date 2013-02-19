// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/navigation_entry_impl.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "ui/base/text/text_elider.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

namespace content {

NavigationEntry* NavigationEntry::Create() {
  return new NavigationEntryImpl();
}

NavigationEntry* NavigationEntry::Create(const NavigationEntry& copy) {
  return new NavigationEntryImpl(static_cast<const NavigationEntryImpl&>(copy));
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

NavigationEntryImpl::NavigationEntryImpl()
    : unique_id_(GetUniqueIDInConstructor()),
      site_instance_(NULL),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      page_id_(-1),
      transition_type_(PAGE_TRANSITION_LINK),
      has_post_data_(false),
      post_id_(-1),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      is_renderer_initiated_(false),
      should_replace_entry_(false),
      can_load_local_resources_(false) {
}

NavigationEntryImpl::NavigationEntryImpl(SiteInstanceImpl* instance,
                                         int page_id,
                                         const GURL& url,
                                         const Referrer& referrer,
                                         const string16& title,
                                         PageTransition transition_type,
                                         bool is_renderer_initiated)
    : unique_id_(GetUniqueIDInConstructor()),
      site_instance_(instance),
      page_type_(PAGE_TYPE_NORMAL),
      url_(url),
      referrer_(referrer),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      post_id_(-1),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      can_load_local_resources_(false) {
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
  url_ = url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() const {
  return url_;
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() const {
  return base_url_for_data_url_;
}

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  referrer_ = referrer;
}

const Referrer& NavigationEntryImpl::GetReferrer() const {
  return referrer_;
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == url_) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? url_ : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const string16& NavigationEntryImpl::GetTitle() const {
  return title_;
}

void NavigationEntryImpl::SetContentState(const std::string& state) {
  content_state_ = state;
}

const std::string& NavigationEntryImpl::GetContentState() const {
  return content_state_;
}

void NavigationEntryImpl::SetPageID(int page_id) {
  page_id_ = page_id;
}

int32 NavigationEntryImpl::GetPageID() const {
  return page_id_;
}

void NavigationEntryImpl::set_site_instance(SiteInstanceImpl* site_instance) {
  site_instance_ = site_instance;
}

const string16& NavigationEntryImpl::GetTitleForDisplay(
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
  string16 title;
  if (!virtual_url_.is_empty()) {
    title = net::FormatUrl(virtual_url_, languages);
  } else if (!url_.is_empty()) {
    title = net::FormatUrl(url_, languages);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (url_.SchemeIsFile()) {
    string16::size_type slashpos = title.rfind('/');
    if (slashpos != string16::npos)
      title = title.substr(slashpos + 1);
  }

  ui::ElideString(title, kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(chrome::kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    PageTransition transition_type) {
  transition_type_ = transition_type;
}

PageTransition NavigationEntryImpl::GetTransitionType() const {
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

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() const {
  return can_load_local_resources_;
}

void NavigationEntryImpl::SetFrameToNavigate(const std::string& frame_name) {
  frame_to_navigate_ = frame_name;
}

const std::string& NavigationEntryImpl::GetFrameToNavigate() const {
  return frame_to_navigate_;
}

void NavigationEntryImpl::SetExtraData(const std::string& key,
                                       const string16& data) {
  extra_data_[key] = data;
}

bool NavigationEntryImpl::GetExtraData(const std::string& key,
                                       string16* data) const {
  std::map<std::string, string16>::const_iterator iter = extra_data_.find(key);
  if (iter == extra_data_.end())
    return false;
  *data = iter->second;
  return true;
}

void NavigationEntryImpl::ClearExtraData(const std::string& key) {
  extra_data_.erase(key);
}

void NavigationEntryImpl::SetScreenshotPNGData(
    const std::vector<unsigned char>& png_data) {
  screenshot_ = png_data.empty() ? NULL : new base::RefCountedBytes(png_data);
  if (screenshot_)
    UMA_HISTOGRAM_MEMORY_KB("Overscroll.ScreenshotSize", screenshot_->size());
}

}  // namespace content
