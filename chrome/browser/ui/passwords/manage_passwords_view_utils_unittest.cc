// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace {

const struct {
  const char* const user_visible_url;
  const char* const form_origin_url;
  bool is_smartlock_branding_enabled;
  PasswordTittleType bubble_type;
  const char* const expected_domain_placeholder;  // "this site" or domain name
  size_t expected_link_range_start;
  size_t expected_link_range_end;
} kDomainsTestCases[] = {
    // Same domains.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     false, PasswordTittleType::SAVE_PASSWORD, "this site", 0, 0},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     true, PasswordTittleType::SAVE_PASSWORD, "this site", 12, 29},

    // Different subdomains.
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3", false,
     PasswordTittleType::SAVE_PASSWORD, "this site", 0, 0},
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3", true,
     PasswordTittleType::SAVE_PASSWORD, "this site", 12, 29},

    // Different domains.
    {"https://another.org", "https://example.com:/login#form?value=3", false,
     PasswordTittleType::SAVE_PASSWORD, "https://example.com", 0, 0},
    {"https://another.org", "https://example.com/login#form?value=3", true,
     PasswordTittleType::SAVE_PASSWORD, "https://example.com", 12, 29},

    // Different domains and password form origin url with
    // default port for the scheme.
    {"https://another.org", "https://example.com:443/login#form?value=3", false,
     PasswordTittleType::SAVE_PASSWORD, "https://example.com", 0, 0},
    {"https://another.org", "http://example.com:80/login#form?value=3", true,
     PasswordTittleType::SAVE_PASSWORD, "http://example.com", 12, 29},

    // Different domains and password form origin url with
    // non-default port for the scheme.
    {"https://another.org", "https://example.com:8001/login#form?value=3",
     false, PasswordTittleType::SAVE_PASSWORD, "https://example.com:8001", 0,
     0},
    {"https://another.org", "https://example.com:8001/login#form?value=3", true,
     PasswordTittleType::SAVE_PASSWORD, "https://example.com:8001", 12, 29},

    // Update bubble, same domains.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     false, PasswordTittleType::UPDATE_PASSWORD, "this site", 0, 0},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     true, PasswordTittleType::UPDATE_PASSWORD, "this site", 12, 29},

    // Update bubble, different domains.
    {"https://another.org", "http://example.com/login#form?value=3", false,
     PasswordTittleType::UPDATE_PASSWORD, "http://example.com", 0, 0},
    {"https://another.org", "http://example.com/login#form?value=3", true,
     PasswordTittleType::UPDATE_PASSWORD, "http://example.com", 12, 29},

    // Same domains, federated credential.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     false, PasswordTittleType::SAVE_ACCOUNT, "this site", 0, 0},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     true, PasswordTittleType::SAVE_ACCOUNT, "this site", 12, 29},

    // Different subdomains, federated credential.
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3", false,
     PasswordTittleType::SAVE_ACCOUNT, "this site", 0, 0},
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3", true,
     PasswordTittleType::SAVE_ACCOUNT, "this site", 12, 29}};

}  // namespace

// Test for GetSavePasswordDialogTitleTextAndLinkRange().
TEST(ManagePasswordsViewUtilTest, GetSavePasswordDialogTitleTextAndLinkRange) {
  for (size_t i = 0; i < arraysize(kDomainsTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "user_visible_url = "
                                    << kDomainsTestCases[i].user_visible_url
                                    << ", form_origin_url = "
                                    << kDomainsTestCases[i].form_origin_url);

    base::string16 title;
    gfx::Range title_link_range;
    GetSavePasswordDialogTitleTextAndLinkRange(
        GURL(kDomainsTestCases[i].user_visible_url),
        GURL(kDomainsTestCases[i].form_origin_url),
        kDomainsTestCases[i].is_smartlock_branding_enabled,
        kDomainsTestCases[i].bubble_type, &title, &title_link_range);

    // Verify against expectations.
    base::string16 domain =
        base::ASCIIToUTF16(kDomainsTestCases[i].expected_domain_placeholder);
    EXPECT_TRUE(title.find(domain) != base::string16::npos);
    EXPECT_EQ(kDomainsTestCases[i].expected_link_range_start,
              title_link_range.start());
    EXPECT_EQ(kDomainsTestCases[i].expected_link_range_end,
              title_link_range.end());
    if (kDomainsTestCases[i].bubble_type ==
        PasswordTittleType::UPDATE_PASSWORD) {
      EXPECT_TRUE(title.find(base::ASCIIToUTF16("update")) !=
                  base::string16::npos);
    } else {
      EXPECT_TRUE(title.find(base::ASCIIToUTF16("save")) !=
                  base::string16::npos);
    }
  }
}

TEST(ManagePasswordsViewUtilTest, GetManagePasswordsDialogTitleText) {
  for (size_t i = 0; i < arraysize(kDomainsTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "user_visible_url = "
                                    << kDomainsTestCases[i].user_visible_url
                                    << ", password_origin_url = "
                                    << kDomainsTestCases[i].form_origin_url);

    base::string16 title;
    gfx::Range title_link_range;
    GetManagePasswordsDialogTitleText(
        GURL(kDomainsTestCases[i].user_visible_url),
        GURL(kDomainsTestCases[i].form_origin_url), &title);

    // Verify against expectations.
    base::string16 domain =
        base::ASCIIToUTF16(kDomainsTestCases[i].expected_domain_placeholder);
    EXPECT_TRUE(title.find(domain) != base::string16::npos);
  }
}

// The parameter is |many_accounts| passed to
// GetAccountChooserDialogTitleTextAndLinkRange
class AccountChooserDialogTitleTest : public ::testing::TestWithParam<bool> {
};

TEST_P(AccountChooserDialogTitleTest,
       GetAccountChooserDialogTitleTextAndLinkRangeSmartLockUsers) {
  base::string16 branding =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK);
  base::string16 title;
  gfx::Range title_link_range;
  GetAccountChooserDialogTitleTextAndLinkRange(
      true /* is_smartlock_branding_enabled */,
      GetParam(),
      &title, &title_link_range);

  // Check that branding string is a part of a title.
  EXPECT_LT(title.find(branding, 0), title.size());
  EXPECT_GT(title.find(branding, 0), 0U);
  // Check that link range is not empty.
  EXPECT_NE(0U, title_link_range.start());
  EXPECT_NE(0U, title_link_range.end());
}

TEST_P(AccountChooserDialogTitleTest,
       GetAccountChooserDialogTitleTextAndLinkRangeNonSmartLockUsers) {
  base::string16 branding =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK);
  base::string16 title;
  gfx::Range title_link_range;
  GetAccountChooserDialogTitleTextAndLinkRange(
      false /* is_smartlock_branding_enabled */,
      GetParam(),
      &title, &title_link_range);
  EXPECT_GE(title.find(branding, 0), title.size());
  EXPECT_EQ(0U, title_link_range.start());
  EXPECT_EQ(0U, title_link_range.end());
}

INSTANTIATE_TEST_CASE_P(, AccountChooserDialogTitleTest,
                        ::testing::Bool());
