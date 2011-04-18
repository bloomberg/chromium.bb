// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_member.h"

#include "base/message_loop.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/test/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBoolPref[] = "bool";
const char kIntPref[] = "int";
const char kDoublePref[] = "double";
const char kStringPref[] = "string";
const char kListPref[] = "list";

void RegisterTestPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(kBoolPref, false);
  prefs->RegisterIntegerPref(kIntPref, 0);
  prefs->RegisterDoublePref(kDoublePref, 0.0);
  prefs->RegisterStringPref(kStringPref, "default");
  prefs->RegisterListPref(kListPref);
}

class GetPrefValueCallback
    : public base::RefCountedThreadSafe<GetPrefValueCallback> {
 public:
  GetPrefValueCallback() : value_(false) {}

  void Init(const char* pref_name, PrefService* prefs) {
    pref_.Init(pref_name, prefs, NULL);
    pref_.MoveToThread(BrowserThread::IO);
  }

  bool FetchValue() {
    if (!BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &GetPrefValueCallback::GetPrefValueOnIOThread))) {
      return false;
    }
    MessageLoop::current()->Run();
    return true;
  }

  bool value() { return value_; }

 private:
  friend class base::RefCountedThreadSafe<GetPrefValueCallback>;
  ~GetPrefValueCallback() {}

  void GetPrefValueOnIOThread() {
    value_ = pref_.GetValue();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  BooleanPrefMember pref_;
  bool value_;
};

class PrefMemberTestClass : public NotificationObserver {
 public:
  explicit PrefMemberTestClass(PrefService* prefs)
      : observe_cnt_(0), prefs_(prefs) {
    str_.Init(kStringPref, prefs, this);
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(NotificationType::PREF_CHANGED == type);
    PrefService* prefs_in = Source<PrefService>(source).ptr();
    EXPECT_EQ(prefs_in, prefs_);
    std::string* pref_name_in = Details<std::string>(details).ptr();
    EXPECT_EQ(*pref_name_in, kStringPref);
    EXPECT_EQ(str_.GetValue(), prefs_->GetString(kStringPref));
    ++observe_cnt_;
  }

  StringPrefMember str_;
  int observe_cnt_;

 private:
  PrefService* prefs_;
};

}  // anonymous namespace

TEST(PrefMemberTest, BasicGetAndSet) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  // Test bool
  BooleanPrefMember boolean;
  boolean.Init(kBoolPref, &prefs, NULL);

  // Check the defaults
  EXPECT_FALSE(prefs.GetBoolean(kBoolPref));
  EXPECT_FALSE(boolean.GetValue());
  EXPECT_FALSE(*boolean);

  // Try changing through the member variable.
  boolean.SetValue(true);
  EXPECT_TRUE(boolean.GetValue());
  EXPECT_TRUE(prefs.GetBoolean(kBoolPref));
  EXPECT_TRUE(*boolean);

  // Try changing back through the pref.
  prefs.SetBoolean(kBoolPref, false);
  EXPECT_FALSE(prefs.GetBoolean(kBoolPref));
  EXPECT_FALSE(boolean.GetValue());
  EXPECT_FALSE(*boolean);

  // Test int
  IntegerPrefMember integer;
  integer.Init(kIntPref, &prefs, NULL);

  // Check the defaults
  EXPECT_EQ(0, prefs.GetInteger(kIntPref));
  EXPECT_EQ(0, integer.GetValue());
  EXPECT_EQ(0, *integer);

  // Try changing through the member variable.
  integer.SetValue(5);
  EXPECT_EQ(5, integer.GetValue());
  EXPECT_EQ(5, prefs.GetInteger(kIntPref));
  EXPECT_EQ(5, *integer);

  // Try changing back through the pref.
  prefs.SetInteger(kIntPref, 2);
  EXPECT_EQ(2, prefs.GetInteger(kIntPref));
  EXPECT_EQ(2, integer.GetValue());
  EXPECT_EQ(2, *integer);

  // Test double
  DoublePrefMember double_member;
  double_member.Init(kDoublePref, &prefs, NULL);

  // Check the defaults
  EXPECT_EQ(0.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(0.0, double_member.GetValue());
  EXPECT_EQ(0.0, *double_member);

  // Try changing through the member variable.
  double_member.SetValue(1.0);
  EXPECT_EQ(1.0, double_member.GetValue());
  EXPECT_EQ(1.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(1.0, *double_member);

  // Try changing back through the pref.
  prefs.SetDouble(kDoublePref, 3.0);
  EXPECT_EQ(3.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(3.0, double_member.GetValue());
  EXPECT_EQ(3.0, *double_member);

  // Test string
  StringPrefMember string;
  string.Init(kStringPref, &prefs, NULL);

  // Check the defaults
  EXPECT_EQ("default", prefs.GetString(kStringPref));
  EXPECT_EQ("default", string.GetValue());
  EXPECT_EQ("default", *string);

  // Try changing through the member variable.
  string.SetValue("foo");
  EXPECT_EQ("foo", string.GetValue());
  EXPECT_EQ("foo", prefs.GetString(kStringPref));
  EXPECT_EQ("foo", *string);

  // Try changing back through the pref.
  prefs.SetString(kStringPref, "bar");
  EXPECT_EQ("bar", prefs.GetString(kStringPref));
  EXPECT_EQ("bar", string.GetValue());
  EXPECT_EQ("bar", *string);

  // Test list
  ListPrefMember list;
  list.Init(kListPref, &prefs, NULL);

  // Check the defaults
  const ListValue* list_value = prefs.GetList(kListPref);
  ASSERT_TRUE(list_value != NULL);
  EXPECT_EQ(0u, list_value->GetSize());
  EXPECT_TRUE(list_value->empty());
  ASSERT_TRUE(list.GetValue() != NULL);
  EXPECT_EQ(0u, list.GetValue()->GetSize());
  EXPECT_TRUE(list.GetValue()->empty());
  ASSERT_TRUE(*list != NULL);
  EXPECT_EQ(0u, (*list)->GetSize());
  EXPECT_TRUE((*list)->empty());

  // Try changing through the member variable.
  scoped_ptr<ListValue> list_value_numbers(new ListValue());
  list_value_numbers->Append(new StringValue("one"));
  list_value_numbers->Append(new StringValue("two"));
  list_value_numbers->Append(new StringValue("three"));
  list.SetValue(list_value_numbers.get());
  EXPECT_TRUE(list_value_numbers->Equals(list.GetValue()));
  EXPECT_TRUE(list_value_numbers->Equals(prefs.GetList(kListPref)));
  EXPECT_TRUE(list_value_numbers->Equals(*list));

  // Try changing back through the pref.
  ListValue* list_value_ints = new ListValue();
  list_value_ints->Append(new FundamentalValue(1));
  list_value_ints->Append(new FundamentalValue(2));
  list_value_ints->Append(new FundamentalValue(3));
  prefs.SetList(kListPref, list_value_ints); // takes ownership
  EXPECT_TRUE(list_value_ints->Equals(list.GetValue()));
  EXPECT_TRUE(list_value_ints->Equals(prefs.GetList(kListPref)));
  EXPECT_TRUE(list_value_ints->Equals(*list));
}

TEST(PrefMemberTest, TwoPrefs) {
  // Make sure two DoublePrefMembers stay in sync.
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  DoublePrefMember pref1;
  pref1.Init(kDoublePref, &prefs, NULL);
  DoublePrefMember pref2;
  pref2.Init(kDoublePref, &prefs, NULL);

  pref1.SetValue(2.3);
  EXPECT_EQ(2.3, *pref2);

  pref2.SetValue(3.5);
  EXPECT_EQ(3.5, *pref1);

  prefs.SetDouble(kDoublePref, 4.2);
  EXPECT_EQ(4.2, *pref1);
  EXPECT_EQ(4.2, *pref2);
}

TEST(PrefMemberTest, Observer) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  PrefMemberTestClass test_obj(&prefs);
  EXPECT_EQ("default", *test_obj.str_);

  // Calling SetValue should not fire the observer.
  test_obj.str_.SetValue("hello");
  EXPECT_EQ(0, test_obj.observe_cnt_);
  EXPECT_EQ("hello", prefs.GetString(kStringPref));

  // Changing the pref does fire the observer.
  prefs.SetString(kStringPref, "world");
  EXPECT_EQ(1, test_obj.observe_cnt_);
  EXPECT_EQ("world", *(test_obj.str_));

  // Not changing the value should not fire the observer.
  prefs.SetString(kStringPref, "world");
  EXPECT_EQ(1, test_obj.observe_cnt_);
  EXPECT_EQ("world", *(test_obj.str_));

  prefs.SetString(kStringPref, "hello");
  EXPECT_EQ(2, test_obj.observe_cnt_);
  EXPECT_EQ("hello", prefs.GetString(kStringPref));
}

TEST(PrefMemberTest, NoInit) {
  // Make sure not calling Init on a PrefMember doesn't cause problems.
  IntegerPrefMember pref;
}

TEST(PrefMemberTest, MoveToThread) {
  TestingPrefService prefs;
  scoped_refptr<GetPrefValueCallback> callback =
      make_scoped_refptr(new GetPrefValueCallback());
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);
  BrowserThread io_thread(BrowserThread::IO);
  ASSERT_TRUE(io_thread.Start());
  RegisterTestPrefs(&prefs);
  callback->Init(kBoolPref, &prefs);

  ASSERT_TRUE(callback->FetchValue());
  EXPECT_FALSE(callback->value());

  prefs.SetBoolean(kBoolPref, true);

  ASSERT_TRUE(callback->FetchValue());
  EXPECT_TRUE(callback->value());
}
