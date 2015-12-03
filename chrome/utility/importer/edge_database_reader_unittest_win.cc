// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/edge_database_reader_win.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "components/compression/compression_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class EdgeDatabaseReaderTest : public ::testing::Test {
 protected:
  bool CopyTestDatabase(const base::string16& database_name,
                        base::FilePath* copied_path) {
    base::FilePath input_path;
    input_path = test_data_path_.AppendASCII("edge_database_reader")
                     .Append(database_name)
                     .AddExtension(L".gz");
    base::FilePath output_path = temp_dir_.path().Append(database_name);

    if (DecompressDatabase(input_path, output_path)) {
      *copied_path = output_path;
      return true;
    }
    return false;
  }

  bool WriteFile(const base::string16& name,
                 const std::string& contents,
                 base::FilePath* output_path) {
    *output_path = temp_dir_.path().Append(name);
    return base::WriteFile(*output_path, contents.c_str(), contents.size()) >=
           0;
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_path_));
  }

 private:
  bool DecompressDatabase(const base::FilePath& gzip_file,
                          const base::FilePath& output_file) {
    std::string gzip_data;
    if (!base::ReadFileToString(gzip_file, &gzip_data))
      return false;
    if (!compression::GzipUncompress(gzip_data, &gzip_data))
      return false;
    return base::WriteFile(output_file, gzip_data.c_str(), gzip_data.size()) >=
           0;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_data_path_;
};

}  // namespace

TEST_F(EdgeDatabaseReaderTest, OpenFileTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
}

TEST_F(EdgeDatabaseReaderTest, NoFileTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(L"ThisIsntARealFileName.edb"));
}

TEST_F(EdgeDatabaseReaderTest, RandomGarbageDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"random.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path.value()));
}

TEST_F(EdgeDatabaseReaderTest, ZerosDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  std::string zeros(0x10000, '\0');
  ASSERT_TRUE(WriteFile(L"zeros.edb", zeros, &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path.value()));
}

TEST_F(EdgeDatabaseReaderTest, EmptyDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(WriteFile(L"empty.edb", "", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path.value()));
}

TEST_F(EdgeDatabaseReaderTest, OpenTableDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(table_enum, nullptr);
}

TEST_F(EdgeDatabaseReaderTest, InvalidTableDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NotARealTableName");
  EXPECT_EQ(table_enum, nullptr);
}

TEST_F(EdgeDatabaseReaderTest, NotOpenDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  EdgeDatabaseReader reader;
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_EQ(table_enum, nullptr);
  EXPECT_EQ(reader.last_error(), JET_errDatabaseNotFound);
}

TEST_F(EdgeDatabaseReaderTest, AlreadyOpenDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  EXPECT_FALSE(reader.OpenDatabase(database_path.value()));
  EXPECT_EQ(reader.last_error(), JET_errOneDatabasePerSession);
}

TEST_F(EdgeDatabaseReaderTest, OpenTableAndReadDataDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(table_enum, nullptr);
  int row_count = 0;
  do {
    int32_t int_col_value = 0;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
    EXPECT_EQ(int_col_value, -row_count);

    uint32_t uint_col_value = 0;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"UIntCol", &uint_col_value));
    EXPECT_EQ(uint_col_value, row_count);

    int64_t longlong_col_value = 0;
    EXPECT_TRUE(
        table_enum->RetrieveColumn(L"LongLongCol", &longlong_col_value));
    EXPECT_EQ(longlong_col_value, row_count);

    GUID guid_col_value = {};
    GUID expected_guid_col_value = {};
    EXPECT_TRUE(table_enum->RetrieveColumn(L"GuidCol", &guid_col_value));
    memset(&expected_guid_col_value, row_count,
           sizeof(expected_guid_col_value));
    EXPECT_EQ(guid_col_value, expected_guid_col_value);

    FILETIME filetime_col_value = {};
    FILETIME expected_filetime_col_value = {};
    SYSTEMTIME system_time = {};
    // Expected time value is row_count+1/January/1970.
    system_time.wYear = 1970;
    system_time.wMonth = 1;
    system_time.wDay = row_count + 1;
    EXPECT_TRUE(
        SystemTimeToFileTime(&system_time, &expected_filetime_col_value));
    EXPECT_TRUE(table_enum->RetrieveColumn(L"DateCol", &filetime_col_value));
    EXPECT_EQ(filetime_col_value.dwLowDateTime,
              expected_filetime_col_value.dwLowDateTime);
    EXPECT_EQ(filetime_col_value.dwHighDateTime,
              expected_filetime_col_value.dwHighDateTime);

    std::wstring row_string = base::StringPrintf(L"String: %d", row_count);
    base::string16 str_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
    EXPECT_EQ(str_col_value, row_string);

    bool bool_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"BoolCol", &bool_col_value));
    EXPECT_EQ(bool_col_value, (row_count % 2) == 0);

    row_count++;
  } while (table_enum->Next());
  EXPECT_EQ(row_count, 16);
}

TEST_F(EdgeDatabaseReaderTest, CheckEnumResetDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(table_enum, nullptr);
  int row_count = 0;
  do {
    row_count++;
  } while (table_enum->Next());
  EXPECT_NE(row_count, 0);
  EXPECT_TRUE(table_enum->Reset());
  do {
    row_count--;
  } while (table_enum->Next());
  EXPECT_EQ(row_count, 0);
}

TEST_F(EdgeDatabaseReaderTest, InvalidColumnDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(table_enum, nullptr);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"NotARealNameCol", &int_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errColumnNotFound);
}

TEST_F(EdgeDatabaseReaderTest, NoColumnDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NoColsTable");
  EXPECT_NE(table_enum, nullptr);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errColumnNotFound);
}

TEST_F(EdgeDatabaseReaderTest, EmptyTableDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"EmptyTable");
  EXPECT_NE(table_enum, nullptr);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_NE(table_enum->last_error(), JET_errColumnNotFound);
  EXPECT_FALSE(table_enum->Reset());
  EXPECT_FALSE(table_enum->Next());
}

TEST_F(EdgeDatabaseReaderTest, UnicodeStringsDatabaseTest) {
  const char* utf8_strings[] = {
      "\xE3\x81\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF",
      "\xE4\xBD\xA0\xE5\xA5\xBD",
      "\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD1\x81\xD1\x82\xD0\xB2\xD1\x83"
      "\xD0\xB9\xD1\x82\xD0\xB5",
      "\x48\x65\x6C\x6C\x6F",
      "\xEC\x95\x88\xEB\x85\x95\xED\x95\x98\xEC\x84\xB8\xEC\x9A\x94",
  };
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"UnicodeTable");
  EXPECT_NE(table_enum, nullptr);
  size_t utf8_strings_count = arraysize(utf8_strings);
  for (size_t row_count = 0; row_count < utf8_strings_count; ++row_count) {
    std::wstring row_string = base::UTF8ToWide(utf8_strings[row_count]);
    base::string16 str_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
    EXPECT_EQ(str_col_value, row_string);
    if (row_count < utf8_strings_count - 1)
      EXPECT_TRUE(table_enum->Next());
    else
      EXPECT_FALSE(table_enum->Next());
  }
}

TEST_F(EdgeDatabaseReaderTest, NonUnicodeStringsDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NonUnicodeTable");
  EXPECT_NE(table_enum, nullptr);
  base::string16 str_col_value;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
}

TEST_F(EdgeDatabaseReaderTest, CheckNullColumnDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NullTable");
  EXPECT_NE(table_enum, nullptr);

  // We expect to successfully open a column value but get the default value
  // back.
  int32_t int_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_EQ(int_col_value, 0);

  uint32_t uint_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"UIntCol", &uint_col_value));
  EXPECT_EQ(uint_col_value, 0);

  int64_t longlong_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"LongLongCol", &longlong_col_value));
  EXPECT_EQ(longlong_col_value, 0);

  GUID guid_col_value = {};
  GUID expected_guid_col_value = {};
  memset(&guid_col_value, 0x1, sizeof(guid_col_value));
  EXPECT_TRUE(table_enum->RetrieveColumn(L"GuidCol", &guid_col_value));
  memset(&expected_guid_col_value, 0, sizeof(expected_guid_col_value));
  EXPECT_EQ(guid_col_value, expected_guid_col_value);

  FILETIME filetime_col_value = {1, 1};
  FILETIME expected_filetime_col_value = {};
  EXPECT_TRUE(table_enum->RetrieveColumn(L"DateCol", &filetime_col_value));
  EXPECT_EQ(filetime_col_value.dwLowDateTime,
            expected_filetime_col_value.dwLowDateTime);
  EXPECT_EQ(filetime_col_value.dwHighDateTime,
            expected_filetime_col_value.dwHighDateTime);

  base::string16 str_col_value;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
  EXPECT_TRUE(str_col_value.empty());

  bool bool_col_value;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"BoolCol", &bool_col_value));
  EXPECT_EQ(bool_col_value, false);
}

TEST_F(EdgeDatabaseReaderTest, CheckInvalidColumnTypeDatabaseTest) {
  // Only verified to work with ESE library on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path.value()));
  scoped_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(table_enum, nullptr);

  uint32_t uint_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  // Check unsigned int with a signed int.
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"UIntCol", &int_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  EXPECT_FALSE(table_enum->RetrieveColumn(L"LongLongCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  EXPECT_FALSE(table_enum->RetrieveColumn(L"GuidCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  EXPECT_FALSE(table_enum->RetrieveColumn(L"DateCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  EXPECT_FALSE(table_enum->RetrieveColumn(L"StrCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
  EXPECT_FALSE(table_enum->RetrieveColumn(L"BoolCol", &uint_col_value));
  EXPECT_EQ(table_enum->last_error(), JET_errInvalidColumnType);
}
