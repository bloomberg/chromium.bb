// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/child/site_isolation_policy.h"
#include "content/public/common/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "ui/gfx/range/range.h"

namespace content {

TEST(SiteIsolationPolicyTest, IsBlockableScheme) {
  GURL data_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA==");
  GURL ftp_url("ftp://google.com");
  GURL mailto_url("mailto:google@google.com");
  GURL about_url("about:chrome");
  GURL http_url("http://google.com");
  GURL https_url("https://google.com");

  EXPECT_FALSE(SiteIsolationPolicy::IsBlockableScheme(data_url));
  EXPECT_FALSE(SiteIsolationPolicy::IsBlockableScheme(ftp_url));
  EXPECT_FALSE(SiteIsolationPolicy::IsBlockableScheme(mailto_url));
  EXPECT_FALSE(SiteIsolationPolicy::IsBlockableScheme(about_url));
  EXPECT_TRUE(SiteIsolationPolicy::IsBlockableScheme(http_url));
  EXPECT_TRUE(SiteIsolationPolicy::IsBlockableScheme(https_url));
}

TEST(SiteIsolationPolicyTest, IsSameSite) {
  GURL a_com_url0("https://mock1.a.com:8080/page1.html");
  GURL a_com_url1("https://mock2.a.com:9090/page2.html");
  GURL a_com_url2("https://a.com/page3.html");
  EXPECT_TRUE(SiteIsolationPolicy::IsSameSite(a_com_url0, a_com_url1));
  EXPECT_TRUE(SiteIsolationPolicy::IsSameSite(a_com_url1, a_com_url2));
  EXPECT_TRUE(SiteIsolationPolicy::IsSameSite(a_com_url2, a_com_url0));

  GURL b_com_url0("https://mock1.b.com/index.html");
  EXPECT_FALSE(SiteIsolationPolicy::IsSameSite(a_com_url0, b_com_url0));

  GURL about_blank_url("about:blank");
  EXPECT_FALSE(SiteIsolationPolicy::IsSameSite(a_com_url0, about_blank_url));

  GURL chrome_url("chrome://extension");
  EXPECT_FALSE(SiteIsolationPolicy::IsSameSite(a_com_url0, chrome_url));

  GURL empty_url("");
  EXPECT_FALSE(SiteIsolationPolicy::IsSameSite(a_com_url0, empty_url));
}

TEST(SiteIsolationPolicyTest, IsValidCorsHeaderSet) {
  GURL frame_origin("http://www.google.com");
  GURL site_origin("http://www.yahoo.com");

  EXPECT_TRUE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "*"));
  EXPECT_FALSE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "\"*\""));
  EXPECT_TRUE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "http://mail.google.com"));
  EXPECT_FALSE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "https://mail.google.com"));
  EXPECT_FALSE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "http://yahoo.com"));
  EXPECT_FALSE(SiteIsolationPolicy::IsValidCorsHeaderSet(
      frame_origin, site_origin, "www.google.com"));
}

TEST(SiteIsolationPolicyTest, SniffForHTML) {
  const char html_data[] = "  \t\r\n    <HtMladfokadfkado";
  const char comment_html_data[] = " <!-- this is comment --> <html><body>";
  const char two_comments_html_data[] =
      "<!-- this is comment -->\n<!-- this is comment --><html><body>";
  const char mixed_comments_html_data[] =
      "<!-- this is comment <!-- --> <script></script>";
  const char non_html_data[] = "        var name=window.location;\nadfadf";
  const char comment_js_data[] = " <!-- this is comment -> document.write(1); ";

  EXPECT_TRUE(
      SiteIsolationPolicy::SniffForHTML(html_data, arraysize(html_data)));
  EXPECT_TRUE(SiteIsolationPolicy::SniffForHTML(comment_html_data,
                                                arraysize(comment_html_data)));
  EXPECT_TRUE(SiteIsolationPolicy::SniffForHTML(
      two_comments_html_data, arraysize(two_comments_html_data)));
  EXPECT_TRUE(SiteIsolationPolicy::SniffForHTML(
      mixed_comments_html_data, arraysize(mixed_comments_html_data)));
  EXPECT_FALSE(SiteIsolationPolicy::SniffForHTML(non_html_data,
                                                 arraysize(non_html_data)));
  EXPECT_FALSE(SiteIsolationPolicy::SniffForHTML(comment_js_data,
                                                 arraysize(comment_js_data)));

  // Basic bounds check.
  EXPECT_FALSE(SiteIsolationPolicy::SniffForHTML(html_data, 0));
}

TEST(SiteIsolationPolicyTest, SniffForXML) {
  const char xml_data[] = "   \t \r \n     <?xml version=\"1.0\"?>\n <catalog";
  const char non_xml_data[] = "        var name=window.location;\nadfadf";

  EXPECT_TRUE(SiteIsolationPolicy::SniffForXML(xml_data, arraysize(xml_data)));
  EXPECT_FALSE(
      SiteIsolationPolicy::SniffForXML(non_xml_data, arraysize(non_xml_data)));

  // Basic bounds check.
  EXPECT_FALSE(SiteIsolationPolicy::SniffForXML(xml_data, 0));
}

TEST(SiteIsolationPolicyTest, SniffForJSON) {
  const char json_data[] = "\t\t\r\n   { \"name\" : \"chrome\", ";
  const char non_json_data0[] = "\t\t\r\n   { name : \"chrome\", ";
  const char non_json_data1[] = "\t\t\r\n   foo({ \"name\" : \"chrome\", ";

  EXPECT_TRUE(
      SiteIsolationPolicy::SniffForJSON(json_data, arraysize(json_data)));
  EXPECT_FALSE(SiteIsolationPolicy::SniffForJSON(non_json_data0,
                                                 arraysize(non_json_data0)));
  EXPECT_FALSE(SiteIsolationPolicy::SniffForJSON(non_json_data1,
                                                 arraysize(non_json_data1)));

  // Basic bounds check.
  EXPECT_FALSE(SiteIsolationPolicy::SniffForJSON(json_data, 0));
}

TEST(SiteIsolationPolicyTest, SniffForJS) {
  const char basic_js_data[] = "var a = 4";
  const char js_data[] = "\t\t\r\n var a = 4";
  const char json_data[] = "\t\t\r\n   { \"name\" : \"chrome\", ";

  EXPECT_TRUE(SiteIsolationPolicy::SniffForJS(js_data, arraysize(js_data)));
  EXPECT_FALSE(
      SiteIsolationPolicy::SniffForJS(json_data, arraysize(json_data)));

  // Basic bounds check.
  EXPECT_FALSE(SiteIsolationPolicy::SniffForJS(basic_js_data, 0));
}

}  // namespace content
