// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "msgs/presentation/messages.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace msgs {
namespace presentation {

// TODO(btolsch): This is in the current (draft) spec, but should we actually
// allow this?
TEST(PresentationMessagesTest, EncodeRequestZeroUrls) {
  uint8_t buffer[256];
  std::vector<std::string> urls;
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{3, urls}, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<ssize_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  UrlAvailabilityRequest decoded_request;
  ssize_t bytes_read =
      DecodeUrlAvailabilityRequest(buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(3u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

TEST(PresentationMessagesTest, EncodeRequestOneUrl) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"https://example.com/receiver.html"};
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{7, urls}, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<ssize_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  UrlAvailabilityRequest decoded_request;
  ssize_t bytes_read =
      DecodeUrlAvailabilityRequest(buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(7u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

TEST(PresentationMessagesTest, EncodeRequestMultipleUrls) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"https://example.com/receiver.html",
                                "https://openscreen.org/demo_receiver.html",
                                "https://turt.le/asdfXCV"};
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{7, urls}, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<ssize_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  UrlAvailabilityRequest decoded_request;
  ssize_t bytes_read =
      DecodeUrlAvailabilityRequest(buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(7u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

TEST(PresentationMessagesTest, EncodeWouldOverflow) {
  uint8_t buffer[40];
  std::vector<std::string> urls{"https://example.com/receiver.html"};
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{7, urls}, buffer, sizeof(buffer));
  ASSERT_GT(bytes_out, static_cast<ssize_t>(sizeof(buffer)));
}

// TODO(btolsch): Expand invalid utf8 testing to good/bad files and fuzzing.
TEST(PresentationMessagesTest, EncodeInvalidUtf8) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"\xc0"};
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{7, urls}, buffer, sizeof(buffer));
  ASSERT_GT(0, bytes_out);
}

TEST(PresentationMessagesTest, DecodeInvalidUtf8) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"https://example.com/receiver.html"};
  ssize_t bytes_out = EncodeUrlAvailabilityRequest(
      UrlAvailabilityRequest{7, urls}, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<ssize_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);
  // Manually change a character in the url string to be non-utf8.
  buffer[30] = 0xc0;

  UrlAvailabilityRequest decoded_request;
  ssize_t bytes_read =
      DecodeUrlAvailabilityRequest(buffer, bytes_out, &decoded_request);
  ASSERT_GT(0, bytes_read);
}

}  // namespace presentation
}  // namespace msgs
}  // namespace openscreen
