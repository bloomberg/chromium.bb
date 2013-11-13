// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/imported_module_verification.h"

#include <set>
#include <utility>
#include <vector>
#include "chrome/browser/install_verification/win/module_ids.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ImportedModuleVerificationTest : public ModuleVerificationTest {};

TEST_F(ImportedModuleVerificationTest, TestCase) {
// Note that module verification is temporarily disabled for 64-bit builds.
// See crbug.com/316274.
#if !defined(_WIN64)
  std::set<ModuleInfo> loaded_modules;
  ModuleIDs empty_modules_of_interest;
  ModuleIDs non_matching_modules_of_interest;
  ModuleIDs matching_modules_of_interest;

  // This set of IDs will match because user32.dll imports kernel32.dll.
  matching_modules_of_interest.insert(
      std::make_pair(CalculateModuleNameDigest(L"fancy_pants.dll"), 999u));
  matching_modules_of_interest.insert(
      std::make_pair(CalculateModuleNameDigest(L"kernel32.dll"), 666u));
  matching_modules_of_interest.insert(
      std::make_pair(CalculateModuleNameDigest(L"unit_tests.exe"), 777u));
  matching_modules_of_interest.insert(
      std::make_pair(CalculateModuleNameDigest(L"user32.dll"), 888u));

  non_matching_modules_of_interest.insert(
      std::make_pair(CalculateModuleNameDigest(L"fancy_pants.dll"), 999u));

  // With empty loaded_modules, nothing matches.
  VerifyImportedModules(loaded_modules,
                        empty_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_TRUE(reported_module_ids_.empty());
  VerifyImportedModules(loaded_modules,
                        non_matching_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_TRUE(reported_module_ids_.empty());
  VerifyImportedModules(loaded_modules,
                        matching_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_TRUE(reported_module_ids_.empty());

  // With populated loaded_modules, only the 'matching' module data gives a
  // match.
  ASSERT_TRUE(GetLoadedModuleInfoSet(&loaded_modules));
  VerifyImportedModules(loaded_modules,
                        empty_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_TRUE(reported_module_ids_.empty());
  VerifyImportedModules(loaded_modules,
                        non_matching_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_TRUE(reported_module_ids_.empty());
  VerifyImportedModules(loaded_modules,
                        matching_modules_of_interest,
                        &ImportedModuleVerificationTest::ReportModule);
  ASSERT_EQ(1u, reported_module_ids_.size());
  ASSERT_EQ(666u, *reported_module_ids_.begin());
#endif
}
