// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "grit/generated_resources.h"
#include "grit/webui_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeWebUIDataSourceTest : public testing::Test {
 public:
  ChromeWebUIDataSourceTest() : result_data_(NULL) {}
  virtual ~ChromeWebUIDataSourceTest() {}
  ChromeWebUIDataSource* source() { return source_.get(); }

  void StartDataRequest(const std::string& path) {
     source_->StartDataRequest(
        path,
        false,
        base::Bind(&ChromeWebUIDataSourceTest::SendResult,
        base::Unretained(this)));
  }

  std::string GetMimeType(const std::string& path) const {
    return source_->GetMimeType(path);
  }

  scoped_refptr<base::RefCountedMemory> result_data_;

 private:
  virtual void SetUp() {
    content::WebUIDataSource* source = ChromeWebUIDataSource::Create("host");
    ChromeWebUIDataSource* source_impl = static_cast<ChromeWebUIDataSource*>(
        source);
    source_ = make_scoped_refptr(source_impl);
  }

  virtual void TearDown() {
  }

  // Store response for later comparisons.
  void SendResult(base::RefCountedMemory* data) {
    result_data_ = data;
  }

  scoped_refptr<ChromeWebUIDataSource> source_;
};

TEST_F(ChromeWebUIDataSourceTest, EmptyStrings) {
  source()->SetJsonPath("strings.js");
  StartDataRequest("strings.js");
  std::string result(reinterpret_cast<const char*>(
      result_data_->front()), result_data_->size());
  EXPECT_NE(result.find("var templateData = {"), std::string::npos);
  EXPECT_NE(result.find("};"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, SomeStrings) {
  source()->SetJsonPath("strings.js");
  source()->AddString("planet", ASCIIToUTF16("pluto"));
  source()->AddLocalizedString("button", IDS_OK);
  StartDataRequest("strings.js");
  std::string result(reinterpret_cast<const char*>(
      result_data_->front()), result_data_->size());
  EXPECT_NE(result.find("\"planet\":\"pluto\""), std::string::npos);
  EXPECT_NE(result.find("\"button\":\"OK\""), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, DefaultResource) {
  source()->SetDefaultResource(IDR_WEBUI_I18N_PROCESS_JS);
  StartDataRequest("foobar" );
  std::string result(
      reinterpret_cast<const char*>(result_data_->front()),
      result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
  StartDataRequest("strings.js");
  result = std::string(
      reinterpret_cast<const char*>(result_data_->front()),
      result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, NamedResource) {
  source()->SetDefaultResource(IDR_WEBUI_I18N_PROCESS_JS);
  source()->AddResourcePath("foobar", IDR_WEBUI_I18N_TEMPLATE_JS);
  StartDataRequest("foobar");
  std::string result(
      reinterpret_cast<const char*>(result_data_->front()),
      result_data_->size());
  EXPECT_NE(result.find("var i18nTemplate"), std::string::npos);
  StartDataRequest("strings.js");
  result = std::string(
      reinterpret_cast<const char*>(result_data_->front()),
      result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, MimeType) {
  const char* html = "text/html";
  const char* js = "application/javascript";
  EXPECT_EQ(GetMimeType(""), html);
  EXPECT_EQ(GetMimeType("foo"), html);
  EXPECT_EQ(GetMimeType("foo.html"), html);
  EXPECT_EQ(GetMimeType(".js"), js);
  EXPECT_EQ(GetMimeType("foo.js"), js);
  EXPECT_EQ(GetMimeType("js"), html);
  EXPECT_EQ(GetMimeType("foojs"), html);
  EXPECT_EQ(GetMimeType("foo.jsp"), html);
}
