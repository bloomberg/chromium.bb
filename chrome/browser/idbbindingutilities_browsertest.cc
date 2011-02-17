// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "webkit/glue/idb_bindings.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebSerializedScriptValue;

// Sanity test, check the function call directly outside the sandbox.
TEST(IDBKeyPathWithoutSandbox, Value) {
  char16 data[] = {0x0353,0x6f66,0x536f,0x7a03,0x6f6f,0x017b};
  std::vector<WebSerializedScriptValue> serialized_values;
  serialized_values.push_back(
      WebSerializedScriptValue::fromString(string16(data, arraysize(data))));
  serialized_values.push_back(
      WebSerializedScriptValue::fromString(string16()));

  std::vector<WebKit::WebIDBKey> values;
  string16 key_path(UTF8ToUTF16("foo"));
  bool error = webkit_glue::IDBKeysFromValuesAndKeyPath(
      serialized_values, key_path, &values);

  ASSERT_EQ(size_t(2), values.size());
  ASSERT_EQ(WebKit::WebIDBKey::StringType, values[0].type());
  ASSERT_EQ(UTF8ToUTF16("zoo"), values[0].string());
  ASSERT_EQ(WebKit::WebIDBKey::InvalidType, values[1].type());
  ASSERT_FALSE(error);

  values.clear();
  key_path = UTF8ToUTF16("PropertyNotAvailable");
  error = webkit_glue::IDBKeysFromValuesAndKeyPath(
      serialized_values, key_path, &values);

  ASSERT_EQ(size_t(2), values.size());
  ASSERT_EQ(WebKit::WebIDBKey::InvalidType, values[0].type());
  ASSERT_EQ(WebKit::WebIDBKey::InvalidType, values[1].type());
  ASSERT_FALSE(error);

  values.clear();
  key_path = UTF8ToUTF16("!+Invalid[KeyPath[[[");
  error = webkit_glue::IDBKeysFromValuesAndKeyPath(
      serialized_values, key_path, &values);

  ASSERT_TRUE(error);
  ASSERT_EQ(size_t(2), values.size());
  ASSERT_EQ(WebKit::WebIDBKey::InvalidType, values[0].type());
  ASSERT_EQ(WebKit::WebIDBKey::InvalidType, values[1].type());
}

class IDBKeyPathHelper : public UtilityProcessHost::Client {
 public:
  IDBKeyPathHelper()
      : expected_id_(0),
        utility_process_host_(NULL),
        value_for_key_path_failed_(false) {
  }

  void CreateUtilityProcess(ResourceDispatcherHost* resource_dispatcher_host) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &IDBKeyPathHelper::CreateUtilityProcess,
                            resource_dispatcher_host));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    utility_process_host_ =
        new UtilityProcessHost(resource_dispatcher_host, this,
                               BrowserThread::IO);
    utility_process_host_->StartBatchMode();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  void DestroyUtilityProcess() {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &IDBKeyPathHelper::DestroyUtilityProcess));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    utility_process_host_->EndBatchMode();
    utility_process_host_ = NULL;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  void SetExpectedKeys(int expected_id,
                       const std::vector<IndexedDBKey>& expected_keys,
                       bool failed) {
    expected_id_ = expected_id;
    expected_keys_ = expected_keys;
    value_for_key_path_failed_ = failed;
  }

  void SetExpectedValue(const SerializedScriptValue& expected_value) {
    expected_value_ = expected_value;
  }

  void CheckValuesForKeyPath(
      int id, const std::vector<SerializedScriptValue>& serialized_values,
      const string16& key_path) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &IDBKeyPathHelper::CheckValuesForKeyPath,
                            id, serialized_values, key_path));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    bool ret =
        utility_process_host_->StartIDBKeysFromValuesAndKeyPath(
            id, serialized_values, key_path);
    ASSERT_TRUE(ret);
  }

  void CheckInjectValue(const IndexedDBKey& key,
                        const SerializedScriptValue& value,
                        const string16& key_path) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &IDBKeyPathHelper::CheckInjectValue,
                            key, value, key_path));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    bool ret = utility_process_host_->StartInjectIDBKey(key, value, key_path);
    ASSERT_TRUE(ret);
  }

  // UtilityProcessHost::Client
  virtual void OnIDBKeysFromValuesAndKeyPathSucceeded(
      int id, const std::vector<IndexedDBKey>& values) {
    EXPECT_EQ(expected_id_, id);
    EXPECT_FALSE(value_for_key_path_failed_);
    ASSERT_EQ(expected_keys_.size(), values.size());
    size_t pos = 0;
    for (std::vector<IndexedDBKey>::const_iterator i(values.begin());
         i != values.end(); ++i, ++pos) {
      ASSERT_EQ(expected_keys_[pos].type(), i->type());
      if (i->type() == WebKit::WebIDBKey::StringType) {
        ASSERT_EQ(expected_keys_[pos].string(), i->string());
      } else if (i->type() == WebKit::WebIDBKey::NumberType) {
        ASSERT_EQ(expected_keys_[pos].number(), i->number());
      }
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  virtual void OnIDBKeysFromValuesAndKeyPathFailed(int id) {
    EXPECT_TRUE(value_for_key_path_failed_);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  virtual void OnInjectIDBKeyFinished(
      const SerializedScriptValue& new_value) {
    EXPECT_EQ(expected_value_.data(), new_value.data());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }


 private:
  int expected_id_;
  std::vector<IndexedDBKey> expected_keys_;
  UtilityProcessHost* utility_process_host_;
  bool value_for_key_path_failed_;
  SerializedScriptValue expected_value_;
};

// This test fixture runs in the UI thread. However, most of the work done by
// UtilityProcessHost (and wrapped by IDBKeyPathHelper above) happens on the IO
// thread. This fixture delegates to IDBKeyPathHelper and blocks via
// "ui_test_utils::RunMessageLoop()", until IDBKeyPathHelper posts a quit
// message the MessageLoop.
class ScopedIDBKeyPathHelper {
 public:
  ScopedIDBKeyPathHelper() {
    key_path_helper_ = new IDBKeyPathHelper();
    key_path_helper_->CreateUtilityProcess(
        g_browser_process->resource_dispatcher_host());
    ui_test_utils::RunMessageLoop();
  }

  ~ScopedIDBKeyPathHelper() {
    key_path_helper_->DestroyUtilityProcess();
    ui_test_utils::RunMessageLoop();
  }

  void SetExpectedKeys(int id, const std::vector<IndexedDBKey>& expected_keys,
                   bool failed) {
    key_path_helper_->SetExpectedKeys(id, expected_keys, failed);
  }

  void SetExpectedValue(const SerializedScriptValue& expected_value) {
    key_path_helper_->SetExpectedValue(expected_value);
  }

  void CheckValuesForKeyPath(
      int id,
      const std::vector<SerializedScriptValue>& serialized_script_values,
      const string16& key_path) {
    key_path_helper_->CheckValuesForKeyPath(id, serialized_script_values,
                                            key_path);
    ui_test_utils::RunMessageLoop();
  }

  void CheckInjectValue(const IndexedDBKey& key,
                        const SerializedScriptValue& value,
                        const string16& key_path) {
    key_path_helper_->CheckInjectValue(key, value, key_path);
    ui_test_utils::RunMessageLoop();
  }

 private:
  scoped_refptr<IDBKeyPathHelper> key_path_helper_;
};

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, IDBKeyPathExtract) {
  ScopedIDBKeyPathHelper scoped_helper;
  const int kId = 7;
  std::vector<IndexedDBKey> expected_keys;
  IndexedDBKey value;
  value.SetString(UTF8ToUTF16("zoo"));
  expected_keys.push_back(value);

  IndexedDBKey invalid_value;
  invalid_value.SetInvalid();
  expected_keys.push_back(invalid_value);

  scoped_helper.SetExpectedKeys(kId, expected_keys, false);

  char16 data[] = {0x0353,0x6f66,0x536f,0x7a03,0x6f6f,0x017b};
  std::vector<SerializedScriptValue> serialized_values;
  serialized_values.push_back(
      SerializedScriptValue(false, false, string16(data, arraysize(data))));
  serialized_values.push_back(
      SerializedScriptValue(true, false, string16()));
  scoped_helper.CheckValuesForKeyPath(
      kId, serialized_values, UTF8ToUTF16("foo"));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, IDBKeyPathPropertyNotAvailable) {
  ScopedIDBKeyPathHelper scoped_helper;
  const int kId = 7;
  std::vector<IndexedDBKey> expected_keys;
  IndexedDBKey invalid_value;
  invalid_value.SetInvalid();
  expected_keys.push_back(invalid_value);
  expected_keys.push_back(invalid_value);

  scoped_helper.SetExpectedKeys(kId, expected_keys, false);

  char16 data[] = {0x0353,0x6f66,0x536f,0x7a03,0x6f6f,0x017b};
  std::vector<SerializedScriptValue> serialized_values;
  serialized_values.push_back(
      SerializedScriptValue(false, false, string16(data, arraysize(data))));
  serialized_values.push_back(
      SerializedScriptValue(true, false, string16()));
  scoped_helper.CheckValuesForKeyPath(kId, serialized_values,
                                      UTF8ToUTF16("PropertyNotAvailable"));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, IDBKeyPathMultipleCalls) {
  ScopedIDBKeyPathHelper scoped_helper;
  const int kId = 7;
  std::vector<IndexedDBKey> expected_keys;
  IndexedDBKey invalid_value;
  invalid_value.SetInvalid();
  expected_keys.push_back(invalid_value);
  expected_keys.push_back(invalid_value);

  scoped_helper.SetExpectedKeys(kId, expected_keys, true);

  char16 data[] = {0x0353,0x6f66,0x536f,0x7a03,0x6f6f,0x017b};
  std::vector<SerializedScriptValue> serialized_values;
  serialized_values.push_back(
      SerializedScriptValue(false, false, string16(data, arraysize(data))));
  serialized_values.push_back(
      SerializedScriptValue(true, false, string16()));
  scoped_helper.CheckValuesForKeyPath(kId, serialized_values,
                                      UTF8ToUTF16("!+Invalid[KeyPath[[["));

  // Call again with the Utility process in batch mode and with valid keys.
  expected_keys.clear();
  IndexedDBKey value;
  value.SetString(UTF8ToUTF16("zoo"));
  expected_keys.push_back(value);
  expected_keys.push_back(invalid_value);
  scoped_helper.SetExpectedKeys(kId + 1, expected_keys, false);
  scoped_helper.CheckValuesForKeyPath(kId + 1, serialized_values,
                                      UTF8ToUTF16("foo"));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, InjectIDBKey) {
  // {foo: 'zoo'}
  const char16 data[] = {0x0353,0x6f66,0x536f,0x7a03,0x6f6f,0x017b};
  SerializedScriptValue value(false, false, string16(data, arraysize(data)));
  IndexedDBKey key;
  key.SetString(UTF8ToUTF16("myNewKey"));

  // {foo: 'zoo', bar: 'myNewKey'}
  const char16 expected_data[] = {0x353, 0x6f66, 0x536f, 0x7a03, 0x6f6f, 0x353,
                                  0x6162, 0x5372, 0x6d08, 0x4e79, 0x7765,
                                  0x654b, 0x7b79, 0x2};
  SerializedScriptValue expected_value(false, false,
                                       string16(expected_data,
                                                arraysize(expected_data)));

  ScopedIDBKeyPathHelper scoped_helper;
  scoped_helper.SetExpectedValue(expected_value);
  scoped_helper.CheckInjectValue(key, value, UTF8ToUTF16("bar"));

  scoped_helper.SetExpectedValue(SerializedScriptValue());  // Expect null.
  scoped_helper.CheckInjectValue(key, value, UTF8ToUTF16("bad.key.path"));
}
