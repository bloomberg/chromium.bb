// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "content/common/cross_site_document_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using Result = content::CrossSiteDocumentClassifier::Result;

namespace content {

TEST(CrossSiteDocumentClassifierTest, IsBlockableScheme) {
  GURL data_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA==");
  GURL ftp_url("ftp://google.com");
  GURL mailto_url("mailto:google@google.com");
  GURL about_url("about:chrome");
  GURL http_url("http://google.com");
  GURL https_url("https://google.com");

  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(data_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(ftp_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(mailto_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(about_url));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsBlockableScheme(http_url));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsBlockableScheme(https_url));
}

TEST(CrossSiteDocumentClassifierTest, IsSameSite) {
  GURL a_com_url0("https://mock1.a.com:8080/page1.html");
  GURL a_com_url1("https://mock2.a.com:9090/page2.html");
  GURL a_com_url2("https://a.com/page3.html");
  url::Origin a_com_origin0 = url::Origin::Create(a_com_url0);
  EXPECT_TRUE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_origin0, a_com_url1));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsSameSite(
      url::Origin::Create(a_com_url1), a_com_url2));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsSameSite(
      url::Origin::Create(a_com_url2), a_com_url0));

  GURL b_com_url0("https://mock1.b.com/index.html");
  EXPECT_FALSE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_origin0, b_com_url0));

  GURL about_blank_url("about:blank");
  EXPECT_FALSE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_origin0, about_blank_url));

  GURL chrome_url("chrome://extension");
  EXPECT_FALSE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_origin0, chrome_url));

  GURL empty_url("");
  EXPECT_FALSE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_origin0, empty_url));
}

TEST(CrossSiteDocumentClassifierTest, IsValidCorsHeaderSet) {
  url::Origin frame_origin = url::Origin::Create(GURL("http://www.google.com"));
  GURL site_origin_url("http://www.yahoo.com");

  EXPECT_TRUE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "*"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "\"*\""));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "http://mail.google.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "https://mail.google.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "http://yahoo.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin_url, "www.google.com"));
}

TEST(CrossSiteDocumentClassifierTest, SniffForHTML) {
  StringPiece html_data("  \t\r\n    <HtMladfokadfkado");
  StringPiece comment_html_data(" <!-- this is comment --> <html><body>");
  StringPiece two_comments_html_data(
      "<!-- this is comment -->\n<!-- this is comment --><html><body>");
  StringPiece commented_out_html_tag_data("<!-- <html> <?xml> \n<html>--><b");
  StringPiece mixed_comments_html_data(
      "<!-- this is comment <!-- --> <script></script>");
  StringPiece non_html_data("        var name=window.location;\nadfadf");
  StringPiece comment_js_data(
      " <!-- this is comment\n document.write(1);\n// -->window.open()");
  StringPiece empty_data("");

  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForHTML(html_data));
  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForHTML(comment_html_data));
  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForHTML(two_comments_html_data));
  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForHTML(
                              commented_out_html_tag_data));
  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForHTML(
                              mixed_comments_html_data));
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForHTML(non_html_data));
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForHTML(comment_js_data));

  // Prefixes of |commented_out_html_tag_data| should be indeterminate.
  StringPiece almost_html = commented_out_html_tag_data;
  while (!almost_html.empty()) {
    almost_html.remove_suffix(1);
    EXPECT_EQ(Result::kMaybe,
              CrossSiteDocumentClassifier::SniffForHTML(almost_html))
        << almost_html;
  }
}

TEST(CrossSiteDocumentClassifierTest, SniffForXML) {
  StringPiece xml_data("   \t \r \n     <?xml version=\"1.0\"?>\n <catalog");
  StringPiece non_xml_data("        var name=window.location;\nadfadf");
  StringPiece empty_data("");

  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForXML(xml_data));
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForXML(non_xml_data));

  // Empty string should be indeterminate.
  EXPECT_EQ(Result::kMaybe,
            CrossSiteDocumentClassifier::SniffForXML(empty_data));
}

TEST(CrossSiteDocumentClassifierTest, SniffForJSON) {
  StringPiece json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  StringPiece json_corrupt_after_first_key(
      "\t\t\r\n   { \"name\" :^^^^!!@#\1\", ");
  StringPiece json_data2("{ \"key   \\\"  \"          \t\t\r\n:");
  StringPiece non_json_data0("\t\t\r\n   { name : \"chrome\", ");
  StringPiece non_json_data1("\t\t\r\n   foo({ \"name\" : \"chrome\", ");
  StringPiece empty_data("");

  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForJSON(json_data));
  EXPECT_EQ(Result::kYes, CrossSiteDocumentClassifier::SniffForJSON(
                              json_corrupt_after_first_key));

  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForJSON(json_data2));

  // All prefixes prefixes of |json_data2| ought to be indeterminate.
  StringPiece almost_json = json_data2;
  while (!almost_json.empty()) {
    almost_json.remove_suffix(1);
    EXPECT_EQ(Result::kMaybe,
              CrossSiteDocumentClassifier::SniffForJSON(almost_json))
        << almost_json;
  }

  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForJSON(non_json_data0));
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForJSON(non_json_data1));

  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForJSON(R"({"" : 1})"))
      << "Empty strings are accepted";
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForJSON(R"({'' : 1})"))
      << "Single quotes are not accepted";
  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForJSON("{\"\\\"\" : 1}"))
      << "Escaped quotes are recognized";
  EXPECT_EQ(Result::kYes,
            CrossSiteDocumentClassifier::SniffForJSON(R"({"\\\u000a" : 1})"))
      << "Escaped control characters are recognized";
  EXPECT_EQ(Result::kMaybe,
            CrossSiteDocumentClassifier::SniffForJSON(R"({"\\\u00)"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(Result::kMaybe, CrossSiteDocumentClassifier::SniffForJSON("{\"\\"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(Result::kMaybe,
            CrossSiteDocumentClassifier::SniffForJSON("{\"\\\""))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForJSON("{\"\n\" : true}"))
      << "Unescaped control characters are rejected";
  EXPECT_EQ(Result::kNo, CrossSiteDocumentClassifier::SniffForJSON("{}"))
      << "Empty dictionary is not recognized (since it's valid JS too)";
  EXPECT_EQ(Result::kNo,
            CrossSiteDocumentClassifier::SniffForJSON("[true, false, 1, 2]"))
      << "Lists dictionary are not recognized (since they're valid JS too)";
  EXPECT_EQ(Result::kNo, CrossSiteDocumentClassifier::SniffForJSON(R"({":"})"))
      << "A colon character inside a string does not trigger a match";
}

TEST(CrossSiteDocumentClassifierTest, GetCanonicalMimeType) {
  std::vector<std::pair<const char*, CrossSiteDocumentMimeType>> tests = {
      // Basic tests for things in the original implementation:
      {"text/html", CROSS_SITE_DOCUMENT_MIME_TYPE_HTML},
      {"text/xml", CROSS_SITE_DOCUMENT_MIME_TYPE_XML},
      {"application/rss+xml", CROSS_SITE_DOCUMENT_MIME_TYPE_XML},
      {"application/xml", CROSS_SITE_DOCUMENT_MIME_TYPE_XML},
      {"application/json", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"text/json", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"text/x-json", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"text/plain", CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN},

      // Other mime types:
      {"application/foobar", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},

      // Regression tests for https://crbug.com/799155 (prefix/suffix matching):
      {"application/json+protobuf", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"text/json+protobuf", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"application/activity+json", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"text/foobar+xml", CROSS_SITE_DOCUMENT_MIME_TYPE_XML},
      // No match without a '+' character:
      {"application/jsonfoobar", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},
      {"application/foobarjson", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},
      {"application/xmlfoobar", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},
      {"application/foobarxml", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},

      // Case-insensitive comparison:
      {"APPLICATION/JSON", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"APPLICATION/JSON+PROTOBUF", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},
      {"APPLICATION/ACTIVITY+JSON", CROSS_SITE_DOCUMENT_MIME_TYPE_JSON},

      // Images are allowed cross-site, and SVG is an image, so we should
      // classify SVG as "other" instead of "xml" (even though it technically is
      // an xml document).
      {"image/svg+xml", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},

      // Javascript should not be blocked.
      {"application/javascript", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},
      {"application/jsonp", CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS},
  };

  for (const auto& test : tests) {
    const char* input = test.first;  // e.g. "text/html"
    CrossSiteDocumentMimeType expected = test.second;
    CrossSiteDocumentMimeType actual =
        CrossSiteDocumentClassifier::GetCanonicalMimeType(input);
    EXPECT_EQ(expected, actual)
        << "when testing with the following input: " << input;
  }
}

}  // namespace content
