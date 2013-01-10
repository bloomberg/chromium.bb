// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlparser.h"

namespace {

TEST(AutofillQueryXmlParserTest, BasicQuery) {
  // An XML string representing a basic query response.
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"0\" />"
                    "<field autofilltype=\"1\" />"
                    "<field autofilltype=\"3\" />"
                    "<field autofilltype=\"2\" />"
                    "<field autofilltype=\"61\" defaultvalue=\"default\"/>"
                    "</autofillqueryresponse>";

  // Create a vector of AutofillServerFieldInfos, to assign the parsed field
  // types to.
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required = USE_UPLOAD_RATES;
  std::string experiment_id;

  // Create a parser.
  AutofillQueryXmlParser parse_handler(&field_infos, &upload_required,
                                       &experiment_id);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler.succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(5U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(UNKNOWN_TYPE, field_infos[1].field_type);
  EXPECT_EQ(NAME_FIRST, field_infos[2].field_type);
  EXPECT_EQ(EMPTY_TYPE, field_infos[3].field_type);
  EXPECT_EQ("", field_infos[3].default_value);
  EXPECT_EQ(FIELD_WITH_DEFAULT_VALUE, field_infos[4].field_type);
  EXPECT_EQ("default", field_infos[4].default_value);
  EXPECT_EQ(std::string(), experiment_id);
}

// Test parsing the upload required attribute.
TEST(AutofillQueryXmlParserTest, TestUploadRequired) {
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required = USE_UPLOAD_RATES;
  std::string experiment_id;

  std::string xml = "<autofillqueryresponse uploadrequired=\"true\">"
                    "<field autofilltype=\"0\" />"
                    "</autofillqueryresponse>";

  scoped_ptr<AutofillQueryXmlParser> parse_handler(
      new AutofillQueryXmlParser(&field_infos, &upload_required,
                                 &experiment_id));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(UPLOAD_REQUIRED, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);

  field_infos.clear();
  xml = "<autofillqueryresponse uploadrequired=\"false\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(UPLOAD_NOT_REQUIRED, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);

  field_infos.clear();
  xml = "<autofillqueryresponse uploadrequired=\"bad_value\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);
}

// Test parsing the experiment id attribute
TEST(AutofillQueryXmlParserTest, ParseExperimentId) {
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required = USE_UPLOAD_RATES;
  std::string experiment_id;

  // When the attribute is missing, we should get back the default value -- the
  // empty string.
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"0\" />"
                    "</autofillqueryresponse>";

  scoped_ptr<AutofillQueryXmlParser> parse_handler(
      new AutofillQueryXmlParser(&field_infos, &upload_required,
                                 &experiment_id));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);

  field_infos.clear();

  // When the attribute is present, make sure we parse it.
  xml = "<autofillqueryresponse experimentid=\"FancyNewAlgorithm\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string("FancyNewAlgorithm"), experiment_id);

  field_infos.clear();

  // Make sure that we can handle parsing both the upload required and the
  // experiment id attribute together.
  xml = "<autofillqueryresponse uploadrequired=\"false\""
        "                       experimentid=\"ServerSmartyPants\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(UPLOAD_NOT_REQUIRED, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string("ServerSmartyPants"), experiment_id);
}

// Test badly formed XML queries.
TEST(AutofillQueryXmlParserTest, ParseErrors) {
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required = USE_UPLOAD_RATES;
  std::string experiment_id;

  // Test no Autofill type.
  std::string xml = "<autofillqueryresponse>"
                    "<field/>"
                    "</autofillqueryresponse>";

  scoped_ptr<AutofillQueryXmlParser> parse_handler(
      new AutofillQueryXmlParser(&field_infos, &upload_required,
                                 &experiment_id));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_FALSE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  EXPECT_EQ(0U, field_infos.size());
  EXPECT_EQ(std::string(), experiment_id);

  // Test an incorrect Autofill type.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"-1\"/>"
        "</autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  // AutofillType was out of range and should be set to NO_SERVER_DATA.
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);

  // Test upper bound for the field type, MAX_VALID_FIELD_TYPE.
  field_infos.clear();
  xml = "<autofillqueryresponse><field autofilltype=\"" +
      base::IntToString(MAX_VALID_FIELD_TYPE) + "\"/></autofillqueryresponse>";

  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  // AutofillType was out of range and should be set to NO_SERVER_DATA.
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);

  // Test an incorrect Autofill type.
  field_infos.clear();
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"No Type\"/>"
        "</autofillqueryresponse>";

  // Parse fails but an entry is still added to field_infos.
  parse_handler.reset(new AutofillQueryXmlParser(&field_infos, &upload_required,
                                                 &experiment_id));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_FALSE(parse_handler->succeeded());
  EXPECT_EQ(USE_UPLOAD_RATES, upload_required);
  ASSERT_EQ(1U, field_infos.size());
  EXPECT_EQ(NO_SERVER_DATA, field_infos[0].field_type);
  EXPECT_EQ(std::string(), experiment_id);
}

// Test successfull upload response.
TEST(AutofillUploadXmlParser, TestSuccessfulResponse) {
  std::string xml = "<autofilluploadresponse positiveuploadrate=\"0.5\" "
                    "negativeuploadrate=\"0.3\"/>";
  double positive = 0;
  double negative = 0;
  AutofillUploadXmlParser parse_handler(&positive, &negative);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler.succeeded());
  EXPECT_DOUBLE_EQ(0.5, positive);
  EXPECT_DOUBLE_EQ(0.3, negative);
}

// Test failed upload response.
TEST(AutofillUploadXmlParser, TestFailedResponse) {
  std::string xml = "<autofilluploadresponse positiveuploadrate=\"\" "
                    "negativeuploadrate=\"0.3\"/>";
  double positive = 0;
  double negative = 0;
  scoped_ptr<AutofillUploadXmlParser> parse_handler(
      new AutofillUploadXmlParser(&positive, &negative));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(!parse_handler->succeeded());
  EXPECT_DOUBLE_EQ(0, positive);
  EXPECT_DOUBLE_EQ(0.3, negative);  // Partially parsed.
  negative = 0;

  xml = "<autofilluploadresponse positiveuploadrate=\"0.5\" "
        "negativeuploadrate=\"0.3\"";
  parse_handler.reset(new AutofillUploadXmlParser(&positive, &negative));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(!parse_handler->succeeded());
  EXPECT_DOUBLE_EQ(0, positive);
  EXPECT_DOUBLE_EQ(0, negative);

  xml = "bad data";
  parse_handler.reset(new AutofillUploadXmlParser(&positive, &negative));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(!parse_handler->succeeded());
  EXPECT_DOUBLE_EQ(0, positive);
  EXPECT_DOUBLE_EQ(0, negative);

  xml = "";
  parse_handler.reset(new AutofillUploadXmlParser(&positive, &negative));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(!parse_handler->succeeded());
  EXPECT_DOUBLE_EQ(0, positive);
  EXPECT_DOUBLE_EQ(0, negative);
}

}  // namespace
