// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_unittest.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "content/browser/browser_thread.h"

// Define macro to get the __LINE__ expansion.
#define NEW_CALLBACK(expected) \
    (new AssertEqualsCallback((expected), __LINE__))

namespace {

// Callback from storage methods which performs the test assertions.
class AssertEqualsCallback : public ExtensionSettingsStorage::Callback {
 public:
  AssertEqualsCallback(DictionaryValue* expected, int line)
      : expected_(expected), line_(line), called_(false) {
  }

  ~AssertEqualsCallback() {
    // Need to DCHECK since ASSERT_* can't be used from destructors.
    DCHECK(called_);
  }

  virtual void OnSuccess(DictionaryValue* actual) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_FALSE(called_) << "Callback has already been called";
    called_ = true;
    if (expected_ == NULL) {
      ASSERT_TRUE(actual == NULL) << "Values are different:\n" <<
        "Line:     " << line_ << "\n" <<
        "Expected: NULL\n" <<
        "Got:      " << GetJson(actual);
    } else {
      ASSERT_TRUE(expected_->Equals(actual)) << "Values are different:\n" <<
          "Line:     " << line_ << "\n" <<
          "Expected: " << GetJson(expected_) <<
          "Got:      " << GetJson(actual);
      delete actual;
    }
  }

  virtual void OnFailure(const std::string& message) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_FALSE(called_) << "Callback has already been called";
    called_ = true;
    // No tests allow failure (yet).
    ASSERT_TRUE(false) << "Callback failed on line " << line_;
  }

 private:
  std::string GetJson(Value* value) {
    std::string json;
    base::JSONWriter::Write(value, true, &json);
    return json;
  }

  DictionaryValue* expected_;
  int line_;
  bool called_;
};

}  // namespace

ExtensionSettingsStorageTest::ExtensionSettingsStorageTest()
    : key1_("foo"), key2_("bar"), key3_("baz") {
  val1_.reset(Value::CreateStringValue(key1_ + "Value"));
  val2_.reset(Value::CreateStringValue(key2_ + "Value"));
  val3_.reset(Value::CreateStringValue(key3_ + "Value"));

  emptyList_.reset(new ListValue());

  list1_.reset(new ListValue());
  list1_->Append(Value::CreateStringValue(key1_));

  list2_.reset(new ListValue());
  list2_->Append(Value::CreateStringValue(key2_));

  list12_.reset(new ListValue());
  list12_->Append(Value::CreateStringValue(key1_));
  list12_->Append(Value::CreateStringValue(key2_));

  list13_.reset(new ListValue());
  list13_->Append(Value::CreateStringValue(key1_));
  list13_->Append(Value::CreateStringValue(key3_));

  list123_.reset(new ListValue());
  list123_->Append(Value::CreateStringValue(key1_));
  list123_->Append(Value::CreateStringValue(key2_));
  list123_->Append(Value::CreateStringValue(key3_));

  emptyDict_.reset(new DictionaryValue());

  dict1_.reset(new DictionaryValue());
  dict1_->Set(key1_, val1_->DeepCopy());

  dict12_.reset(new DictionaryValue());
  dict12_->Set(key1_, val1_->DeepCopy());
  dict12_->Set(key2_, val2_->DeepCopy());

  dict123_.reset(new DictionaryValue());
  dict123_->Set(key1_, val1_->DeepCopy());
  dict123_->Set(key2_, val2_->DeepCopy());
  dict123_->Set(key3_, val3_->DeepCopy());
}

ExtensionSettingsStorageTest::~ExtensionSettingsStorageTest() {
}

void ExtensionSettingsStorageTest::SetUp() {
  ui_message_loop_ = new MessageLoopForUI();
  // Use the same message loop for the UI and FILE threads, giving a test
  // pattern where storage API calls get posted to the same message loop (the
  // current one), then all run with MessageLoop::current()->RunAllPending().
  ui_thread_ = new BrowserThread(BrowserThread::UI, MessageLoop::current());
  file_thread_ = new BrowserThread(BrowserThread::FILE, MessageLoop::current());

  FilePath temp_dir;
  file_util::CreateNewTempDirectory(FilePath::StringType(), &temp_dir);
  settings_ = new ExtensionSettings(temp_dir);

  storage_ = NULL;
  (GetParam())(
      settings_,
      "fakeExtension",
      base::Bind(
          &ExtensionSettingsStorageTest::SetStorage,
          base::Unretained(this)));
  MessageLoop::current()->RunAllPending();
  DCHECK(storage_ != NULL);
}

void ExtensionSettingsStorageTest::TearDown() {
  settings_ = NULL;
  delete file_thread_;
  delete ui_thread_;
  delete ui_message_loop_;
}

void ExtensionSettingsStorageTest::SetStorage(
    ExtensionSettingsStorage* storage) {
  storage_ = storage;
}

TEST_P(ExtensionSettingsStorageTest, GetWhenEmpty) {
  storage_->Get(key1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(key2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, GetWithSingleValue) {
  storage_->Set(key1_, *val1_, NEW_CALLBACK(dict1_.get()));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(key2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(NEW_CALLBACK(dict1_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, GetWithMultipleValues) {
  storage_->Set(*dict12_, NEW_CALLBACK(dict12_.get()));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list13_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list12_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(NEW_CALLBACK(dict12_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, RemoveWhenEmpty) {
  storage_->Remove(key1_, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithSingleValue) {
  storage_->Set(key1_, *val1_, NEW_CALLBACK(dict1_.get()));
  MessageLoop::current()->RunAllPending();
  storage_->Remove(key1_, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(key2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list12_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithMultipleValues) {
  storage_->Set(*dict123_, NEW_CALLBACK(dict123_.get()));
  MessageLoop::current()->RunAllPending();
  storage_->Remove(key3_, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list13_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list12_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(NEW_CALLBACK(dict12_.get()));

  storage_->Remove(*list2_, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(key2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list2_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(NEW_CALLBACK(dict1_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, SetWhenOverwriting) {
  DictionaryValue key1Val2;
  key1Val2.Set(key1_, val2_->DeepCopy());
  storage_->Set(key1_, *val2_, NEW_CALLBACK(&key1Val2));
  MessageLoop::current()->RunAllPending();

  storage_->Set(*dict12_, NEW_CALLBACK(dict12_.get()));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(key3_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*emptyList_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list13_, NEW_CALLBACK(dict1_.get()));
  storage_->Get(*list12_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(*list123_, NEW_CALLBACK(dict12_.get()));
  storage_->Get(NEW_CALLBACK(dict12_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenEmpty) {
  storage_->Clear(NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenNotEmpty) {
  storage_->Set(*dict12_, NEW_CALLBACK(dict12_.get()));
  MessageLoop::current()->RunAllPending();

  storage_->Clear(NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(key1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(*list1_, NEW_CALLBACK(emptyDict_.get()));
  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

// Dots should be allowed in key names; they shouldn't be interpreted as
// indexing into a dictionary.
TEST_P(ExtensionSettingsStorageTest, DotsInKeyNames) {
  std::string dot_key("foo.bar");
  StringValue dot_value("baz.qux");
  ListValue dot_list;
  dot_list.Append(Value::CreateStringValue(dot_key));
  DictionaryValue dot_dict;
  dot_dict.SetWithoutPathExpansion(dot_key, dot_value.DeepCopy());

  storage_->Set(dot_key, dot_value, NEW_CALLBACK(&dot_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Get(dot_key, NEW_CALLBACK(&dot_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Remove(dot_key, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Set(dot_dict, NEW_CALLBACK(&dot_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Get(dot_list, NEW_CALLBACK(&dot_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Remove(dot_list, NEW_CALLBACK(NULL));
  MessageLoop::current()->RunAllPending();

  storage_->Get(NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}

TEST_P(ExtensionSettingsStorageTest, DotsInKeyNamesWithDicts) {
  DictionaryValue outer_dict;
  DictionaryValue* inner_dict = new DictionaryValue();
  outer_dict.Set("foo", inner_dict);
  inner_dict->Set("bar", Value::CreateStringValue("baz"));

  storage_->Set(outer_dict, NEW_CALLBACK(&outer_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Get("foo", NEW_CALLBACK(&outer_dict));
  MessageLoop::current()->RunAllPending();

  storage_->Get("foo.bar", NEW_CALLBACK(emptyDict_.get()));
  MessageLoop::current()->RunAllPending();
}
