// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <limits>
#include <string>
#include <vector>

#include "base/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/leveldb/leveldb_slice.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;

namespace content {

namespace {

static IndexedDBKey CreateArrayIDBKey() {
  return IndexedDBKey(IndexedDBKey::KeyArray());
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1) {
  IndexedDBKey::KeyArray array;
  array.push_back(key1);
  return IndexedDBKey(array);
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1,
                                      const IndexedDBKey& key2) {
  IndexedDBKey::KeyArray array;
  array.push_back(key1);
  array.push_back(key2);
  return IndexedDBKey(array);
}

static std::vector<char> WrappedEncodeByte(char value) {
  std::vector<char> buffer;
  EncodeByte(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeByte) {
  std::vector<char> expected;
  expected.push_back(0);
  unsigned char c;

  c = 0;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 1;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 255;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));
}

TEST(IndexedDBLevelDBCodingTest, DecodeByte) {
  std::vector<unsigned char> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];
    std::vector<char> v;
    EncodeByte(n, &v);

    unsigned char res;
    const char* p = DecodeByte(&*v.begin(), &*v.rbegin() + 1, res);
    EXPECT_EQ(n, res);
    EXPECT_EQ(&*v.rbegin() + 1, p);
  }
}

static std::vector<char> WrappedEncodeBool(bool value) {
  std::vector<char> buffer;
  EncodeBool(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeBool) {
  {
    std::vector<char> expected;
    expected.push_back(1);
    EXPECT_EQ(expected, WrappedEncodeBool(true));
  }
  {
    std::vector<char> expected;
    expected.push_back(0);
    EXPECT_EQ(expected, WrappedEncodeBool(false));
  }
}

static int CompareKeys(const std::vector<char>& a, const std::vector<char>& b) {
  bool ok;
  int result = CompareEncodedIDBKeys(a, b, ok);
  EXPECT_TRUE(ok);
  return result;
}

TEST(IndexedDBLevelDBCodingTest, MaxIDBKey) {
  std::vector<char> max_key = MaxIDBKey();

  std::vector<char> min_key = MinIDBKey();
  std::vector<char> array_key =
      EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()));
  std::vector<char> string_key =
      EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")));
  std::vector<char> number_key =
      EncodeIDBKey(IndexedDBKey(3.14, WebIDBKey::NumberType));
  std::vector<char> date_key =
      EncodeIDBKey(IndexedDBKey(1000000, WebIDBKey::DateType));

  EXPECT_GT(CompareKeys(max_key, min_key), 0);
  EXPECT_GT(CompareKeys(max_key, array_key), 0);
  EXPECT_GT(CompareKeys(max_key, string_key), 0);
  EXPECT_GT(CompareKeys(max_key, number_key), 0);
  EXPECT_GT(CompareKeys(max_key, date_key), 0);
}

TEST(IndexedDBLevelDBCodingTest, MinIDBKey) {
  std::vector<char> min_key = MinIDBKey();

  std::vector<char> max_key = MaxIDBKey();
  std::vector<char> array_key =
      EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()));
  std::vector<char> string_key =
      EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")));
  std::vector<char> number_key =
      EncodeIDBKey(IndexedDBKey(3.14, WebIDBKey::NumberType));
  std::vector<char> date_key =
      EncodeIDBKey(IndexedDBKey(1000000, WebIDBKey::DateType));

  EXPECT_LT(CompareKeys(min_key, max_key), 0);
  EXPECT_LT(CompareKeys(min_key, array_key), 0);
  EXPECT_LT(CompareKeys(min_key, string_key), 0);
  EXPECT_LT(CompareKeys(min_key, number_key), 0);
  EXPECT_LT(CompareKeys(min_key, date_key), 0);
}

static std::vector<char> WrappedEncodeInt(int64 value) {
  std::vector<char> buffer;
  EncodeInt(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeInt) {
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(0).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(1).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(255).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeInt(256).size());
  EXPECT_EQ(static_cast<size_t>(4), WrappedEncodeInt(0xffffffff).size());
#ifdef NDEBUG
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeInt(-1).size());
#endif
}

TEST(IndexedDBLevelDBCodingTest, DecodeBool) {
  {
    std::vector<char> encoded;
    encoded.push_back(1);
    EXPECT_TRUE(DecodeBool(&*encoded.begin(), &*encoded.rbegin() + 1));
  }
  {
    std::vector<char> encoded;
    encoded.push_back(0);
    EXPECT_FALSE(DecodeBool(&*encoded.begin(), &*encoded.rbegin() + 1));
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeInt) {
  std::vector<int64> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);
  test_cases.push_back(256);
  test_cases.push_back(65535);
  test_cases.push_back(655536);
  test_cases.push_back(7711192431755665792ll);
  test_cases.push_back(0x7fffffffffffffffll);
#ifdef NDEBUG
  test_cases.push_back(-3);
#endif

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64 n = test_cases[i];
    std::vector<char> v = WrappedEncodeInt(n);
    EXPECT_EQ(n, DecodeInt(&*v.begin(), &*v.rbegin() + 1));
  }
}

static std::vector<char> WrappedEncodeVarInt(int64 value) {
  std::vector<char> buffer;
  EncodeVarInt(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeVarInt) {
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeVarInt(0).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeVarInt(1).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeVarInt(255).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeVarInt(256).size());
  EXPECT_EQ(static_cast<size_t>(5), WrappedEncodeVarInt(0xffffffff).size());
  EXPECT_EQ(static_cast<size_t>(8),
            WrappedEncodeVarInt(0xfffffffffffffLL).size());
  EXPECT_EQ(static_cast<size_t>(9),
            WrappedEncodeVarInt(0x7fffffffffffffffLL).size());
#ifdef NDEBUG
  EXPECT_EQ(static_cast<size_t>(10), WrappedEncodeVarInt(-100).size());
#endif
}

TEST(IndexedDBLevelDBCodingTest, DecodeVarInt) {
  std::vector<int64> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);
  test_cases.push_back(256);
  test_cases.push_back(65535);
  test_cases.push_back(655536);
  test_cases.push_back(7711192431755665792ll);
  test_cases.push_back(0x7fffffffffffffffll);
#ifdef NDEBUG
  test_cases.push_back(-3);
#endif

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64 n = test_cases[i];
    std::vector<char> v = WrappedEncodeVarInt(n);

    int64 res;
    const char* p = DecodeVarInt(&*v.begin(), &*v.rbegin() + 1, res);
    EXPECT_EQ(n, res);
    EXPECT_EQ(&*v.rbegin() + 1, p);

    p = DecodeVarInt(&*v.begin(), &*v.rbegin(), res);
    EXPECT_EQ(0, p);
    p = DecodeVarInt(&*v.begin(), &*v.begin(), res);
    EXPECT_EQ(0, p);
  }
}

static std::vector<char> WrappedEncodeString(string16 value) {
  std::vector<char> buffer;
  EncodeString(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeString) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(static_cast<size_t>(0),
            WrappedEncodeString(ASCIIToUTF16("")).size());
  EXPECT_EQ(static_cast<size_t>(2),
            WrappedEncodeString(ASCIIToUTF16("a")).size());
  EXPECT_EQ(static_cast<size_t>(6),
            WrappedEncodeString(ASCIIToUTF16("foo")).size());
  EXPECT_EQ(static_cast<size_t>(6),
            WrappedEncodeString(string16(test_string_a)).size());
  EXPECT_EQ(static_cast<size_t>(4),
            WrappedEncodeString(string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeString) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};
  const char empty[] = {0};
  std::vector<char> v;

  EXPECT_EQ(string16(), DecodeString(empty, empty));

  v = WrappedEncodeString(ASCIIToUTF16("a"));
  EXPECT_EQ(ASCIIToUTF16("a"), DecodeString(&*v.begin(), &*v.rbegin() + 1));

  v = WrappedEncodeString(ASCIIToUTF16("foo"));
  EXPECT_EQ(ASCIIToUTF16("foo"), DecodeString(&*v.begin(), &*v.rbegin() + 1));

  v = WrappedEncodeString(string16(test_string_a));
  EXPECT_EQ(string16(test_string_a),
            DecodeString(&*v.begin(), &*v.rbegin() + 1));

  v = WrappedEncodeString(string16(test_string_b));
  EXPECT_EQ(string16(test_string_b),
            DecodeString(&*v.begin(), &*v.rbegin() + 1));
}

static std::vector<char> WrappedEncodeStringWithLength(string16 value) {
  std::vector<char> buffer;
  EncodeStringWithLength(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeStringWithLength) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(static_cast<size_t>(1),
            WrappedEncodeStringWithLength(string16()).size());
  EXPECT_EQ(static_cast<size_t>(3),
            WrappedEncodeStringWithLength(ASCIIToUTF16("a")).size());
  EXPECT_EQ(static_cast<size_t>(7),
            WrappedEncodeStringWithLength(string16(test_string_a)).size());
  EXPECT_EQ(static_cast<size_t>(5),
            WrappedEncodeStringWithLength(string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeStringWithLength) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  const int kLongStringLen = 1234;
  char16 long_string[kLongStringLen + 1];
  for (int i = 0; i < kLongStringLen; ++i)
    long_string[i] = i;
  long_string[kLongStringLen] = 0;

  std::vector<string16> test_cases;
  test_cases.push_back(ASCIIToUTF16(""));
  test_cases.push_back(ASCIIToUTF16("a"));
  test_cases.push_back(ASCIIToUTF16("foo"));
  test_cases.push_back(string16(test_string_a));
  test_cases.push_back(string16(test_string_b));
  test_cases.push_back(string16(long_string));

  for (size_t i = 0; i < test_cases.size(); ++i) {
    string16 s = test_cases[i];
    std::vector<char> v = WrappedEncodeStringWithLength(s);
    string16 res;
    const char* p = DecodeStringWithLength(&*v.begin(), &*v.rbegin() + 1, res);
    EXPECT_EQ(s, res);
    EXPECT_EQ(&*v.rbegin() + 1, p);

    EXPECT_EQ(0, DecodeStringWithLength(&*v.begin(), &*v.rbegin(), res));
    EXPECT_EQ(0, DecodeStringWithLength(&*v.begin(), &*v.begin(), res));
  }
}

static int CompareStrings(const char* p,
                          const char* limit_p,
                          const char* q,
                          const char* limit_q) {
  bool ok;
  int result = CompareEncodedStringsWithLength(p, limit_p, q, limit_q, ok);
  EXPECT_TRUE(ok);
  EXPECT_EQ(p, limit_p);
  EXPECT_EQ(q, limit_q);
  return result;
}

TEST(IndexedDBLevelDBCodingTest, CompareEncodedStringsWithLength) {
  const char16 test_string_a[] = {0x1000, 0x1000, '\0'};
  const char16 test_string_b[] = {0x1000, 0x1000, 0x1000, '\0'};
  const char16 test_string_c[] = {0x1000, 0x1000, 0x1001, '\0'};
  const char16 test_string_d[] = {0x1001, 0x1000, 0x1000, '\0'};
  const char16 test_string_e[] = {0xd834, 0xdd1e, '\0'};
  const char16 test_string_f[] = {0xfffd, '\0'};

  std::vector<string16> test_cases;
  test_cases.push_back(ASCIIToUTF16(""));
  test_cases.push_back(ASCIIToUTF16("a"));
  test_cases.push_back(ASCIIToUTF16("b"));
  test_cases.push_back(ASCIIToUTF16("baaa"));
  test_cases.push_back(ASCIIToUTF16("baab"));
  test_cases.push_back(ASCIIToUTF16("c"));
  test_cases.push_back(string16(test_string_a));
  test_cases.push_back(string16(test_string_b));
  test_cases.push_back(string16(test_string_c));
  test_cases.push_back(string16(test_string_d));
  test_cases.push_back(string16(test_string_e));
  test_cases.push_back(string16(test_string_f));

  for (size_t i = 0; i < test_cases.size() - 1; ++i) {
    string16 a = test_cases[i];
    string16 b = test_cases[i + 1];

    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);
    EXPECT_EQ(b.compare(b), 0);

    std::vector<char> encoded_a = WrappedEncodeStringWithLength(a);
    EXPECT_TRUE(encoded_a.size());
    std::vector<char> encoded_b = WrappedEncodeStringWithLength(b);
    EXPECT_TRUE(encoded_a.size());

    const char* p = &*encoded_a.begin();
    const char* limit_p = &*encoded_a.rbegin() + 1;
    const char* q = &*encoded_b.begin();
    const char* limit_q = &*encoded_b.rbegin() + 1;

    EXPECT_LT(CompareStrings(p, limit_p, q, limit_q), 0);
    EXPECT_GT(CompareStrings(q, limit_q, p, limit_p), 0);
    EXPECT_EQ(CompareStrings(p, limit_p, p, limit_p), 0);
    EXPECT_EQ(CompareStrings(q, limit_q, q, limit_q), 0);
  }
}

static std::vector<char> WrappedEncodeDouble(double value) {
  std::vector<char> buffer;
  EncodeDouble(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeDouble) {
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeDouble(0).size());
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeDouble(3.14).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeDouble) {
  std::vector<char> v;
  const char* p;
  double d;

  v = WrappedEncodeDouble(3.14);
  p = DecodeDouble(&*v.begin(), &*v.rbegin() + 1, &d);
  EXPECT_EQ(3.14, d);
  EXPECT_EQ(&*v.rbegin() + 1, p);

  v = WrappedEncodeDouble(-3.14);
  p = DecodeDouble(&*v.begin(), &*v.rbegin() + 1, &d);
  EXPECT_EQ(-3.14, d);
  EXPECT_EQ(&*v.rbegin() + 1, p);

  v = WrappedEncodeDouble(3.14);
  p = DecodeDouble(&*v.begin(), &*v.rbegin(), &d);
  EXPECT_EQ(0, p);
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKey) {
  IndexedDBKey expected_key;
  scoped_ptr<IndexedDBKey> decoded_key;
  std::vector<char> v;
  const char* p;

  expected_key = IndexedDBKey(1234, WebIDBKey::NumberType);
  v = EncodeIDBKey(expected_key);
  p = DecodeIDBKey(&*v.begin(), &*v.rbegin() + 1, &decoded_key);
  EXPECT_TRUE(decoded_key->IsEqual(expected_key));
  EXPECT_EQ(&*v.rbegin() + 1, p);
  EXPECT_EQ(0, DecodeIDBKey(&*v.begin(), &*v.rbegin(), &decoded_key));

  expected_key = IndexedDBKey(ASCIIToUTF16("Hello World!"));
  v = EncodeIDBKey(expected_key);
  p = DecodeIDBKey(&*v.begin(), &*v.rbegin() + 1, &decoded_key);
  EXPECT_TRUE(decoded_key->IsEqual(expected_key));
  EXPECT_EQ(&*v.rbegin() + 1, p);
  EXPECT_EQ(0, DecodeIDBKey(&*v.begin(), &*v.rbegin(), &decoded_key));

  expected_key = IndexedDBKey(IndexedDBKey::KeyArray());
  v = EncodeIDBKey(expected_key);
  p = DecodeIDBKey(&*v.begin(), &*v.rbegin() + 1, &decoded_key);
  EXPECT_TRUE(decoded_key->IsEqual(expected_key));
  EXPECT_EQ(&*v.rbegin() + 1, p);
  EXPECT_EQ(0, DecodeIDBKey(&*v.begin(), &*v.rbegin(), &decoded_key));

  expected_key = IndexedDBKey(7890, WebIDBKey::DateType);
  v = EncodeIDBKey(expected_key);
  p = DecodeIDBKey(&*v.begin(), &*v.rbegin() + 1, &decoded_key);
  EXPECT_TRUE(decoded_key->IsEqual(expected_key));
  EXPECT_EQ(&*v.rbegin() + 1, p);
  EXPECT_EQ(0, DecodeIDBKey(&*v.begin(), &*v.rbegin(), &decoded_key));

  IndexedDBKey::KeyArray array;
  array.push_back(IndexedDBKey(1234, WebIDBKey::NumberType));
  array.push_back(IndexedDBKey(ASCIIToUTF16("Hello World!")));
  array.push_back(IndexedDBKey(7890, WebIDBKey::DateType));
  expected_key = IndexedDBKey(array);
  v = EncodeIDBKey(expected_key);
  p = DecodeIDBKey(&*v.begin(), &*v.rbegin() + 1, &decoded_key);
  EXPECT_TRUE(decoded_key->IsEqual(expected_key));
  EXPECT_EQ(&*v.rbegin() + 1, p);
  EXPECT_EQ(0, DecodeIDBKey(&*v.begin(), &*v.rbegin(), &decoded_key));
}

static std::vector<char> WrappedEncodeIDBKeyPath(
    const IndexedDBKeyPath& value) {
  std::vector<char> buffer;
  EncodeIDBKeyPath(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeIDBKeyPath) {
  const unsigned char kIDBKeyPathTypeCodedByte1 = 0;
  const unsigned char kIDBKeyPathTypeCodedByte2 = 0;
  {
    IndexedDBKeyPath key_path;
    EXPECT_EQ(key_path.type(), WebIDBKeyPath::NullType);
    std::vector<char> v = WrappedEncodeIDBKeyPath(key_path);
    EXPECT_EQ(v.size(), 3U);
    EXPECT_EQ(v[0], kIDBKeyPathTypeCodedByte1);
    EXPECT_EQ(v[1], kIDBKeyPathTypeCodedByte2);
    EXPECT_EQ(v[2], WebIDBKeyPath::NullType);
  }

  {
    std::vector<string16> test_cases;
    test_cases.push_back(string16());
    test_cases.push_back(ASCIIToUTF16("foo"));
    test_cases.push_back(ASCIIToUTF16("foo.bar"));

    for (size_t i = 0; i < test_cases.size(); ++i) {
      IndexedDBKeyPath key_path = IndexedDBKeyPath(test_cases[i]);
      std::vector<char> v = WrappedEncodeIDBKeyPath(key_path);
      EXPECT_EQ(v.size(),
                WrappedEncodeStringWithLength(test_cases[i]).size() + 3);
      const char* p = &*v.begin();
      const char* limit = &*v.rbegin() + 1;
      EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte1);
      EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte2);
      EXPECT_EQ(*p++, WebIDBKeyPath::StringType);
      string16 string;
      p = DecodeStringWithLength(p, limit, string);
      EXPECT_EQ(string, test_cases[i]);
      EXPECT_EQ(p, limit);
    }
  }

  {
    std::vector<string16> test_case;
    test_case.push_back(string16());
    test_case.push_back(ASCIIToUTF16("foo"));
    test_case.push_back(ASCIIToUTF16("foo.bar"));

    IndexedDBKeyPath key_path(test_case);
    EXPECT_EQ(key_path.type(), WebIDBKeyPath::ArrayType);
    std::vector<char> v = WrappedEncodeIDBKeyPath(key_path);
    const char* p = &*v.begin();
    const char* limit = &*v.rbegin() + 1;
    EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte1);
    EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte2);
    EXPECT_EQ(*p++, WebIDBKeyPath::ArrayType);
    int64 count;
    p = DecodeVarInt(p, limit, count);
    EXPECT_EQ(count, static_cast<int64>(test_case.size()));
    for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
      string16 string;
      p = DecodeStringWithLength(p, limit, string);
      EXPECT_EQ(string, test_case[i]);
    }
    EXPECT_EQ(p, limit);
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeIDBKeyPath) {
  const unsigned char kIDBKeyPathTypeCodedByte1 = 0;
  const unsigned char kIDBKeyPathTypeCodedByte2 = 0;
  const char empty[] = {0};
  {
    // Legacy encoding of string key paths.
    std::vector<string16> test_cases;
    test_cases.push_back(string16());
    test_cases.push_back(ASCIIToUTF16("foo"));
    test_cases.push_back(ASCIIToUTF16("foo.bar"));

    for (size_t i = 0; i < test_cases.size(); ++i) {
      std::vector<char> v = WrappedEncodeString(test_cases[i]);
      const char* begin;
      const char* end;
      if (!v.size()) {
        begin = empty;
        end = empty;
      } else {
        begin = &*v.begin();
        end = &*v.rbegin() + 1;
      }
      IndexedDBKeyPath key_path = DecodeIDBKeyPath(begin, end);
      EXPECT_EQ(key_path.type(), WebIDBKeyPath::StringType);
      EXPECT_EQ(test_cases[i], key_path.string());
    }
  }
  {
    std::vector<char> v;
    v.push_back(kIDBKeyPathTypeCodedByte1);
    v.push_back(kIDBKeyPathTypeCodedByte2);
    v.push_back(WebIDBKeyPath::NullType);
    IndexedDBKeyPath key_path = DecodeIDBKeyPath(&*v.begin(), &*v.rbegin() + 1);
    EXPECT_EQ(key_path.type(), WebIDBKeyPath::NullType);
    EXPECT_TRUE(key_path.IsNull());
  }
  {
    std::vector<string16> test_cases;
    test_cases.push_back(string16());
    test_cases.push_back(ASCIIToUTF16("foo"));
    test_cases.push_back(ASCIIToUTF16("foo.bar"));

    for (size_t i = 0; i < test_cases.size(); ++i) {
      std::vector<char> v;
      v.push_back(kIDBKeyPathTypeCodedByte1);
      v.push_back(kIDBKeyPathTypeCodedByte2);
      v.push_back(WebIDBKeyPath::StringType);
      std::vector<char> test_case =
          WrappedEncodeStringWithLength(test_cases[i]);
      v.insert(v.end(), test_case.begin(), test_case.end());
      IndexedDBKeyPath key_path =
          DecodeIDBKeyPath(&*v.begin(), &*v.rbegin() + 1);
      EXPECT_EQ(key_path.type(), WebIDBKeyPath::StringType);
      EXPECT_EQ(test_cases[i], key_path.string());
    }
  }
  {
    std::vector<string16> test_case;
    test_case.push_back(string16());
    test_case.push_back(ASCIIToUTF16("foo"));
    test_case.push_back(ASCIIToUTF16("foo.bar"));

    std::vector<char> v;
    v.push_back(kIDBKeyPathTypeCodedByte1);
    v.push_back(kIDBKeyPathTypeCodedByte2);
    v.push_back(WebIDBKeyPath::ArrayType);
    EncodeVarInt(test_case.size(), &v);
    for (size_t i = 0; i < test_case.size(); ++i) {
      std::vector<char> test_case_value =
          WrappedEncodeStringWithLength(test_case[i]);
      v.insert(v.end(), test_case_value.begin(), test_case_value.end());
    }
    IndexedDBKeyPath key_path = DecodeIDBKeyPath(&*v.begin(), &*v.rbegin() + 1);
    EXPECT_EQ(key_path.type(), WebIDBKeyPath::ArrayType);
    EXPECT_EQ(key_path.array().size(), test_case.size());
    for (size_t i = 0; i < test_case.size(); ++i)
      EXPECT_EQ(key_path.array()[i], test_case[i]);
  }
}

TEST(IndexedDBLevelDBCodingTest, ExtractAndCompareIDBKeys) {
  std::vector<IndexedDBKey> keys;

  keys.push_back(IndexedDBKey(-10, WebIDBKey::NumberType));
  keys.push_back(IndexedDBKey(0, WebIDBKey::NumberType));
  keys.push_back(IndexedDBKey(3.14, WebIDBKey::NumberType));

  keys.push_back(IndexedDBKey(0, WebIDBKey::DateType));
  keys.push_back(IndexedDBKey(100, WebIDBKey::DateType));
  keys.push_back(IndexedDBKey(100000, WebIDBKey::DateType));

  keys.push_back(IndexedDBKey(ASCIIToUTF16("")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("a")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("b")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("baaa")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("baab")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("c")));

  keys.push_back(CreateArrayIDBKey());
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKey::NumberType)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKey::NumberType),
                                   IndexedDBKey(3.14, WebIDBKey::NumberType)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKey::DateType)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKey::DateType),
                                   IndexedDBKey(0, WebIDBKey::DateType)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16(""))));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16("")),
                                   IndexedDBKey(ASCIIToUTF16("a"))));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey()));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey(), CreateArrayIDBKey()));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey())));
  keys.push_back(CreateArrayIDBKey(
      CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey()))));

  for (size_t i = 0; i < keys.size() - 1; ++i) {
    const IndexedDBKey& key_a = keys[i];
    const IndexedDBKey& key_b = keys[i + 1];

    EXPECT_TRUE(key_a.IsLessThan(key_b));

    std::vector<char> encoded_a = EncodeIDBKey(key_a);
    EXPECT_TRUE(encoded_a.size());
    std::vector<char> encoded_b = EncodeIDBKey(key_b);
    EXPECT_TRUE(encoded_b.size());

    std::vector<char> extracted_a;
    std::vector<char> extracted_b;

    const char* p = ExtractEncodedIDBKey(
        &*encoded_a.begin(), &*encoded_a.rbegin() + 1, &extracted_a);
    EXPECT_EQ(&*encoded_a.rbegin() + 1, p);
    EXPECT_EQ(encoded_a, extracted_a);

    const char* q = ExtractEncodedIDBKey(
        &*encoded_b.begin(), &*encoded_b.rbegin() + 1, &extracted_b);
    EXPECT_EQ(&*encoded_b.rbegin() + 1, q);
    EXPECT_EQ(encoded_b, extracted_b);

    EXPECT_LT(CompareKeys(extracted_a, extracted_b), 0);
    EXPECT_GT(CompareKeys(extracted_b, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_a, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_b, extracted_b), 0);

    EXPECT_EQ(0,
              ExtractEncodedIDBKey(
                  &*encoded_a.begin(), &*encoded_a.rbegin(), &extracted_a));
  }
}

TEST(IndexedDBLevelDBCodingTest, ComparisonTest) {
  std::vector<std::vector<char> > keys;
  keys.push_back(SchemaVersionKey::Encode());
  keys.push_back(MaxDatabaseIdKey::Encode());
  keys.push_back(DatabaseFreeListKey::Encode(0));
  keys.push_back(DatabaseFreeListKey::EncodeMaxKey());
  keys.push_back(DatabaseNameKey::Encode(ASCIIToUTF16(""), ASCIIToUTF16("")));
  keys.push_back(DatabaseNameKey::Encode(ASCIIToUTF16(""), ASCIIToUTF16("a")));
  keys.push_back(DatabaseNameKey::Encode(ASCIIToUTF16("a"), ASCIIToUTF16("a")));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::ORIGIN_NAME));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::DATABASE_NAME));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_VERSION));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_INT_VERSION));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::NAME));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::KEY_PATH));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::AUTO_INCREMENT));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::EVICTABLE));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::LAST_VERSION));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::MAX_INDEX_ID));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::HAS_KEY_PATH));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1, 1));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::NAME));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::UNIQUE));
  keys.push_back(
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::KEY_PATH));
  keys.push_back(
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::MULTI_ENTRY));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 31, 0));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 31, 1));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1, 31));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1, 32));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreFreeListKey::Encode(1, 1));
  keys.push_back(ObjectStoreFreeListKey::EncodeMaxKey(1));
  keys.push_back(IndexFreeListKey::Encode(1, 1, kMinimumIndexId));
  keys.push_back(IndexFreeListKey::EncodeMaxKey(1, 1));
  keys.push_back(IndexFreeListKey::Encode(1, 2, kMinimumIndexId));
  keys.push_back(IndexFreeListKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("")));
  keys.push_back(ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("a")));
  keys.push_back(IndexNamesKey::Encode(1, 1, ASCIIToUTF16("")));
  keys.push_back(IndexNamesKey::Encode(1, 1, ASCIIToUTF16("a")));
  keys.push_back(IndexNamesKey::Encode(1, 2, ASCIIToUTF16("a")));
  keys.push_back(ObjectStoreDataKey::Encode(1, 1, MinIDBKey()));
  keys.push_back(ObjectStoreDataKey::Encode(1, 1, MaxIDBKey()));
  keys.push_back(ExistsEntryKey::Encode(1, 1, MinIDBKey()));
  keys.push_back(ExistsEntryKey::Encode(1, 1, MaxIDBKey()));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 31, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 2, 30, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(
      IndexDataKey::EncodeMaxKey(1, 2, std::numeric_limits<int32>::max() - 1));

  for (size_t i = 0; i < keys.size(); ++i) {
    const LevelDBSlice key_a(keys[i]);
    EXPECT_EQ(Compare(key_a, key_a), 0);

    for (size_t j = i + 1; j < keys.size(); ++j) {
      const LevelDBSlice key_b(keys[j]);
      EXPECT_LT(Compare(key_a, key_b), 0);
      EXPECT_GT(Compare(key_b, key_a), 0);
    }
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeVarIntVSEncodeByteTest) {
  std::vector<unsigned char> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(127);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];

    std::vector<char> vA = WrappedEncodeByte(n);
    std::vector<char> vB = WrappedEncodeVarInt(static_cast<int64>(n));

    EXPECT_EQ(vA.size(), vB.size());
    EXPECT_EQ(*vA.begin(), *vB.begin());
  }
}

}  // namespace

}  // namespace content
