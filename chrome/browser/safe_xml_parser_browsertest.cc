// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_service_manager_listener.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/data_decoder/public/interfaces/xml_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

class SafeXmlParserTest : public InProcessBrowserTest {
 public:
  SafeXmlParserTest() = default;
  ~SafeXmlParserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    listener_.Init();

    // The data_decoder service will stop if no connection is bound to it after
    // 5 seconds. We bind a connection to it for the duration of the test so it
    // is guaranteed the service is always running.
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(data_decoder::mojom::kServiceName, &xml_parser_ptr_);
    listener_.WaitUntilServiceStarted(data_decoder::mojom::kServiceName);
    EXPECT_EQ(
        1U, listener_.GetServiceStartCount(data_decoder::mojom::kServiceName));
  }

  uint32_t GetServiceStartCount(const std::string& service_name) const {
    return listener_.GetServiceStartCount(service_name);
  }

  // Parses |xml| and compares its parsed representation with |expected_json|.
  // If |expected_json| is empty, the XML parsing is expected to fail.
  void TestParse(base::StringPiece xml, base::StringPiece expected_json) {
    SCOPED_TRACE(xml);

    base::RunLoop run_loop;
    std::unique_ptr<base::Value> expected_value;
    if (!expected_json.empty()) {
      expected_value = base::JSONReader::Read(expected_json);
      DCHECK(expected_value) << "Bad test, incorrect JSON: " << expected_json;
    }

    data_decoder::ParseXml(
        content::ServiceManagerConnection::GetForProcess()->GetConnector(),
        xml.as_string(),
        base::BindOnce(&SafeXmlParserTest::XmlParsingDone,
                       base::Unretained(this), run_loop.QuitClosure(),
                       std::move(expected_value)));
    run_loop.Run();
  }

 private:
  void XmlParsingDone(base::Closure quit_loop_closure,
                      std::unique_ptr<base::Value> expected_value,
                      std::unique_ptr<base::Value> actual_value,
                      const base::Optional<std::string>& error) {
    base::ScopedClosureRunner(std::move(quit_loop_closure));
    if (!expected_value) {
      EXPECT_FALSE(actual_value);
      EXPECT_TRUE(error);
      return;
    }
    EXPECT_FALSE(error);
    ASSERT_TRUE(actual_value);
    EXPECT_EQ(*expected_value, *actual_value);
  }

  data_decoder::mojom::XmlParserPtr xml_parser_ptr_;
  TestServiceManagerListener listener_;

  DISALLOW_COPY_AND_ASSIGN(SafeXmlParserTest);
};

}  // namespace

// Tests that SafeXmlParser does parse. (actual XML parsing is tested in the
// service unit-tests).
IN_PROC_BROWSER_TEST_F(SafeXmlParserTest, Parse) {
  TestParse("[\"this is JSON not XML\"]", "");
  TestParse("<hello>bonjour</hello>",
            R"(
              {"type": "element",
               "tag": "hello",
               "children": [{"type": "text", "text": "bonjour"}]
               } )");
}

// Tests that when calling SafeXmlParser::Parse() a new service is started
// every time.
IN_PROC_BROWSER_TEST_F(SafeXmlParserTest, Isolation) {
  for (int i = 0; i < 5; i++) {
    base::RunLoop run_loop;
    bool parsing_success = false;
    data_decoder::ParseXml(
        content::ServiceManagerConnection::GetForProcess()->GetConnector(),
        "<hello>bonjour</hello>",
        base::BindOnce(
            [](bool* success, base::Closure quit_loop_closure,
               std::unique_ptr<base::Value> actual_value,
               const base::Optional<std::string>& error) {
              *success = !error;
              std::move(quit_loop_closure).Run();
            },
            &parsing_success, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(parsing_success);
    // 2 + i below because the data_decoder is already running and the index
    // starts at 0.
    EXPECT_EQ(2U + i, GetServiceStartCount(data_decoder::mojom::kServiceName));
  }
}
