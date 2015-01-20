// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/ssl_status.h"
#include "url/gurl.h"

namespace web {

// Implementation of NavigationItem.
class NavigationItemImpl : public web::NavigationItem {
 public:
  // Creates a default NavigationItemImpl.
  NavigationItemImpl();
  ~NavigationItemImpl() override;

  // NavigationItem implementation:
  int GetUniqueID() const override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;
  void SetReferrer(const web::Referrer& referrer) override;
  const web::Referrer& GetReferrer() const override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() const override;
  void SetTitle(const base::string16& title) override;
  const base::string16& GetTitle() const override;
  void SetPageID(int page_id) override;
  int32 GetPageID() const override;
  const base::string16& GetTitleForDisplay(
      const std::string& languages) const override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() const override;
  const FaviconStatus& GetFavicon() const override;
  FaviconStatus& GetFavicon() override;
  const SSLStatus& GetSSL() const override;
  SSLStatus& GetSSL() override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() const override;

 private:
  int unique_id_;
  GURL url_;
  Referrer referrer_;
  GURL virtual_url_;
  base::string16 title_;
  int32 page_id_;
  ui::PageTransition transition_type_;
  FaviconStatus favicon_;
  SSLStatus ssl_;
  base::Time timestamp_;

  // This is a cached version of the result of GetTitleForDisplay. When the URL,
  // virtual URL, or title is set, this should be cleared to force a refresh.
  mutable base::string16 cached_display_title_;

  // Copy and assignment is explicitly allowed for this class.
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
