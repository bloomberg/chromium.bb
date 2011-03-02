// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlsecurity.h>  // NOLINT
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/win/registry.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {
wchar_t test_root[] = L"SOFTWARE\\TmpTmp";
}

class DeleteRegKeyWorkItemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(test_root);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, test_root,
                                        KEY_READ));
  }

  virtual void TearDown() {
    logging::CloseLogFile();
    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(test_root));
  }
};

// Test that deleting a key that doesn't exist succeeds, and that rollback does
// nothing.
TEST_F(DeleteRegKeyWorkItemTest, TestNoKey) {
  RegKey key;
  std::wstring key_name(std::wstring(test_root) + L"\\NoKeyHere");
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  scoped_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(HKEY_CURRENT_USER, key_name));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  item->Rollback();
  item.reset();
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
}

// Test that deleting a subkey of a key that doesn't exist succeeds, and that
// rollback does nothing.
TEST_F(DeleteRegKeyWorkItemTest, TestNoSubkey) {
  RegKey key;
  std::wstring key_name(std::wstring(test_root) + L"\\NoKeyHere\\OrHere");
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  scoped_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(HKEY_CURRENT_USER, key_name));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  item->Rollback();
  item.reset();
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
}

// Test that deleting an empty key succeeds, and that rollback brings it back.
TEST_F(DeleteRegKeyWorkItemTest, TestEmptyKey) {
  RegKey key;
  std::wstring key_name(std::wstring(test_root) + L"\\EmptyKey");
  EXPECT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, key_name.c_str(),
                                      KEY_WRITE));
  key.Close();
  scoped_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(HKEY_CURRENT_USER, key_name));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  item->Rollback();
  item.reset();
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
}

// Test that deleting a key with subkeys and values succeeds, and that rollback
// brings them all back.
TEST_F(DeleteRegKeyWorkItemTest, TestNonEmptyKey) {
  RegKey key;
  std::wstring key_name(std::wstring(test_root) + L"\\NonEmptyKey");
  EXPECT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, key_name.c_str(),
                                      KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(NULL, key_name.c_str()));
  EXPECT_EQ(ERROR_SUCCESS, key.CreateKey(L"Subkey", KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"SomeValue", 1U));
  key.Close();
  scoped_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(HKEY_CURRENT_USER, key_name));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  item->Rollback();
  item.reset();
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_READ));
  std::wstring str_value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(NULL, &str_value));
  EXPECT_EQ(key_name, str_value);
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey", KEY_READ));
  DWORD dw_value = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"SomeValue", &dw_value));
  EXPECT_EQ(1U, dw_value);
}

// Test that deleting a key with subkeys we can't delete fails, and that
// everything is there after rollback.
TEST_F(DeleteRegKeyWorkItemTest, TestUndeletableKey) {
  RegKey key;
  std::wstring key_name(std::wstring(test_root) + L"\\UndeletableKey");
  EXPECT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, key_name.c_str(),
                                      KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(NULL, key_name.c_str()));
  DWORD dw_value = 1;
  RegKey subkey;
  RegKey subkey2;
  EXPECT_EQ(ERROR_SUCCESS, subkey.Create(key.Handle(), L"Subkey",
                                         KEY_WRITE | WRITE_DAC));
  EXPECT_EQ(ERROR_SUCCESS, subkey.WriteValue(L"SomeValue", 1U));
  EXPECT_EQ(ERROR_SUCCESS, subkey2.Create(subkey.Handle(), L"Subkey2",
                                          KEY_WRITE | WRITE_DAC));
  EXPECT_EQ(ERROR_SUCCESS, subkey2.WriteValue(L"", 2U));
  CSecurityDesc sec_desc;
  sec_desc.FromString(L"D:PAI(A;OICI;KR;;;BU)");  // builtin users read
  EXPECT_EQ(ERROR_SUCCESS,
            RegSetKeySecurity(subkey.Handle(), DACL_SECURITY_INFORMATION,
                              const_cast<SECURITY_DESCRIPTOR*>(
                                  sec_desc.GetPSECURITY_DESCRIPTOR())));
  sec_desc.FromString(L"D:PAI(A;OICI;KA;;;BU)");  // builtin users all access
  EXPECT_EQ(ERROR_SUCCESS,
            RegSetKeySecurity(subkey2.Handle(), DACL_SECURITY_INFORMATION,
                              const_cast<SECURITY_DESCRIPTOR*>(
                                  sec_desc.GetPSECURITY_DESCRIPTOR())));
  subkey2.Close();
  subkey.Close();
  key.Close();
  scoped_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(HKEY_CURRENT_USER, key_name));
  EXPECT_FALSE(item->Do());
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_QUERY_VALUE));
  item->Rollback();
  item.reset();
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                    KEY_QUERY_VALUE));
  std::wstring str_value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(NULL, &str_value));
  EXPECT_EQ(key_name, str_value);
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey", KEY_READ | WRITE_DAC));
  dw_value = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"SomeValue", &dw_value));
  EXPECT_EQ(1U, dw_value);
  // Give users all access to the subkey so it can be deleted.
  EXPECT_EQ(ERROR_SUCCESS,
      RegSetKeySecurity(key.Handle(), DACL_SECURITY_INFORMATION,
                        const_cast<SECURITY_DESCRIPTOR*>(
                            sec_desc.GetPSECURITY_DESCRIPTOR())));
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey2", KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"", &dw_value));
  EXPECT_EQ(2U, dw_value);
}
