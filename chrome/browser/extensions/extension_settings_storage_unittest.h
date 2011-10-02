// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_UNITTEST_H_
#pragma once

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "content/browser/browser_thread.h"

// Parameter type for the value-parameterized tests.
typedef ExtensionSettingsStorage* (*ExtensionSettingsStorageTestParam)(
    const FilePath& file_path, const std::string& extension_id);

// Test fixture for ExtensionSettingsStorage tests.  Tests are defined in
// extension_settings_storage_unittest.cc with configurations for both cached
// and non-cached leveldb storage, and cached no-op storage.
class ExtensionSettingsStorageTest
  : public testing::TestWithParam<ExtensionSettingsStorageTestParam> {
 public:
  ExtensionSettingsStorageTest();
  virtual ~ExtensionSettingsStorageTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<ExtensionSettingsStorage> storage_;

  std::string key1_;
  std::string key2_;
  std::string key3_;

  scoped_ptr<Value> val1_;
  scoped_ptr<Value> val2_;
  scoped_ptr<Value> val3_;

  std::vector<std::string> empty_list_;
  std::vector<std::string> list1_;
  std::vector<std::string> list2_;
  std::vector<std::string> list3_;
  std::vector<std::string> list12_;
  std::vector<std::string> list13_;
  std::vector<std::string> list123_;

  std::set<std::string> empty_set_;
  std::set<std::string> set1_;
  std::set<std::string> set2_;
  std::set<std::string> set3_;
  std::set<std::string> set12_;
  std::set<std::string> set13_;
  std::set<std::string> set123_;

  scoped_ptr<DictionaryValue> empty_dict_;
  scoped_ptr<DictionaryValue> dict1_;
  scoped_ptr<DictionaryValue> dict12_;
  scoped_ptr<DictionaryValue> dict123_;

 private:
  ScopedTempDir temp_dir_;

  // Need these so that the DCHECKs for running on FILE or UI threads pass.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_UNITTEST_H_
