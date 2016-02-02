// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_item_impl.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/text_elider.h"

namespace {

// Returns a new unique ID for use in NavigationItem during construction.  The
// returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

namespace web {

// static
scoped_ptr<NavigationItem> NavigationItem::Create() {
  return scoped_ptr<NavigationItem>(new NavigationItemImpl());
}

NavigationItemImpl::NavigationItemImpl()
    : unique_id_(GetUniqueIDInConstructor()),
      transition_type_(ui::PAGE_TRANSITION_LINK),
      is_overriding_user_agent_(false),
      is_created_from_push_state_(false),
      should_skip_resubmit_data_confirmation_(false),
      is_renderer_initiated_(false),
      is_unsafe_(false),
      facade_delegate_(nullptr) {
}

NavigationItemImpl::~NavigationItemImpl() {
}

NavigationItemImpl::NavigationItemImpl(const NavigationItemImpl& item)
    : unique_id_(item.unique_id_),
      url_(item.url_),
      referrer_(item.referrer_),
      virtual_url_(item.virtual_url_),
      title_(item.title_),
      page_display_state_(item.page_display_state_),
      transition_type_(item.transition_type_),
      favicon_(item.favicon_),
      ssl_(item.ssl_),
      timestamp_(item.timestamp_),
      is_overriding_user_agent_(item.is_overriding_user_agent_),
      http_request_headers_([item.http_request_headers_ copy]),
      serialized_state_object_([item.serialized_state_object_ copy]),
      is_created_from_push_state_(item.is_created_from_push_state_),
      should_skip_resubmit_data_confirmation_(
          item.should_skip_resubmit_data_confirmation_),
      post_data_([item.post_data_ copy]),
      is_renderer_initiated_(item.is_renderer_initiated_),
      is_unsafe_(item.is_unsafe_),
      cached_display_title_(item.cached_display_title_),
      facade_delegate_(nullptr) {
}

void NavigationItemImpl::SetFacadeDelegate(
    scoped_ptr<NavigationItemFacadeDelegate> facade_delegate) {
  facade_delegate_ = std::move(facade_delegate);
}

NavigationItemFacadeDelegate* NavigationItemImpl::GetFacadeDelegate() const {
  return facade_delegate_.get();
}

int NavigationItemImpl::GetUniqueID() const {
  return unique_id_;
}

void NavigationItemImpl::SetURL(const GURL& url) {
  url_ = url;
  cached_display_title_.clear();
}

const GURL& NavigationItemImpl::GetURL() const {
  return url_;
}

void NavigationItemImpl::SetReferrer(const web::Referrer& referrer) {
  referrer_ = referrer;
}

const web::Referrer& NavigationItemImpl::GetReferrer() const {
  return referrer_;
}

void NavigationItemImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == url_) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationItemImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? url_ : virtual_url_;
}

void NavigationItemImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationItemImpl::GetTitle() const {
  return title_;
}

void NavigationItemImpl::SetPageDisplayState(
    const web::PageDisplayState& display_state) {
  page_display_state_ = display_state;
}

const PageDisplayState& NavigationItemImpl::GetPageDisplayState() const {
  return page_display_state_;
}

const base::string16& NavigationItemImpl::GetTitleForDisplay(
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
  } else if (!url_.is_empty()) {
    title = url_formatter::FormatUrl(url_, languages);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (url_.SchemeIsFile()) {
    base::string16::size_type slashpos = title.rfind('/');
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);
  }

  const size_t kMaxTitleChars = 4 * 1024;
  gfx::ElideString(title, kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

void NavigationItemImpl::SetTransitionType(ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationItemImpl::GetTransitionType() const {
  return transition_type_;
}

const FaviconStatus& NavigationItemImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationItemImpl::GetFavicon() {
  return favicon_;
}

const SSLStatus& NavigationItemImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationItemImpl::GetSSL() {
  return ssl_;
}

void NavigationItemImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationItemImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationItemImpl::SetUnsafe(bool is_unsafe) {
  is_unsafe_ = is_unsafe;
}

bool NavigationItemImpl::IsUnsafe() const {
  return is_unsafe_;
}

void NavigationItemImpl::SetIsOverridingUserAgent(
    bool is_overriding_user_agent) {
  is_overriding_user_agent_ = is_overriding_user_agent;
}

bool NavigationItemImpl::IsOverridingUserAgent() const {
  return is_overriding_user_agent_;
}

bool NavigationItemImpl::HasPostData() const {
  return post_data_.get() != nil;
}

NSDictionary* NavigationItemImpl::GetHttpRequestHeaders() const {
  return [[http_request_headers_ copy] autorelease];
}

void NavigationItemImpl::AddHttpRequestHeaders(
    NSDictionary* additional_headers) {
  if (!additional_headers)
    return;

  if (http_request_headers_)
    [http_request_headers_ addEntriesFromDictionary:additional_headers];
  else
    http_request_headers_.reset([additional_headers mutableCopy]);
}

void NavigationItemImpl::SetSerializedStateObject(
    NSString* serialized_state_object) {
  serialized_state_object_.reset([serialized_state_object retain]);
}

NSString* NavigationItemImpl::GetSerializedStateObject() const {
  return serialized_state_object_.get();
}

void NavigationItemImpl::SetIsCreatedFromPushState(bool push_state) {
  is_created_from_push_state_ = push_state;
}

bool NavigationItemImpl::IsCreatedFromPushState() const {
  return is_created_from_push_state_;
}

void NavigationItemImpl::SetShouldSkipResubmitDataConfirmation(bool skip) {
  should_skip_resubmit_data_confirmation_ = skip;
}

bool NavigationItemImpl::ShouldSkipResubmitDataConfirmation() const {
  return should_skip_resubmit_data_confirmation_;
}

void NavigationItemImpl::SetPostData(NSData* post_data) {
  post_data_.reset([post_data retain]);
}

NSData* NavigationItemImpl::GetPostData() const {
  return post_data_.get();
}

void NavigationItemImpl::RemoveHttpRequestHeaderForKey(NSString* key) {
  DCHECK(key);
  [http_request_headers_ removeObjectForKey:key];
  if (![http_request_headers_ count])
    http_request_headers_.reset();
}

void NavigationItemImpl::ResetHttpRequestHeaders() {
  http_request_headers_.reset();
}

void NavigationItemImpl::ResetForCommit() {
  // Any state that only matters when a navigation item is pending should be
  // cleared here.
  set_is_renderer_initiated(false);
}

}  // namespace web
