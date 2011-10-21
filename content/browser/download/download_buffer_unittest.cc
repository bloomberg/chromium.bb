// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_buffer.h"

#include <string.h>

#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char* kTestData[] = {
  "Four score and twenty years ago",
  "Our four (or five) fathers",
  "Danced and sang in the woods"
};
const size_t kTestDataQty = arraysize(kTestData);

class DownloadBufferTest : public testing::Test {
 public:
  DownloadBufferTest() {
  }
  ~DownloadBufferTest() {
  }

  void CreateBuffer() {
    EXPECT_EQ(0u, content_buffer_.size());

    for (size_t i = 0; i < kTestDataQty; ++i) {
      net::StringIOBuffer* io_buffer = new net::StringIOBuffer(kTestData[i]);
      content_buffer_.push_back(std::make_pair(io_buffer, io_buffer->size()));
      EXPECT_EQ(i + 1, content_buffer_.size());
    }
  }

  ContentVector* content_buffer() {
    return &content_buffer_;
  }

 private:
  ContentVector content_buffer_;
};

TEST_F(DownloadBufferTest, CreateContentVector) {
  CreateBuffer();

  ContentVector* contents = content_buffer();

  EXPECT_EQ(kTestDataQty, content_buffer()->size());
  EXPECT_EQ(kTestDataQty, contents->size());

  for (size_t i = 0; i < kTestDataQty; ++i) {
    size_t io_buffer_size = strlen(kTestData[i]);
    EXPECT_STREQ(kTestData[i], (*contents)[i].first->data());
    EXPECT_EQ(io_buffer_size, (*contents)[i].second);
  }
}

TEST_F(DownloadBufferTest, CreateDownloadBuffer) {
  scoped_refptr<DownloadBuffer> content_buffer(new DownloadBuffer);
  EXPECT_EQ(0u, content_buffer->size());

  for (size_t i = 0; i < kTestDataQty; ++i) {
    net::StringIOBuffer* io_buffer = new net::StringIOBuffer(kTestData[i]);
    EXPECT_EQ(i + 1, content_buffer->AddData(io_buffer, io_buffer->size()));
  }
  scoped_ptr<ContentVector> contents(content_buffer->ReleaseContents());

  EXPECT_EQ(0u, content_buffer->size());
  EXPECT_EQ(kTestDataQty, contents->size());

  for (size_t i = 0; i < kTestDataQty; ++i) {
    size_t io_buffer_size = strlen(kTestData[i]);
    EXPECT_STREQ(kTestData[i], (*contents)[i].first->data());
    EXPECT_EQ(io_buffer_size, (*contents)[i].second);
  }
}

TEST_F(DownloadBufferTest, AssembleData) {
  CreateBuffer();

  size_t assembled_bytes = 0;
  scoped_refptr<net::IOBuffer> assembled_buffer =
      AssembleData(*content_buffer(), &assembled_bytes);

  // Did not change the content buffer.
  EXPECT_EQ(kTestDataQty, content_buffer()->size());

  // Verify the data.
  size_t total_size = 0;
  for (size_t i = 0; i < kTestDataQty; ++i) {
    size_t len = strlen(kTestData[i]);
    // assembled_buffer->data() may not be NULL-terminated, so we can't use
    // EXPECT_STREQ().
    EXPECT_EQ(
        0, memcmp(kTestData[i], assembled_buffer->data() + total_size, len));
    total_size += len;
  }
  // Verify the data length.
  EXPECT_EQ(total_size, assembled_bytes);
}

TEST_F(DownloadBufferTest, AssembleDataWithEmptyBuffer) {
  ContentVector buffer;
  EXPECT_EQ(0u, buffer.size());

  size_t assembled_bytes = 0;
  scoped_refptr<net::IOBuffer> assembled_buffer =
      AssembleData(buffer, &assembled_bytes);

  // Did not change the content buffer.
  EXPECT_EQ(0u, buffer.size());
  EXPECT_EQ(0u, assembled_bytes);
  EXPECT_TRUE(NULL == assembled_buffer.get());
}

}  // namespace

}  // namespace content
