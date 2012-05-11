// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/public/common/ssl_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(NULL) {
  }

  virtual void SetUp() {
    entry1_.reset(new NavigationEntryImpl);

    instance_ = static_cast<SiteInstanceImpl*>(SiteInstance::Create(NULL));
    entry2_.reset(new NavigationEntryImpl(
          instance_, 3,
          GURL("test:url"),
          Referrer(GURL("from"), WebKit::WebReferrerPolicyDefault),
          ASCIIToUTF16("title"),
          PAGE_TRANSITION_TYPED,
          false));
  }

  virtual void TearDown() {
  }

 protected:
  scoped_ptr<NavigationEntryImpl> entry1_;
  scoped_ptr<NavigationEntryImpl> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  SiteInstanceImpl* instance_;
};

// Test unique ID accessors
TEST_F(NavigationEntryTest, NavigationEntryUniqueIDs) {
  // Two entries should have different IDs by default
  EXPECT_NE(entry1_.get()->GetUniqueID(), entry2_.get()->GetUniqueID());

  // Can set an entry to have the same ID as another
  entry2_.get()->set_unique_id(entry1_.get()->GetUniqueID());
  EXPECT_EQ(entry1_.get()->GetUniqueID(), entry2_.get()->GetUniqueID());
}

// Test URL accessors
TEST_F(NavigationEntryTest, NavigationEntryURLs) {
  // Start with no virtual_url (even if a url is set)
  EXPECT_FALSE(entry1_.get()->has_virtual_url());
  EXPECT_FALSE(entry2_.get()->has_virtual_url());

  EXPECT_EQ(GURL(), entry1_.get()->GetURL());
  EXPECT_EQ(GURL(), entry1_.get()->GetVirtualURL());
  EXPECT_TRUE(entry1_.get()->GetTitleForDisplay("").empty());

  // Setting URL affects virtual_url and GetTitleForDisplay
  entry1_.get()->SetURL(GURL("http://www.google.com"));
  EXPECT_EQ(GURL("http://www.google.com"), entry1_.get()->GetURL());
  EXPECT_EQ(GURL("http://www.google.com"), entry1_.get()->GetVirtualURL());
  EXPECT_EQ(ASCIIToUTF16("www.google.com"),
            entry1_.get()->GetTitleForDisplay(""));

  // file:/// URLs should only show the filename.
  entry1_.get()->SetURL(GURL("file:///foo/bar baz.txt"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt"),
            entry1_.get()->GetTitleForDisplay(""));

  // Title affects GetTitleForDisplay
  entry1_.get()->SetTitle(ASCIIToUTF16("Google"));
  EXPECT_EQ(ASCIIToUTF16("Google"), entry1_.get()->GetTitleForDisplay(""));

  // Setting virtual_url doesn't affect URL
  entry2_.get()->SetVirtualURL(GURL("display:url"));
  EXPECT_TRUE(entry2_.get()->has_virtual_url());
  EXPECT_EQ(GURL("test:url"), entry2_.get()->GetURL());
  EXPECT_EQ(GURL("display:url"), entry2_.get()->GetVirtualURL());

  // Having a title set in constructor overrides virtual URL
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->GetTitleForDisplay(""));

  // User typed URL is independent of the others
  EXPECT_EQ(GURL(), entry1_.get()->GetUserTypedURL());
  EXPECT_EQ(GURL(), entry2_.get()->GetUserTypedURL());
  entry2_.get()->set_user_typed_url(GURL("typedurl"));
  EXPECT_EQ(GURL("typedurl"), entry2_.get()->GetUserTypedURL());
}

// Test Favicon inner class construction.
TEST_F(NavigationEntryTest, NavigationEntryFavicons) {
  EXPECT_EQ(GURL(), entry1_.get()->GetFavicon().url);
  EXPECT_FALSE(entry1_.get()->GetFavicon().valid);
}

// Test SSLStatus inner class
TEST_F(NavigationEntryTest, NavigationEntrySSLStatus) {
  // Default (unknown)
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry1_.get()->GetSSL().security_style);
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry2_.get()->GetSSL().security_style);
  EXPECT_EQ(0, entry1_.get()->GetSSL().cert_id);
  EXPECT_EQ(0U, entry1_.get()->GetSSL().cert_status);
  EXPECT_EQ(-1, entry1_.get()->GetSSL().security_bits);
  int content_status = entry1_.get()->GetSSL().content_status;
  EXPECT_FALSE(!!(content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_FALSE(!!(content_status & SSLStatus::RAN_INSECURE_CONTENT));
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_TRUE(entry1_.get()->site_instance() == NULL);
  EXPECT_EQ(instance_, entry2_.get()->site_instance());
  entry1_.get()->set_site_instance(instance_);
  EXPECT_EQ(instance_, entry1_.get()->site_instance());

  // Page type
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry1_.get()->GetPageType());
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry2_.get()->GetPageType());
  entry2_.get()->set_page_type(PAGE_TYPE_INTERSTITIAL);
  EXPECT_EQ(PAGE_TYPE_INTERSTITIAL, entry2_.get()->GetPageType());

  // Referrer
  EXPECT_EQ(GURL(), entry1_.get()->GetReferrer().url);
  EXPECT_EQ(GURL("from"), entry2_.get()->GetReferrer().url);
  entry2_.get()->SetReferrer(
      Referrer(GURL("from2"), WebKit::WebReferrerPolicyDefault));
  EXPECT_EQ(GURL("from2"), entry2_.get()->GetReferrer().url);

  // Title
  EXPECT_EQ(string16(), entry1_.get()->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->GetTitle());
  entry2_.get()->SetTitle(ASCIIToUTF16("title2"));
  EXPECT_EQ(ASCIIToUTF16("title2"), entry2_.get()->GetTitle());

  // State
  EXPECT_EQ(std::string(), entry1_.get()->GetContentState());
  EXPECT_EQ(std::string(), entry2_.get()->GetContentState());
  entry2_.get()->SetContentState("state");
  EXPECT_EQ("state", entry2_.get()->GetContentState());

  // Page ID
  EXPECT_EQ(-1, entry1_.get()->GetPageID());
  EXPECT_EQ(3, entry2_.get()->GetPageID());
  entry2_.get()->SetPageID(2);
  EXPECT_EQ(2, entry2_.get()->GetPageID());

  // Transition type
  EXPECT_EQ(PAGE_TRANSITION_LINK, entry1_.get()->GetTransitionType());
  EXPECT_EQ(PAGE_TRANSITION_TYPED, entry2_.get()->GetTransitionType());
  entry2_.get()->SetTransitionType(PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(PAGE_TRANSITION_RELOAD, entry2_.get()->GetTransitionType());

  // Is renderer initiated
  EXPECT_FALSE(entry1_.get()->is_renderer_initiated());
  EXPECT_FALSE(entry2_.get()->is_renderer_initiated());
  entry2_.get()->set_is_renderer_initiated(true);
  EXPECT_TRUE(entry2_.get()->is_renderer_initiated());

  // Post Data
  EXPECT_FALSE(entry1_.get()->GetHasPostData());
  EXPECT_FALSE(entry2_.get()->GetHasPostData());
  entry2_.get()->SetHasPostData(true);
  EXPECT_TRUE(entry2_.get()->GetHasPostData());

  // Restored
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE, entry1_->restore_type());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE, entry2_->restore_type());
  entry2_->set_restore_type(NavigationEntryImpl::RESTORE_LAST_SESSION);
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION, entry2_->restore_type());

  // Original URL
  EXPECT_EQ(GURL(), entry1_.get()->GetOriginalRequestURL());
  EXPECT_EQ(GURL(), entry2_.get()->GetOriginalRequestURL());
  entry2_.get()->SetOriginalRequestURL(GURL("original_url"));
  EXPECT_EQ(GURL("original_url"), entry2_.get()->GetOriginalRequestURL());

  // User agent override
  EXPECT_FALSE(entry1_.get()->GetIsOverridingUserAgent());
  EXPECT_FALSE(entry2_.get()->GetIsOverridingUserAgent());
  entry2_.get()->SetIsOverridingUserAgent(true);
  EXPECT_TRUE(entry2_.get()->GetIsOverridingUserAgent());
}

}  // namespace content
