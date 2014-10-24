// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_item_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_util.h"
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
      page_id_(-1),
      transition_type_(ui::PAGE_TRANSITION_LINK) {
}

NavigationItemImpl::~NavigationItemImpl() {
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

void NavigationItemImpl::SetPageID(int page_id) {
  page_id_ = page_id;
}

int32 NavigationItemImpl::GetPageID() const {
  return page_id_;
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
    title = net::FormatUrl(virtual_url_, languages);
  } else if (!url_.is_empty()) {
    title = net::FormatUrl(url_, languages);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (url_.SchemeIsFile()) {
    base::string16::size_type slashpos = title.rfind('/');
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);
  }

  const int kMaxTitleChars = 4 * 1024;
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

}  // namespace web
