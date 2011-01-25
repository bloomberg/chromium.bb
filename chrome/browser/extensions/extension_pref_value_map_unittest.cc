// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/common/pref_store_observer_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExt1[] = "ext1";
const char kExt2[] = "ext2";
const char kExt3[] = "ext3";

const char kPref1[] = "path1.subpath";
const char kPref2[] = "path2";
const char kPref3[] = "path3";
const char kPref4[] = "path4";
}  // namespace

static Value* CreateVal(const char* str) {
  return Value::CreateStringValue(str);
}

static base::Time CreateTime(int64 t) {
  return base::Time::FromInternalValue(10);
}

template <typename BASECLASS>
class ExtensionPrefValueMapTestBase : public BASECLASS {
 public:
  // Returns an empty string if the key is not set.
  std::string GetValue(const char * key, bool incognito) const {
    const Value *value = epvm_.GetEffectivePrefValue(key, incognito);
    std::string string_value = "";
    if (value)
      value->GetAsString(&string_value);
    return string_value;
  }

 protected:
  ExtensionPrefValueMap epvm_;
};

class ExtensionPrefValueMapTest
    : public ExtensionPrefValueMapTestBase<testing::Test> {
};

// A gmock-ified implementation of PrefStore::Observer.
class ExtensionPrefValueMapObserverMock
    : public ExtensionPrefValueMap::Observer {
 public:
  ExtensionPrefValueMapObserverMock() {}
  virtual ~ExtensionPrefValueMapObserverMock() {}

  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD0(OnInitializationCompleted, void());
  MOCK_METHOD0(OnExtensionPrefValueMapDestruction, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefValueMapObserverMock);
};

TEST_F(ExtensionPrefValueMapTest, SetAndGetPrefValue) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  EXPECT_EQ("val1", GetValue(kPref1, false));
};

TEST_F(ExtensionPrefValueMapTest, GetNotSetPrefValue) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  EXPECT_EQ("", GetValue(kPref1, false));
};

// Make sure the last-installed extension wins for each preference.
TEST_F(ExtensionPrefValueMapTest, Override) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.RegisterExtension(kExt2, CreateTime(20), true);
  epvm_.RegisterExtension(kExt3, CreateTime(30), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, false, CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, false, CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, false, CreateVal("val4"));
  epvm_.SetExtensionPref(kExt2, kPref2, false, CreateVal("val5"));

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val6"));
  epvm_.SetExtensionPref(kExt1, kPref2, false, CreateVal("val7"));
  epvm_.SetExtensionPref(kExt1, kPref3, false, CreateVal("val8"));

  EXPECT_EQ("val3", GetValue(kPref1, false));
  EXPECT_EQ("val5", GetValue(kPref2, false));
  EXPECT_EQ("val8", GetValue(kPref3, false));
}

TEST_F(ExtensionPrefValueMapTest, SetAndGetPrefValueIncognito) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  EXPECT_EQ("val1", GetValue(kPref1, true));
}

TEST_F(ExtensionPrefValueMapTest, UninstallOnlyExtension) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.UnregisterExtension(kExt1);

  EXPECT_EQ("", GetValue(kPref1, false));
}

// Tests uninstalling an extension that wasn't winning for any preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallIrrelevantExtension) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.RegisterExtension(kExt2, CreateTime(10), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, false, CreateVal("val2"));

  epvm_.SetExtensionPref(kExt1, kPref2, false, CreateVal("val3"));
  epvm_.SetExtensionPref(kExt2, kPref2, false, CreateVal("val4"));

  epvm_.UnregisterExtension(kExt1);

  EXPECT_EQ("val2", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
}

// Tests uninstalling an extension that was winning for all preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallExtensionFromTop) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.RegisterExtension(kExt2, CreateTime(20), true);
  epvm_.RegisterExtension(kExt3, CreateTime(30), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, false, CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, false, CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, false, CreateVal("val4"));
  epvm_.SetExtensionPref(kExt3, kPref2, false, CreateVal("val5"));

  epvm_.UnregisterExtension(kExt3);

  EXPECT_EQ("val2", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
}

// Tests uninstalling an extension that was winning for only some preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallExtensionFromMiddle) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.RegisterExtension(kExt2, CreateTime(20), true);
  epvm_.RegisterExtension(kExt3, CreateTime(30), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, false, CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, false, CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, false, CreateVal("val4"));
  epvm_.SetExtensionPref(kExt2, kPref2, false, CreateVal("val5"));

  epvm_.SetExtensionPref(kExt1, kPref3, false, CreateVal("val6"));

  epvm_.SetExtensionPref(kExt2, kPref4, false, CreateVal("val7"));

  epvm_.UnregisterExtension(kExt2);

  EXPECT_EQ("val3", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
  EXPECT_EQ("val6", GetValue(kPref3, false));
  EXPECT_EQ("", GetValue(kPref4, false));
}

// Tests triggering of notifications to registered observers.
TEST_F(ExtensionPrefValueMapTest, NotifyWhenNeeded) {
  using testing::_;
  using testing::Mock;
  using testing::StrEq;

  epvm_.RegisterExtension(kExt1, CreateTime(10), true);

  ExtensionPrefValueMapObserverMock observer;
  epvm_.AddObserver(&observer);

  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  Mock::VerifyAndClearExpectations(&observer);

  // Write the same value again.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1))).Times(0);
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  Mock::VerifyAndClearExpectations(&observer);

  // Override incognito value.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, true, CreateVal("val2"));
  Mock::VerifyAndClearExpectations(&observer);

  // Override non-incognito value.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val3"));
  Mock::VerifyAndClearExpectations(&observer);

  // Disable.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionState(kExt1, false);
  Mock::VerifyAndClearExpectations(&observer);

  // Enable.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionState(kExt1, true);
  Mock::VerifyAndClearExpectations(&observer);

  // Uninstall
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.UnregisterExtension(kExt1);
  Mock::VerifyAndClearExpectations(&observer);

  epvm_.RemoveObserver(&observer);

  // Write new value --> no notification after removing observer.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1))).Times(0);
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val4"));
  Mock::VerifyAndClearExpectations(&observer);
}

// Tests disabling an extension.
TEST_F(ExtensionPrefValueMapTest, DisableExt) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionState(kExt1, false);
  EXPECT_EQ("", GetValue(kPref1, false));
}

// Tests disabling and reenabling an extension.
TEST_F(ExtensionPrefValueMapTest, ReenableExt) {
  epvm_.RegisterExtension(kExt1, CreateTime(10), true);

  epvm_.SetExtensionPref(kExt1, kPref1, false, CreateVal("val1"));
  epvm_.SetExtensionState(kExt1, false);
  epvm_.SetExtensionState(kExt1, true);
  EXPECT_EQ("val1", GetValue(kPref1, false));
}

struct OverrideIncognitoTestCase {
  OverrideIncognitoTestCase(int val_ext1_regular,
                            int val_ext1_incognito,
                            int val_ext2_regular,
                            int val_ext2_incognito,
                            int effective_value_regular,
                            int effective_value_incognito)
      : val_ext1_regular_(val_ext1_regular),
        val_ext1_incognito_(val_ext1_incognito),
        val_ext2_regular_(val_ext2_regular),
        val_ext2_incognito_(val_ext2_incognito),
        effective_value_regular_(effective_value_regular),
        effective_value_incognito_(effective_value_incognito) {}

  int val_ext1_regular_;           // pref value of extension 1
  int val_ext1_incognito_;         // pref value of extension 1 incognito
  int val_ext2_regular_;           // pref value of extension 2
  int val_ext2_incognito_;         // pref value of extension 2 incognito
  int effective_value_regular_;    // desired winner regular
  int effective_value_incognito_;  // desired winner incognito
};

class ExtensionPrefValueMapTestIncognitoTests
    : public ExtensionPrefValueMapTestBase<
          testing::TestWithParam<OverrideIncognitoTestCase> > {
};

TEST_P(ExtensionPrefValueMapTestIncognitoTests, OverrideIncognito) {
  OverrideIncognitoTestCase test = GetParam();
  const char* strings[] = {
      "undefined",
      "val1",
      "val2",
      "val3",
      "val4"
  };

  epvm_.RegisterExtension(kExt1, CreateTime(10), true);
  epvm_.RegisterExtension(kExt2, CreateTime(20), true);
  if (test.val_ext1_regular_) {
    epvm_.SetExtensionPref(kExt1, kPref1, false,
                           CreateVal(strings[test.val_ext1_regular_]));
  }
  if (test.val_ext1_incognito_) {
    epvm_.SetExtensionPref(kExt1, kPref1, true,
                           CreateVal(strings[test.val_ext1_incognito_]));
  }
  if (test.val_ext2_regular_) {
    epvm_.SetExtensionPref(kExt2, kPref1, false,
                           CreateVal(strings[test.val_ext2_regular_]));
  }
  if (test.val_ext2_incognito_) {
    epvm_.SetExtensionPref(kExt2, kPref1, true,
                           CreateVal(strings[test.val_ext2_incognito_]));
  }
  std::string actual;
  EXPECT_EQ(strings[test.effective_value_regular_], GetValue(kPref1, false));
  EXPECT_EQ(strings[test.effective_value_incognito_], GetValue(kPref1, true));
  epvm_.UnregisterExtension(kExt1);
  epvm_.UnregisterExtension(kExt2);
}

INSTANTIATE_TEST_CASE_P(
    ExtensionPrefValueMapTestIncognitoTestsInstance,
    ExtensionPrefValueMapTestIncognitoTests,
    testing::Values(
        // e.g. (1, 0, 0, 4,  1, 4), means:
        // ext1 regular is set to "val1", ext2 incognito is set to "val4"
        // --> the winning regular value is "val1", the winning incognito
        //     value is "val4".
        OverrideIncognitoTestCase(1, 0, 0, 0,  1, 1),
        OverrideIncognitoTestCase(1, 2, 0, 0,  1, 2),
        OverrideIncognitoTestCase(1, 0, 3, 0,  3, 3),
        OverrideIncognitoTestCase(1, 0, 0, 4,  1, 4),
        // The last 3 in the following line is intentional!
        OverrideIncognitoTestCase(1, 2, 3, 0,  3, 3),
        OverrideIncognitoTestCase(1, 2, 0, 4,  1, 4),
        OverrideIncognitoTestCase(1, 2, 3, 4,  3, 4)));
