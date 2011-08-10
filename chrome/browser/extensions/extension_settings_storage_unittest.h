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
#include "base/task.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "content/browser/browser_thread.h"

// Parameter type for the value-parameterized tests.
typedef void (*ExtensionSettingsStorageTestParam)(
    ExtensionSettings* settings,
    const std::string& extension_id,
    const ExtensionSettings::Callback& callback);

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
  ExtensionSettingsStorage* storage_;

  std::string key1_;
  std::string key2_;
  std::string key3_;
  scoped_ptr<Value> val1_;
  scoped_ptr<Value> val2_;
  scoped_ptr<Value> val3_;
  scoped_ptr<ListValue> emptyList_;
  scoped_ptr<ListValue> list1_;
  scoped_ptr<ListValue> list2_;
  scoped_ptr<ListValue> list12_;
  scoped_ptr<ListValue> list13_;
  scoped_ptr<ListValue> list123_;
  scoped_ptr<DictionaryValue> emptyDict_;
  scoped_ptr<DictionaryValue> dict1_;
  scoped_ptr<DictionaryValue> dict12_;
  scoped_ptr<DictionaryValue> dict123_;

 private:
  void SetStorage(ExtensionSettingsStorage* storage);

  scoped_refptr<ExtensionSettings> settings_;
  MessageLoopForUI* ui_message_loop_;
  BrowserThread* ui_thread_;
  BrowserThread* file_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_UNITTEST_H_
