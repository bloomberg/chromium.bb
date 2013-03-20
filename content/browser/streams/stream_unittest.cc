// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class StreamTest : public testing::Test {
 public:
  StreamTest() : producing_seed_key_(0) {}

  virtual void SetUp() OVERRIDE {
    registry_.reset(new StreamRegistry());
  }

  // Create a new IO buffer of the given |buffer_size| and fill it with random
  // data.
  scoped_refptr<net::IOBuffer> NewIOBuffer(size_t buffer_size) {
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(buffer_size));
    char *bufferp = buffer->data();
    for (size_t i = 0; i < buffer_size; i++)
      bufferp[i] = (i + producing_seed_key_) % (1 << sizeof(char));
    ++producing_seed_key_;
    return buffer;
  }

 protected:
  MessageLoop message_loop_;
  scoped_ptr<StreamRegistry> registry_;

 private:
  int producing_seed_key_;
};

class TestStreamReader : public StreamReadObserver {
 public:
  TestStreamReader() : buffer_(new net::GrowableIOBuffer()) {
  }
  virtual ~TestStreamReader() {}

  void Read(Stream* stream) {
    const size_t kBufferSize = 32768;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

    int bytes_read = 0;
    while (stream->ReadRawData(buffer, kBufferSize, &bytes_read) ==
           Stream::STREAM_HAS_DATA) {
      size_t old_capacity = buffer_->capacity();
      buffer_->SetCapacity(old_capacity + bytes_read);
      memcpy(buffer_->StartOfBuffer() + old_capacity,
             buffer->data(), bytes_read);
    }
  }

  virtual void OnDataAvailable(Stream* stream) OVERRIDE {
    Read(stream);
  }

  scoped_refptr<net::GrowableIOBuffer> buffer() { return buffer_; }

 private:
  scoped_refptr<net::GrowableIOBuffer> buffer_;
};

class TestStreamWriter : public StreamWriteObserver {
 public:
  TestStreamWriter() {}
  virtual ~TestStreamWriter() {}

  void Write(Stream* stream,
             scoped_refptr<net::IOBuffer> buffer,
             size_t buffer_size) {
    stream->AddData(buffer, buffer_size);
  }

  virtual void OnSpaceAvailable(Stream* stream) OVERRIDE {
  }

  virtual void OnClose(Stream* stream) OVERRIDE {
  }
};

TEST_F(StreamTest, SetReadObserver) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, GURL(), url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));
}

TEST_F(StreamTest, SetReadObserver_SecondFails) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, GURL(), url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));
  EXPECT_FALSE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, SetReadObserver_TwoReaders) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, GURL(), url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));

  // Once the first read observer is removed, a new one can be added.
  stream->RemoveReadObserver(&reader1);
  EXPECT_TRUE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, Stream) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, GURL(), url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream, buffer, kBufferSize);
  stream->Finalize();
  reader.Read(stream);
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(reader.buffer()->capacity(), kBufferSize);
  for (int i = 0; i < kBufferSize; i++)
    EXPECT_EQ(buffer->data()[i], reader.buffer()->data()[i]);
}

TEST_F(StreamTest, GetStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, GURL(), url));

  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, GetStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, GURL(), url1));

  GURL url2("blob://stream2");
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2);
}

TEST_F(StreamTest, CloneStream) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, GURL(), url1));

  GURL url2("blob://stream2");
  ASSERT_TRUE(registry_->CloneStream(url2, url1));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, CloneStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, GURL(), url1));

  GURL url2("blob://stream2");
  GURL url3("blob://stream3");
  ASSERT_FALSE(registry_->CloneStream(url2, url3));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2);
}

TEST_F(StreamTest, UnregisterStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, GURL(), url));

  registry_->UnregisterStream(url);
  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_FALSE(stream2);
}

}  // namespace content
