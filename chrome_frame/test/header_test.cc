// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"
#include "base/rand_util.h"

namespace chrome_frame_test {

class TestData {
 public:
  TestData(const std::string& value, bool in_header, LoadedInRenderer expected)
      : value_(value),
        in_header_(in_header),
        expected_(expected),
        name_(base::IntToString(base::RandInt(0, 1000))) {
  }

  LoadedInRenderer GetExpectedRenderer() const {
    return expected_;
  }

  std::wstring GetPath() const {
    return ASCIIToWide("/" + name_);
  }

  std::wstring GetUrl(MockWebServer* server_mock) const {
    return server_mock->Resolve(ASCIIToWide(name_));
  }

  void ExpectOnServer(MockWebServer* server_mock) const {
    EXPECT_CALL(*server_mock, Get(testing::_, GetPath(), testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(SendFast(GetHeaders(), GetHtml()));
  }

  std::string GetHeaders() const {
    std::ostringstream headers;
    headers << "HTTP/1.1 200 OK\r\n"
            << "Connection: close\r\n"
            << "Content-Type: text/html\r\n";
    if (in_header_) {
      headers << "X-UA-COMPATIBLE: " << value_ << "\r\n";
    }
    return headers.str();
  }

  std::string GetHtml() const {
    std::ostringstream html;
    html << "<html><head>";
    if (!in_header_) {
       html << "<meta http-equiv=\"x-ua-compatible\" content=\"" << value_
            << "\" />";
    }
    html << "</head><body>This is some text.</body></html>";
    return html.str();
  }

 private:
  LoadedInRenderer expected_;
  std::string name_;
  std::string value_;
  bool in_header_;
};

// Determines the major version of the installed IE
// Returns -1 in case of failure, 0 if the version is newer than currently known
int GetIEMajorVersion() {
  switch (GetInstalledIEVersion()) {
    case IE_6:
      return 6;
    case IE_7:
      return 7;
    case IE_8:
      return 8;
    case IE_9:
      return 9;
    case IE_10:
      return 10;
    case IE_INVALID:
    case NON_IE:
    case IE_UNSUPPORTED:
      ADD_FAILURE() << "Failed to detect IE version.";
      return -1;
    default:
      return 0;
  }
}

int LowVersion() {
  int ie_version = GetIEMajorVersion();
  switch (ie_version) {
    case -1:
    case 0:
      return 5;
    default:
      return ie_version - 1;
  }
}

int HighVersion() {
  int ie_version = GetIEMajorVersion();
  switch (ie_version) {
    case -1:
    case 0:
      return 1000;
    default:
      return ie_version + 1;
  }
}

int EqualVersion() {
  int ie_version = GetIEMajorVersion();
  switch (ie_version) {
    case -1:
    case 0:
      return 1000;
    default:
      return ie_version;
  }
}

std::string HeaderValue(int ie_version) {
  if (ie_version == -1) {
    return "IE=8; Chrome=1";
  } else {
    return std::string("IE=8; Chrome=IE") + base::IntToString(ie_version);
  }
}

std::string CorruptHeaderValue(int ie_version) {
  return HeaderValue(ie_version) + ".0";
}

std::string LongHeaderValue(int ie_version) {
  std::string long_value  = HeaderValue(ie_version)  + "; " +
      std::string(256, 'a') + "=bbb";
  DCHECK_GT(long_value.length(), 256u);
  DCHECK_LT(long_value.length(), 512u);
  return long_value;
}

class HeaderTest
    : public MockIEEventSinkTest,
      public testing::TestWithParam<TestData> {
 public:
  HeaderTest() {}
};

INSTANTIATE_TEST_CASE_P(MetaTag, HeaderTest, testing::Values(
    TestData(HeaderValue(LowVersion()), false, IN_IE),
    TestData(HeaderValue(EqualVersion()), false, IN_CF),
    TestData(HeaderValue(HighVersion()), false, IN_CF),
    TestData(LongHeaderValue(LowVersion()), false, IN_IE),
    TestData(LongHeaderValue(EqualVersion()), false, IN_CF),
    TestData(LongHeaderValue(HighVersion()), false, IN_CF)));
INSTANTIATE_TEST_CASE_P(HttpHeader, HeaderTest, testing::Values(
    TestData(HeaderValue(LowVersion()), true, IN_IE),
    TestData(HeaderValue(EqualVersion()), true, IN_CF),
    TestData(HeaderValue(HighVersion()), true, IN_CF),
    TestData(LongHeaderValue(LowVersion()), true, IN_IE),
    TestData(LongHeaderValue(EqualVersion()), true, IN_CF),
    TestData(LongHeaderValue(HighVersion()), true, IN_CF)));
INSTANTIATE_TEST_CASE_P(CorruptValueHeader, HeaderTest, testing::Values(
    TestData(CorruptHeaderValue(LowVersion()), true, IN_IE),
    TestData(CorruptHeaderValue(EqualVersion()), true, IN_IE),
    TestData(CorruptHeaderValue(HighVersion()), true, IN_IE),
    TestData(CorruptHeaderValue(LowVersion()), false, IN_IE),
    TestData(CorruptHeaderValue(EqualVersion()), false, IN_IE),
    TestData(CorruptHeaderValue(HighVersion()), false, IN_IE)));

TEST_P(HeaderTest, HandleXUaCompatibleHeader) {
  std::wstring url = GetParam().GetUrl(&server_mock_);
  LoadedInRenderer expected_renderer = GetParam().GetExpectedRenderer();

  GetParam().ExpectOnServer(&server_mock_);
  ie_mock_.ExpectNavigation(expected_renderer, url);

  EXPECT_CALL(ie_mock_, OnLoad(expected_renderer, testing::StrEq(url)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(url);
}

}  // namespace chrome_frame_test
