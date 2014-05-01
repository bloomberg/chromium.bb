// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

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

bool ReadDataFile(const std::string& path, std::vector<uint8_t>* result) {
  FILE* fp = OpenSourceRootRelativeFile(path.c_str());
  if (!fp) {
    ADD_FAILURE() << "File not found: " << path;
    return false;
  }
  for (;;) {
    unsigned int value;
    int rv = fscanf(fp, "%x", &value);
    if (rv != 1)
      break;
    result->push_back(static_cast<uint8_t>(value & 0xFF));
  }
  bool error = ferror(fp);
  fclose(fp);
  return !error;
}

bool ReadResultFile(const std::string& path, std::string* result) {
  FILE* fp = OpenSourceRootRelativeFile(path.c_str());
  if (!fp)
    return false;
  fseek(fp, 0, SEEK_END);
  size_t size = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);
  result->resize(size);
  size_t size_read = fread(&result->at(0), 1, size, fp);
  fclose(fp);
  if (size != size_read)
    return false;
  // Result files are new-line delimited text files. Remove any CRs.
  result->erase(std::remove(result->begin(), result->end(), '\r'),
                result->end());
  return true;
}

std::string GetPath(const std::string& root, const std::string& suffix) {
  return "mojo/public/interfaces/bindings/tests/data/" + root + suffix;
}

void RunValidationTest(const std::string& root, std::string (*func)(Message*)) {
  std::vector<uint8_t> data;
  ASSERT_TRUE(ReadDataFile(GetPath(root, ".data"), &data));

  std::string expected;
  ASSERT_TRUE(ReadResultFile(GetPath(root, ".expected"), &expected));

  Message message;
  message.AllocUninitializedData(static_cast<uint32_t>(data.size()));
  memcpy(message.mutable_data(), &data[0], data.size());

  std::string result = func(&message);
  EXPECT_EQ(expected, result) << "failed test: " << root;
}

class DummyMessageReceiver : public MessageReceiver {
 public:
  virtual bool Accept(Message* message) MOJO_OVERRIDE {
    return true;  // Any message is OK.
  }
  virtual bool AcceptWithResponder(Message* message,
                                   MessageReceiver* responder) MOJO_OVERRIDE {
    assert(false);
    return false;
  }
};

std::string DumpMessageHeader(Message* message) {
  DummyMessageReceiver not_reached_receiver;
  internal::MessageHeaderValidator validator(&not_reached_receiver);
  bool rv = validator.Accept(message);
  if (!rv)
    return "ERROR\n";

  std::ostringstream os;
  os << "num_bytes: " << message->header()->num_bytes << "\n"
     << "num_fields: " << message->header()->num_fields << "\n"
     << "name: " << message->header()->name << "\n"
     << "flags: " << message->header()->flags << "\n";
  return os.str();
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
