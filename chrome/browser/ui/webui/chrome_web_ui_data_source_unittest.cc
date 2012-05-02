// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "grit/common_resources.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

// A mock data source (so we can override SendResponse to get at its data).
class MockChromeWebUIDataSource : public ChromeWebUIDataSource {
 public:
  MockChromeWebUIDataSource()
      : ChromeWebUIDataSource("host"),
        result_request_id_(-1),
        result_data_(NULL) {
  }

  // Subvert protected methods.
  virtual void StartDataRequest(
      const std::string& path, bool is_incognito, int request_id) OVERRIDE {
    ChromeWebUIDataSource::StartDataRequest(path, is_incognito, request_id);
  }

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE {
    return ChromeWebUIDataSource::GetMimeType(path);
  }

  // Store response for later comparisons.
  virtual void SendResponse(int request_id, base::RefCountedMemory* data) {
    result_request_id_ = request_id;
    result_data_ = data;
  }

  int result_request_id_;
  scoped_refptr<base::RefCountedMemory> result_data_;

 private:
  virtual ~MockChromeWebUIDataSource() {}
};

class ChromeWebUIDataSourceTest : public testing::Test {
 public:
  ChromeWebUIDataSourceTest() {}
  virtual ~ChromeWebUIDataSourceTest() {}
  MockChromeWebUIDataSource* source() { return source_.get(); }

 private:
  virtual void SetUp() {
    source_ = make_scoped_refptr(new MockChromeWebUIDataSource());
  }

  virtual void TearDown() {
  }

  scoped_refptr<MockChromeWebUIDataSource> source_;
};

TEST_F(ChromeWebUIDataSourceTest, EmptyStrings) {
  source()->set_json_path("strings.js");
  source()->StartDataRequest("strings.js" , false, 1);
  EXPECT_EQ(source()->result_request_id_, 1);
  std::string result(reinterpret_cast<const char*>(
      source()->result_data_->front()), source()->result_data_->size());
  EXPECT_NE(result.find("var templateData = {"), std::string::npos);
  EXPECT_NE(result.find("};"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, SomeStrings) {
  source()->set_json_path("strings.js");
  source()->AddString("planet", ASCIIToUTF16("pluto"));
  source()->AddLocalizedString("button", IDS_OK);
  source()->StartDataRequest("strings.js" , false, 2);
  EXPECT_EQ(source()->result_request_id_, 2);
  std::string result(reinterpret_cast<const char*>(
      source()->result_data_->front()), source()->result_data_->size());
  EXPECT_NE(result.find("\"planet\":\"pluto\""), std::string::npos);
  EXPECT_NE(result.find("\"button\":\"OK\""), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, DefaultResource) {
  source()->set_default_resource(IDR_I18N_PROCESS_JS);
  source()->StartDataRequest("foobar" , false, 3);
  EXPECT_EQ(source()->result_request_id_, 3);
  std::string result(
      reinterpret_cast<const char*>(source()->result_data_->front()),
      source()->result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
  source()->StartDataRequest("strings.js" , false, 4);
  EXPECT_EQ(source()->result_request_id_, 4);
  result = std::string(
      reinterpret_cast<const char*>(source()->result_data_->front()),
      source()->result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, NamedResource) {
  source()->set_default_resource(IDR_I18N_PROCESS_JS);
  source()->add_resource_path("foobar", IDR_I18N_TEMPLATE_JS);
  source()->StartDataRequest("foobar" , false, 5);
  EXPECT_EQ(source()->result_request_id_, 5);
  std::string result(
      reinterpret_cast<const char*>(source()->result_data_->front()),
      source()->result_data_->size());
  EXPECT_NE(result.find("var i18nTemplate"), std::string::npos);
  source()->StartDataRequest("strings.js" , false, 4);
  EXPECT_EQ(source()->result_request_id_, 4);
  result = std::string(
      reinterpret_cast<const char*>(source()->result_data_->front()),
      source()->result_data_->size());
  EXPECT_NE(result.find("i18nTemplate.process"), std::string::npos);
}

TEST_F(ChromeWebUIDataSourceTest, MimeType) {
  const char* html = "text/html";
  const char* js = "application/javascript";
  EXPECT_EQ(source()->GetMimeType(""), html);
  EXPECT_EQ(source()->GetMimeType("foo"), html);
  EXPECT_EQ(source()->GetMimeType("foo.html"), html);
  EXPECT_EQ(source()->GetMimeType(".js"), js);
  EXPECT_EQ(source()->GetMimeType("foo.js"), js);
  EXPECT_EQ(source()->GetMimeType("js"), html);
  EXPECT_EQ(source()->GetMimeType("foojs"), html);
  EXPECT_EQ(source()->GetMimeType("foo.jsp"), html);
}
