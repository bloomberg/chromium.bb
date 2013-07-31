// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/content/browser/autocheckout_page_meta_data.h"
#include "components/autofill/core/browser/autofill_xml_parser.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlparser.h"

namespace autofill {
namespace {

class AutofillQueryXmlParserTest : public testing::Test {
 public:
  AutofillQueryXmlParserTest(): upload_required_(USE_UPLOAD_RATES) {};
  virtual ~AutofillQueryXmlParserTest() {};

 protected:
  void ParseQueryXML(const std::string& xml, bool should_succeed) {
    // Create a parser.
    AutofillQueryXmlParser parse_handler(&field_infos_,
                                         &upload_required_,
                                         &experiment_id_,
                                         &page_meta_data_);
    buzz::XmlParser parser(&parse_handler);
    parser.Parse(xml.c_str(), xml.length(), true);
    EXPECT_EQ(should_succeed, parse_handler.succeeded());
  }

  std::vector<AutofillServerFieldInfo> field_infos_;
  UploadRequired upload_required_;
  std::string experiment_id_;
  autofill::AutocheckoutPageMetaData page_meta_data_;
};

class AutofillUploadXmlParserTest : public testing::Test {
 public:
  AutofillUploadXmlParserTest(): positive_(0), negative_(0) {};
  virtual ~AutofillUploadXmlParserTest() {};

 protected:
  void ParseUploadXML(const std::string& xml, bool should_succeed) {
    // Create a parser.
    AutofillUploadXmlParser parse_handler(&positive_, &negative_);
    buzz::XmlParser parser(&parse_handler);
    parser.Parse(xml.c_str(), xml.length(), true);

    EXPECT_EQ(should_succeed, parse_handler.succeeded());
  }

  double positive_;
  double negative_;
};

TEST_F(AutofillQueryXmlParserTest, BasicQuery) {
  // An XML string representing a basic query response.
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"0\" />"
                    "<field autofilltype=\"1\" />"
                    "<field autofilltype=\"3\" />"
                    "<field autofilltype=\"2\" />"
                    "<field autofilltype=\"61\" defaultvalue=\"default\"/>"
                    "</autofillqueryresponse>";
  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(5U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_EQ(UNKNOWN_TYPE, field_infos_[1].field_type);
  EXPECT_EQ(NAME_FIRST, field_infos_[2].field_type);
  EXPECT_EQ(EMPTY_TYPE, field_infos_[3].field_type);
  EXPECT_TRUE(field_infos_[3].default_value.empty());
  EXPECT_EQ(FIELD_WITH_DEFAULT_VALUE, field_infos_[4].field_type);
  EXPECT_EQ("default", field_infos_[4].default_value);
  EXPECT_TRUE(experiment_id_.empty());
}

// Test parsing the upload required attribute.
TEST_F(AutofillQueryXmlParserTest, TestUploadRequired) {
  std::string xml = "<autofillqueryresponse uploadrequired=\"true\">"
                    "<field autofilltype=\"0\" />"
                    "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(upload_required_, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());

  field_infos_.clear();
  xml = "<autofillqueryresponse uploadrequired=\"false\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(UPLOAD_NOT_REQUIRED, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());

  field_infos_.clear();
  xml = "<autofillqueryresponse uploadrequired=\"bad_value\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());
}

// Test parsing the experiment id attribute
TEST_F(AutofillQueryXmlParserTest, ParseExperimentId) {
  // When the attribute is missing, we should get back the default value -- the
  // empty string.
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"0\" />"
                    "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());

  field_infos_.clear();

  // When the attribute is present, make sure we parse it.
  xml = "<autofillqueryresponse experimentid=\"FancyNewAlgorithm\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_EQ(std::string("FancyNewAlgorithm"), experiment_id_);

  field_infos_.clear();

  // Make sure that we can handle parsing both the upload required and the
  // experiment id attribute together.
  xml = "<autofillqueryresponse uploadrequired=\"false\""
        "                       experimentid=\"ServerSmartyPants\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(UPLOAD_NOT_REQUIRED, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_EQ("ServerSmartyPants", experiment_id_);
}

// Fails on ASAN bot. http://crbug.com/253797
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ParseAutofillFlow DISABLED_ParseAutofillFlow
#else
#define MAYBE_ParseAutofillFlow ParseAutofillFlow
#endif

// Test XML response with autofill_flow information.
TEST_F(AutofillQueryXmlParserTest, MAYBE_ParseAutofillFlow) {
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"55\"/>"
                    "<autofill_flow page_no=\"1\" total_pages=\"10\">"
                    "<page_advance_button id=\"foo\"/>"
                    "<flow_page page_no=\"0\">"
                    "<type>1</type>"
                    "<type>2</type>"
                    "</flow_page>"
                    "<flow_page page_no=\"1\">"
                    "<type>3</type>"
                    "</flow_page>"
                    "</autofill_flow>"
                    "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  EXPECT_TRUE(page_meta_data_.ignore_ajax);
  EXPECT_EQ("foo", page_meta_data_.proceed_element_descriptor.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::ID,
            page_meta_data_.proceed_element_descriptor.retrieval_method);
  EXPECT_EQ(2U, page_meta_data_.page_types.size());
  EXPECT_EQ(2U, page_meta_data_.page_types[0].size());
  EXPECT_EQ(1U, page_meta_data_.page_types[1].size());
  EXPECT_EQ(AUTOCHECKOUT_STEP_SHIPPING, page_meta_data_.page_types[0][0]);
  EXPECT_EQ(AUTOCHECKOUT_STEP_DELIVERY, page_meta_data_.page_types[0][1]);
  EXPECT_EQ(AUTOCHECKOUT_STEP_BILLING, page_meta_data_.page_types[1][0]);

  // Clear |field_infos_| for the next test;
  field_infos_.clear();

  // Test css_selector as page_advance_button.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\">"
        "<page_advance_button css_selector=\"[name=&quot;foo&quot;]\"/>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  EXPECT_EQ("[name=\"foo\"]",
            page_meta_data_.proceed_element_descriptor.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::CSS_SELECTOR,
            page_meta_data_.proceed_element_descriptor.retrieval_method);

  // Clear |field_infos_| for the next test;
  field_infos_.clear();

  // Test first attribute is always the one set.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\">"
        "<page_advance_button css_selector=\"[name=&quot;foo&quot;]\""
        " id=\"foo\"/>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  EXPECT_EQ("[name=\"foo\"]",
            page_meta_data_.proceed_element_descriptor.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::CSS_SELECTOR,
            page_meta_data_.proceed_element_descriptor.retrieval_method);

  // Clear |field_infos_| for the next test;
  field_infos_.clear();

  // Test parsing click_elements_before_formfill correctly.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\">"
        "<click_elements_before_formfill>"
        "<web_element id=\"btn1\" /></click_elements_before_formfill>"
        "<click_elements_before_formfill>"
        "<web_element css_selector=\"[name=&quot;btn2&quot;]\"/>"
        "</click_elements_before_formfill>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  ASSERT_EQ(2U, page_meta_data_.click_elements_before_form_fill.size());
  autofill::WebElementDescriptor& click_elment =
      page_meta_data_.click_elements_before_form_fill[0];
  EXPECT_EQ("btn1", click_elment.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::ID, click_elment.retrieval_method);
  click_elment = page_meta_data_.click_elements_before_form_fill[1];
  EXPECT_EQ("[name=\"btn2\"]", click_elment.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::CSS_SELECTOR,
            click_elment.retrieval_method);

  // Clear |field_infos_| for the next test;
  field_infos_.clear();

  // Test parsing click_elements_after_formfill correctly.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\">"
        "<click_elements_after_formfill>"
        "<web_element id=\"btn1\" /></click_elements_after_formfill>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  ASSERT_EQ(1U, page_meta_data_.click_elements_after_form_fill.size());
  click_elment = page_meta_data_.click_elements_after_form_fill[0];
  EXPECT_EQ("btn1", click_elment.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::ID, click_elment.retrieval_method);

  // Clear |field_infos_| for the next test.
  field_infos_.clear();

  // Test setting of ignore_ajax attribute.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\" ignore_ajax=\"true\">"
        "<page_advance_button css_selector=\"[name=&quot;foo&quot;]\""
        " id=\"foo\"/>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  EXPECT_TRUE(page_meta_data_.ignore_ajax);
  EXPECT_EQ("[name=\"foo\"]",
            page_meta_data_.proceed_element_descriptor.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::CSS_SELECTOR,
            page_meta_data_.proceed_element_descriptor.retrieval_method);

 // Clear |field_infos_| for the next test.
  field_infos_.clear();

  // Test redundant setting to false of ignore_ajax attribute.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"55\"/>"
        "<autofill_flow page_no=\"1\" total_pages=\"10\" ignore_ajax=\"false\">"
        "<page_advance_button css_selector=\"[name=&quot;foo&quot;]\""
        " id=\"foo\"/>"
        "</autofill_flow>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(1U, field_infos_.size());
  EXPECT_EQ(1, page_meta_data_.current_page_number);
  EXPECT_EQ(10, page_meta_data_.total_pages);
  EXPECT_FALSE(page_meta_data_.ignore_ajax);
  EXPECT_EQ("[name=\"foo\"]",
            page_meta_data_.proceed_element_descriptor.descriptor);
  EXPECT_EQ(autofill::WebElementDescriptor::CSS_SELECTOR,
            page_meta_data_.proceed_element_descriptor.retrieval_method);
}

// Test badly formed XML queries.
TEST_F(AutofillQueryXmlParserTest, ParseErrors) {
  // Test no Autofill type.
  std::string xml = "<autofillqueryresponse>"
                    "<field/>"
                    "</autofillqueryresponse>";

  ParseQueryXML(xml, false);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  EXPECT_EQ(0U, field_infos_.size());
  EXPECT_TRUE(experiment_id_.empty());

  // Test an incorrect Autofill type.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"-1\"/>"
        "</autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  // AutofillType was out of range and should be set to NO_SERVER_DATA.
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());

  // Test upper bound for the field type, MAX_VALID_FIELD_TYPE.
  field_infos_.clear();
  xml = "<autofillqueryresponse><field autofilltype=\"" +
      base::IntToString(MAX_VALID_FIELD_TYPE) + "\"/></autofillqueryresponse>";

  ParseQueryXML(xml, true);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  // AutofillType was out of range and should be set to NO_SERVER_DATA.
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());

  // Test an incorrect Autofill type.
  field_infos_.clear();
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"No Type\"/>"
        "</autofillqueryresponse>";

  // Parse fails but an entry is still added to field_infos_.
  ParseQueryXML(xml, false);

  EXPECT_EQ(USE_UPLOAD_RATES, upload_required_);
  ASSERT_EQ(1U, field_infos_.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos_[0].field_type);
  EXPECT_TRUE(experiment_id_.empty());
}

// Test successfull upload response.
TEST_F(AutofillUploadXmlParserTest, TestSuccessfulResponse) {
  ParseUploadXML("<autofilluploadresponse positiveuploadrate=\"0.5\" "
                 "negativeuploadrate=\"0.3\"/>",
                 true);

  EXPECT_DOUBLE_EQ(0.5, positive_);
  EXPECT_DOUBLE_EQ(0.3, negative_);
}

// Test failed upload response.
TEST_F(AutofillUploadXmlParserTest, TestFailedResponse) {
  ParseUploadXML("<autofilluploadresponse positiveuploadrate=\"\" "
                 "negativeuploadrate=\"0.3\"/>",
                 false);

  EXPECT_DOUBLE_EQ(0, positive_);
  EXPECT_DOUBLE_EQ(0.3, negative_);  // Partially parsed.
  negative_ = 0;

  ParseUploadXML("<autofilluploadresponse positiveuploadrate=\"0.5\" "
                 "negativeuploadrate=\"0.3\"",
                 false);

  EXPECT_DOUBLE_EQ(0, positive_);
  EXPECT_DOUBLE_EQ(0, negative_);

  ParseUploadXML("bad data", false);

  EXPECT_DOUBLE_EQ(0, positive_);
  EXPECT_DOUBLE_EQ(0, negative_);

  ParseUploadXML(std::string(), false);

  EXPECT_DOUBLE_EQ(0, positive_);
  EXPECT_DOUBLE_EQ(0, negative_);
}

}  // namespace
}  // namespace autofill
