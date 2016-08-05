// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/google_update_integration.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Copied from chrome/browser/google/google_brand.cc.
const wchar_t* const kOrganicBrands[] = {
    L"CHCA", L"CHCB", L"CHCG", L"CHCH", L"CHCI", L"CHCJ", L"CHCK", L"CHCL",
    L"CHFO", L"CHFT", L"CHHS", L"CHHM", L"CHMA", L"CHMB", L"CHME", L"CHMF",
    L"CHMG", L"CHMH", L"CHMI", L"CHMQ", L"CHMV", L"CHNB", L"CHNC", L"CHNG",
    L"CHNH", L"CHNI", L"CHOA", L"CHOB", L"CHOC", L"CHON", L"CHOO", L"CHOP",
    L"CHOQ", L"CHOR", L"CHOS", L"CHOT", L"CHOU", L"CHOX", L"CHOY", L"CHOZ",
    L"CHPD", L"CHPE", L"CHPF", L"CHPG", L"ECBA", L"ECBB", L"ECDA", L"ECDB",
    L"ECSA", L"ECSB", L"ECVA", L"ECVB", L"ECWA", L"ECWB", L"ECWC", L"ECWD",
    L"ECWE", L"ECWF", L"EUBB", L"EUBC", L"GGLA", L"GGLS",

    // EUB*
    L"EUBQ",

    // EUC*
    L"EUCQ",

    // GGR*
    L"GGRQ",
};

}  // namespace

// Test that all expected brands are considered organic.
class IsOrganicTest : public ::testing::TestWithParam<const wchar_t*> {};

TEST_P(IsOrganicTest, IsOrganicBrand) {
  EXPECT_TRUE(IsOrganic(GetParam()));
}

INSTANTIATE_TEST_CASE_P(OrganicBrands,
                        IsOrganicTest,
                        ::testing::ValuesIn(kOrganicBrands));

// Test that a smattering of non-organic brands are not considered organic.
class IsNotOrganicTest : public ::testing::TestWithParam<const wchar_t*> {};

TEST_P(IsNotOrganicTest, IsNotOrganicBrand) {
  EXPECT_FALSE(IsOrganic(GetParam()));
}

INSTANTIATE_TEST_CASE_P(NonOrganicBrands,
                        IsNotOrganicTest,
                        ::testing::Values(L"AOHY", L"YAKS", L""));
