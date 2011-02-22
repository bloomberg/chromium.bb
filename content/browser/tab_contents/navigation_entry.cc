// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/navigation_entry.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "grit/app_resources.h"
#include "net/base/net_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

NavigationEntry::SSLStatus::SSLStatus()
    : security_style_(SECURITY_STYLE_UNKNOWN),
      cert_id_(0),
      cert_status_(0),
      security_bits_(-1),
      connection_status_(0),
      content_status_(NORMAL_CONTENT) {
}

NavigationEntry::FaviconStatus::FaviconStatus() : valid_(false) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  bitmap_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}


NavigationEntry::NavigationEntry()
    : unique_id_(GetUniqueID()),
      site_instance_(NULL),
      page_type_(NORMAL_PAGE),
      update_virtual_url_with_url_(false),
      page_id_(-1),
      transition_type_(PageTransition::LINK),
      has_post_data_(false),
      restore_type_(RESTORE_NONE) {
}

NavigationEntry::NavigationEntry(SiteInstance* instance,
                                 int page_id,
                                 const GURL& url,
                                 const GURL& referrer,
                                 const string16& title,
                                 PageTransition::Type transition_type)
    : unique_id_(GetUniqueID()),
      site_instance_(instance),
      page_type_(NORMAL_PAGE),
      url_(url),
      referrer_(referrer),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      restore_type_(RESTORE_NONE) {
}

NavigationEntry::~NavigationEntry() {
}

void NavigationEntry::set_site_instance(SiteInstance* site_instance) {
  site_instance_ = site_instance;
}

const string16& NavigationEntry::GetTitleForDisplay(
    const std::string& languages) {
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
  ui::ElideString(title, chrome::kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntry::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(chrome::kViewSourceScheme);
}
