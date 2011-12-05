// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/navigation_entry.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/site_instance.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "ui/base/text/text_elider.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

NavigationEntry::SSLStatus::SSLStatus()
    : security_style_(content::SECURITY_STYLE_UNKNOWN),
      cert_id_(0),
      cert_status_(0),
      security_bits_(-1),
      connection_status_(0),
      content_status_(NORMAL_CONTENT) {
}

NavigationEntry::FaviconStatus::FaviconStatus() : valid_(false) {
  bitmap_ = *content::GetContentClient()->browser()->GetDefaultFavicon();
}


NavigationEntry::NavigationEntry()
    : unique_id_(GetUniqueID()),
      site_instance_(NULL),
      page_type_(content::PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      page_id_(-1),
      transition_type_(content::PAGE_TRANSITION_LINK),
      has_post_data_(false),
      restore_type_(RESTORE_NONE),
      is_renderer_initiated_(false) {
}

NavigationEntry::NavigationEntry(SiteInstance* instance,
                                 int page_id,
                                 const GURL& url,
                                 const content::Referrer& referrer,
                                 const string16& title,
                                 content::PageTransition transition_type,
                                 bool is_renderer_initiated)
    : unique_id_(GetUniqueID()),
      site_instance_(instance),
      page_type_(content::PAGE_TYPE_NORMAL),
      url_(url),
      referrer_(referrer),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      restore_type_(RESTORE_NONE),
      is_renderer_initiated_(is_renderer_initiated) {
}

NavigationEntry::~NavigationEntry() {
}

void NavigationEntry::set_site_instance(SiteInstance* site_instance) {
  site_instance_ = site_instance;
}

const string16& NavigationEntry::GetTitleForDisplay(
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

  ui::ElideString(title, content::kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntry::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(chrome::kViewSourceScheme);
}
