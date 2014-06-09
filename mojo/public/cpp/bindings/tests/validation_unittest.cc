// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

template <typename T>
void Append(std::vector<uint8_t>* data_vector, T data) {
  size_t pos = data_vector->size();
  data_vector->resize(pos + sizeof(T));
  memcpy(&(*data_vector)[pos], &data, sizeof(T));
}

bool TestInputParser(const std::string& input,
                     bool expected_result,
                     const std::vector<uint8_t>& expected_parsed_input) {
  std::vector<uint8_t> parsed_input;
  std::string error_message;

  bool result = ParseValidationTestInput(input, &parsed_input, &error_message);
  if (expected_result) {
    if (result && error_message.empty() &&
        expected_parsed_input == parsed_input) {
      return true;
    }

    // Compare with an empty string instead of checking |error_message.empty()|,
    // so that the message will be printed out if the two are not equal.
    EXPECT_EQ(std::string(), error_message);
    EXPECT_EQ(expected_parsed_input, parsed_input);
    return false;
  }

  EXPECT_FALSE(error_message.empty());
  return !result && !error_message.empty();
}

std::vector<std::string> GetMatchingTests(const std::vector<std::string>& names,
                                          const std::string& prefix) {
  const std::string suffix = ".data";
  std::vector<std::string> tests;
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i].size() >= suffix.size() &&
        names[i].substr(0, prefix.size()) == prefix &&
        names[i].substr(names[i].size() - suffix.size()) == suffix)
      tests.push_back(names[i].substr(0, names[i].size() - suffix.size()));
  }
  return tests;
}

bool ReadFile(const std::string& path, std::string* result) {
  FILE* fp = OpenSourceRootRelativeFile(path.c_str());
  if (!fp) {
    ADD_FAILURE() << "File not found: " << path;
    return false;
  }
  fseek(fp, 0, SEEK_END);
  size_t size = static_cast<size_t>(ftell(fp));
  if (size == 0) {
    result->clear();
    fclose(fp);
    return true;
  }
  fseek(fp, 0, SEEK_SET);
  result->resize(size);
  size_t size_read = fread(&result->at(0), 1, size, fp);
  fclose(fp);
  return size == size_read;
}

bool ReadAndParseDataFile(const std::string& path, std::vector<uint8_t>* data) {
  std::string input;
  if (!ReadFile(path, &input))
    return false;

  std::string error_message;
  if (!ParseValidationTestInput(input, data, &error_message)) {
    ADD_FAILURE() << error_message;
    return false;
  }

  return true;
}

bool ReadResultFile(const std::string& path, std::string* result) {
  if (!ReadFile(path, result))
    return false;

  // Result files are new-line delimited text files. Remove any CRs.
  result->erase(std::remove(result->begin(), result->end(), '\r'),
                result->end());

  // Remove trailing LFs.
  size_t pos = result->find_last_not_of('\n');
  if (pos == std::string::npos)
    result->clear();
  else
    result->resize(pos + 1);

  return true;
}

std::string GetPath(const std::string& root, const std::string& suffix) {
  return "mojo/public/interfaces/bindings/tests/data/" + root + suffix;
}

void RunValidationTest(const std::string& root, std::string (*func)(Message*)) {
  std::vector<uint8_t> data;
  ASSERT_TRUE(ReadAndParseDataFile(GetPath(root, ".data"), &data));

  std::string expected;
  ASSERT_TRUE(ReadResultFile(GetPath(root, ".expected"), &expected));

  Message message;
  message.AllocUninitializedData(static_cast<uint32_t>(data.size()));
  if (!data.empty())
    memcpy(message.mutable_data(), &data[0], data.size());

  std::string result = func(&message);
  EXPECT_EQ(expected, result) << "failed test: " << root;
}

class DummyMessageReceiver : public MessageReceiver {
 public:
  virtual bool Accept(Message* message) MOJO_OVERRIDE {
    return true;  // Any message is OK.
  }
};

std::string DumpMessageHeader(Message* message) {
  internal::ValidationErrorObserverForTesting observer;
  DummyMessageReceiver not_reached_receiver;
  internal::MessageHeaderValidator validator(&not_reached_receiver);
  bool rv = validator.Accept(message);
  if (!rv) {
    EXPECT_NE(internal::VALIDATION_ERROR_NONE, observer.last_error());
    return internal::ValidationErrorToString(observer.last_error());
  }

  std::ostringstream os;
  os << "num_bytes: " << message->header()->num_bytes << "\n"
     << "num_fields: " << message->header()->num_fields << "\n"
     << "name: " << message->header()->name << "\n"
     << "flags: " << message->header()->flags;
  return os.str();
}

TEST(ValidationTest, InputParser) {
  {
    // The parser, as well as Append() defined above, assumes that this code is
    // running on a little-endian platform. Test whether that is true.
    uint16_t x = 1;
    ASSERT_EQ(1, *(reinterpret_cast<char*>(&x)));
  }
  {
    // Test empty input.
    std::string input;
    std::vector<uint8_t> expected;

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    // Test input that only consists of comments and whitespaces.
    std::string input = "    \t  // hello world \n\r \t// the answer is 42   ";
    std::vector<uint8_t> expected;

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    std::string input = "[u1]0x10// hello world !! \n\r  \t [u2]65535 \n"
                        "[u4]65536 [u8]0xFFFFFFFFFFFFFFFF 0 0Xff";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint8_t>(0x10));
    Append(&expected, static_cast<uint16_t>(65535));
    Append(&expected, static_cast<uint32_t>(65536));
    Append(&expected, static_cast<uint64_t>(0xffffffffffffffff));
    Append(&expected, static_cast<uint8_t>(0));
    Append(&expected, static_cast<uint8_t>(0xff));

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    std::string input = "[s8]-0x800 [s1]-128\t[s2]+0 [s4]-40";
    std::vector<uint8_t> expected;
    Append(&expected, -static_cast<int64_t>(0x800));
    Append(&expected, static_cast<int8_t>(-128));
    Append(&expected, static_cast<int16_t>(0));
    Append(&expected, static_cast<int32_t>(-40));

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    std::string input = "[b]00001011 [b]10000000  // hello world\r [b]00000000";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint8_t>(11));
    Append(&expected, static_cast<uint8_t>(128));
    Append(&expected, static_cast<uint8_t>(0));

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    std::string input = "[f]+.3e9 [d]-10.03";
    std::vector<uint8_t> expected;
    Append(&expected, +.3e9f);
    Append(&expected, -10.03);

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }
  {
    std::string input = "[dist4]foo 0 [dist8]bar 0 [anchr]foo [anchr]bar";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint32_t>(14));
    Append(&expected, static_cast<uint8_t>(0));
    Append(&expected, static_cast<uint64_t>(9));
    Append(&expected, static_cast<uint8_t>(0));

    EXPECT_TRUE(TestInputParser(input, true, expected));
  }

  // Test some failure cases.
  {
    const char* error_inputs[] = {
      "/ hello world",
      "[u1]x",
      "[u1]0x100",
      "[s2]-0x8001",
      "[b]1",
      "[b]1111111k",
      "[dist4]unmatched",
      "[anchr]hello [dist8]hello",
      NULL
    };

    for (size_t i = 0; error_inputs[i]; ++i) {
      std::vector<uint8_t> expected;
      if (!TestInputParser(error_inputs[i], false, expected))
        ADD_FAILURE() << "Unexpected test result for: " << error_inputs[i];
    }
  }
}

TEST(ValidationTest, TestAll) {
  std::vector<std::string> names =
      EnumerateSourceRootRelativeDirectory(GetPath("", ""));

  std::vector<std::string> header_tests =
      GetMatchingTests(names, "validate_header_");

  for (size_t i = 0; i < header_tests.size(); ++i)
    RunValidationTest(header_tests[i], &DumpMessageHeader);
}

}  // namespace
}  // namespace test
}  // namespace mojo
