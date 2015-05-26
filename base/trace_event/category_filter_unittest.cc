// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/category_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

// Test the category filter.
TEST(CategoryFilterTest, CategoryFilter) {
  // Using the default filter.
  CategoryFilter default_cf = CategoryFilter(
      CategoryFilter::kDefaultCategoryFilterString);
  std::string category_filter_str = default_cf.ToString();
  EXPECT_STREQ("-*Debug,-*Test", category_filter_str.c_str());
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("not-excluded-category"));
  EXPECT_FALSE(
      default_cf.IsCategoryGroupEnabled("disabled-by-default-category"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  // Make sure that upon an empty string, we fall back to the default filter.
  default_cf = CategoryFilter();
  category_filter_str = default_cf.ToString();
  EXPECT_STREQ("-*Debug,-*Test", category_filter_str.c_str());
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("not-excluded-category"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  // Using an arbitrary non-empty filter.
  CategoryFilter cf("included,-excluded,inc_pattern*,-exc_pattern*");
  category_filter_str = cf.ToString();
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*",
               category_filter_str.c_str());
  EXPECT_TRUE(cf.IsCategoryGroupEnabled("included"));
  EXPECT_TRUE(cf.IsCategoryGroupEnabled("inc_pattern_category"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("exc_pattern_category"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("excluded"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("not-excluded-nor-included"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  cf.Merge(default_cf);
  category_filter_str = cf.ToString();
  EXPECT_STREQ("-excluded,-exc_pattern*,-*Debug,-*Test",
                category_filter_str.c_str());
  cf.Clear();

  CategoryFilter reconstructed_cf(category_filter_str);
  category_filter_str = reconstructed_cf.ToString();
  EXPECT_STREQ("-excluded,-exc_pattern*,-*Debug,-*Test",
               category_filter_str.c_str());

  // One included category.
  CategoryFilter one_inc_cf("only_inc_cat");
  category_filter_str = one_inc_cf.ToString();
  EXPECT_STREQ("only_inc_cat", category_filter_str.c_str());

  // One excluded category.
  CategoryFilter one_exc_cf("-only_exc_cat");
  category_filter_str = one_exc_cf.ToString();
  EXPECT_STREQ("-only_exc_cat", category_filter_str.c_str());

  // Enabling a disabled- category does not require all categories to be traced
  // to be included.
  CategoryFilter disabled_cat("disabled-by-default-cc,-excluded");
  EXPECT_STREQ("disabled-by-default-cc,-excluded",
               disabled_cat.ToString().c_str());
  EXPECT_TRUE(disabled_cat.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(disabled_cat.IsCategoryGroupEnabled("some_other_group"));
  EXPECT_FALSE(disabled_cat.IsCategoryGroupEnabled("excluded"));

  // Enabled a disabled- category and also including makes all categories to
  // be traced require including.
  CategoryFilter disabled_inc_cat("disabled-by-default-cc,included");
  EXPECT_STREQ("included,disabled-by-default-cc",
               disabled_inc_cat.ToString().c_str());
  EXPECT_TRUE(
      disabled_inc_cat.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(disabled_inc_cat.IsCategoryGroupEnabled("included"));
  EXPECT_FALSE(disabled_inc_cat.IsCategoryGroupEnabled("other_included"));

  // Test that IsEmptyOrContainsLeadingOrTrailingWhitespace actually catches
  // categories that are explicitly forbiden.
  // This method is called in a DCHECK to assert that we don't have these types
  // of strings as categories.
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category"));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category"));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category   "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category   "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      ""));
  EXPECT_FALSE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "good_category"));
}

}  // namespace trace_event
}  // namespace base
