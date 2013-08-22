// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/test/test_launcher.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace base {

bool ProcessGTestOutput(const base::FilePath& output_file,
                        std::vector<TestResult>* results) {
  DCHECK(results);

  std::string xml_contents;
  if (!file_util::ReadFileToString(output_file, &xml_contents))
    return false;

  XmlReader xml_reader;
  if (!xml_reader.Load(xml_contents))
    return false;

  enum {
    STATE_INIT,
    STATE_TESTSUITE,
    STATE_TESTCASE,
    STATE_FAILURE,
    STATE_END,
  } state = STATE_INIT;

  while (xml_reader.Read()) {
    xml_reader.SkipToElement();
    std::string node_name(xml_reader.NodeName());

    switch (state) {
      case STATE_INIT:
        if (node_name == "testsuites" && !xml_reader.IsClosingElement())
          state = STATE_TESTSUITE;
        else
          return false;
        break;
      case STATE_TESTSUITE:
        if (node_name == "testsuites" && xml_reader.IsClosingElement())
          state = STATE_END;
        else if (node_name == "testsuite" && !xml_reader.IsClosingElement())
          state = STATE_TESTCASE;
        else
          return false;
        break;
      case STATE_TESTCASE:
        if (node_name == "testsuite" && xml_reader.IsClosingElement()) {
          state = STATE_TESTSUITE;
        } else if (node_name == "testcase" && !xml_reader.IsClosingElement()) {
          std::string test_status;
          if (!xml_reader.NodeAttribute("status", &test_status))
            return false;

          if (test_status != "run" && test_status != "notrun")
            return false;
          if (test_status != "run")
            break;

          TestResult result;
          if (!xml_reader.NodeAttribute("classname", &result.test_case_name))
            return false;
          if (!xml_reader.NodeAttribute("name", &result.test_name))
            return false;

          std::string test_time_str;
          if (!xml_reader.NodeAttribute("time", &test_time_str))
            return false;
          result.elapsed_time =
              TimeDelta::FromMicroseconds(strtod(test_time_str.c_str(), NULL) *
                                          Time::kMicrosecondsPerSecond);

          result.success = true;

          results->push_back(result);
        } else if (node_name == "failure" && !xml_reader.IsClosingElement()) {
          std::string failure_message;
          if (!xml_reader.NodeAttribute("message", &failure_message))
            return false;

          DCHECK(!results->empty());
          results->at(results->size() - 1).success = false;

          state = STATE_FAILURE;
        } else if (node_name == "testcase" && xml_reader.IsClosingElement()) {
          // Deliberately empty.
        } else {
          return false;
        }
        break;
      case STATE_FAILURE:
        if (node_name == "failure" && xml_reader.IsClosingElement())
          state = STATE_TESTCASE;
        else
          return false;
        break;
      case STATE_END:
        // If we are here and there are still XML elements, the file has wrong
        // format.
        return false;
    }
  }

  return true;
}

}  // namespace base