// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_android.h"

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "media/base/test_data_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used in test that do blocking reads from a local file.
class TestMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  TestMediaDataSource(chrome::mojom::MediaDataSourcePtr* interface,
                      const base::FilePath& file_path)
      : file_path_(file_path), binding_(this, mojo::MakeRequest(interface)) {}

  ~TestMediaDataSource() override {}

 private:
  // chrome::mojom::MediaDataSource implementation.
  void Read(int64_t position,
            int64_t length,
            chrome::mojom::MediaDataSource::ReadCallback callback) override {
    base::File file(file_path_, base::File::Flags::FLAG_OPEN |
                                    base::File::Flags::FLAG_READ);
    auto buffer = std::vector<uint8_t>(length);
    int bytes_read = file.Read(position, (char*)(buffer.data()), length);
    if (bytes_read < length)
      buffer.resize(bytes_read);

    std::move(callback).Run(std::vector<uint8_t>(std::move(buffer)));
  }

  base::FilePath file_path_;
  mojo::Binding<chrome::mojom::MediaDataSource> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestMediaDataSource);
};

class MediaParserAndroidTest : public testing::Test {
 public:
  MediaParserAndroidTest() : ref_factory_(base::DoNothing()) {}
  ~MediaParserAndroidTest() override = default;

  void SetUp() override {
    parser_ = std::make_unique<MediaParserAndroid>(ref_factory_.CreateRef());
  }

  void TearDown() override { parser_.reset(); }

 protected:
  MediaParserAndroid* parser() { return parser_.get(); }

 private:
  std::unique_ptr<MediaParserAndroid> parser_;

  service_manager::ServiceContextRefFactory ref_factory_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(MediaParserAndroidTest);
};

// Test to verify encoded video frame can be extracted.
TEST_F(MediaParserAndroidTest, VideoFrameExtraction) {
  auto file_path = media::GetTestDataFilePath("bear.mp4");
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(file_path, &size));

  chrome::mojom::MediaDataSourcePtr data_source_ptr;
  TestMediaDataSource test_data_source(&data_source_ptr, file_path);

  bool result = false;
  base::RunLoop run_loop;
  parser()->ExtractVideoFrame(
      "video/mp4", size, std::move(data_source_ptr),
      base::BindLambdaForTesting([&](bool success, const std::vector<uint8_t>&,
                                     const media::VideoDecoderConfig&) {
        result = success;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(result);
}

}  // namespace
