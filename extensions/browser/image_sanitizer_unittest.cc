// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/image_sanitizer.h"

#include <map>
#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/data_decoder/public/cpp/test_data_decoder_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kBase64edValidPng[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd"
    "1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC";

constexpr char kBase64edInvalidPng[] =
    "Rw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd"
    "1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC";

class ImageSanitizerTest : public testing::Test {
 public:
  ImageSanitizerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::DEFAULT) {}

 protected:
  void CreateValidImage(const base::FilePath::StringPieceType& file_name) {
    ASSERT_TRUE(WriteBase64DataToFile(kBase64edValidPng, file_name));
  }

  void CreateInvalidImage(const base::FilePath::StringPieceType& file_name) {
    ASSERT_TRUE(WriteBase64DataToFile(kBase64edInvalidPng, file_name));
  }

  const base::FilePath& GetImagePath() const { return temp_dir_.GetPath(); }

  void WaitForSanitizationDone() {
    ASSERT_FALSE(done_callback_);
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CreateAndStartSanitizer(
      const std::set<base::FilePath>& image_relative_paths) {
    sanitizer_ = ImageSanitizer::CreateAndStart(
        test_data_decoder_service_.connector(), service_manager::Identity(),
        temp_dir_.GetPath(), image_relative_paths,
        base::BindRepeating(&ImageSanitizerTest::ImageSanitizerDecodedImage,
                            base::Unretained(this)),
        base::BindOnce(&ImageSanitizerTest::ImageSanitizationDone,
                       base::Unretained(this)));
  }

  ImageSanitizer::Status last_reported_status() const { return last_status_; }

  const base::FilePath& last_reported_path() const {
    return last_reported_path_;
  }

  std::map<base::FilePath, SkBitmap>* decoded_images() {
    return &decoded_images_;
  }

 private:
  bool WriteBase64DataToFile(const std::string& base64_data,
                             const base::FilePath::StringPieceType& file_name) {
    std::string binary;
    if (!base::Base64Decode(base64_data, &binary))
      return false;

    base::FilePath path = temp_dir_.GetPath().Append(file_name);
    return base::WriteFile(path, binary.data(), binary.size());
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void ImageSanitizationDone(ImageSanitizer::Status status,
                             const base::FilePath& path) {
    last_status_ = status;
    last_reported_path_ = path;
    if (done_callback_)
      std::move(done_callback_).Run();
  }

  void ImageSanitizerDecodedImage(const base::FilePath& path, SkBitmap image) {
    EXPECT_EQ(decoded_images()->find(path), decoded_images()->end());
    (*decoded_images())[path] = image;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  data_decoder::TestDataDecoderService test_data_decoder_service_;
  ImageSanitizer::Status last_status_;
  base::FilePath last_reported_path_;
  base::OnceClosure done_callback_;
  std::unique_ptr<ImageSanitizer> sanitizer_;
  base::ScopedTempDir temp_dir_;
  std::map<base::FilePath, SkBitmap> decoded_images_;

  DISALLOW_COPY_AND_ASSIGN(ImageSanitizerTest);
};

}  // namespace

TEST_F(ImageSanitizerTest, NoImagesProvided) {
  CreateAndStartSanitizer(std::set<base::FilePath>());
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kSuccess);
  EXPECT_TRUE(last_reported_path().empty());
}

TEST_F(ImageSanitizerTest, InvalidPathAbsolute) {
  base::FilePath normal_path(FILE_PATH_LITERAL("hello.png"));
#if defined(OS_WIN)
  base::FilePath absolute_path(FILE_PATH_LITERAL("c:\\Windows\\win32"));
#else
  base::FilePath absolute_path(FILE_PATH_LITERAL("/usr/bin/root"));
#endif
  CreateAndStartSanitizer({normal_path, absolute_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kImagePathError);
  EXPECT_EQ(last_reported_path(), absolute_path);
}

TEST_F(ImageSanitizerTest, InvalidPathReferenceParent) {
  base::FilePath good_path(FILE_PATH_LITERAL("hello.png"));
  base::FilePath bad_path(FILE_PATH_LITERAL("hello"));
  bad_path = bad_path.Append(base::FilePath::kParentDirectory)
                 .Append(base::FilePath::kParentDirectory)
                 .Append(base::FilePath::kParentDirectory)
                 .Append(FILE_PATH_LITERAL("usr"))
                 .Append(FILE_PATH_LITERAL("bin"));
  CreateAndStartSanitizer({good_path, bad_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kImagePathError);
  EXPECT_EQ(last_reported_path(), bad_path);
}

TEST_F(ImageSanitizerTest, ValidCase) {
  constexpr std::array<const base::FilePath::CharType* const, 10> kFileNames{
      {FILE_PATH_LITERAL("image0.png"), FILE_PATH_LITERAL("image1.png"),
       FILE_PATH_LITERAL("image2.png"), FILE_PATH_LITERAL("image3.png"),
       FILE_PATH_LITERAL("image4.png"), FILE_PATH_LITERAL("image5.png"),
       FILE_PATH_LITERAL("image6.png"), FILE_PATH_LITERAL("image7.png"),
       FILE_PATH_LITERAL("image8.png"), FILE_PATH_LITERAL("image9.png")}};
  std::set<base::FilePath> paths;
  for (const base::FilePath::CharType* file_name : kFileNames) {
    CreateValidImage(file_name);
    paths.insert(base::FilePath(file_name));
  }
  CreateAndStartSanitizer(paths);
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kSuccess);
  EXPECT_TRUE(last_reported_path().empty());
  // Make sure the image files are there and non empty, and that the
  // ImageSanitizerDecodedImage callback was invoked for every image.
  for (const auto& path : paths) {
    int64_t file_size = 0;
    base::FilePath full_path = GetImagePath().Append(path);
    EXPECT_TRUE(base::GetFileSize(full_path, &file_size));
    EXPECT_GT(file_size, 0);

    ASSERT_TRUE(base::ContainsKey(*decoded_images(), path));
    EXPECT_FALSE((*decoded_images())[path].drawsNothing());
  }
  // No extra images should have been reported.
  EXPECT_EQ(decoded_images()->size(), 10U);
}

TEST_F(ImageSanitizerTest, MissingImage) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("image.png");
  constexpr base::FilePath::CharType kNonExistingName[] =
      FILE_PATH_LITERAL("i_don_t_exist.png");
  CreateValidImage(kGoodPngName);
  base::FilePath good_png(kGoodPngName);
  base::FilePath bad_png(kNonExistingName);
  CreateAndStartSanitizer({good_png, bad_png});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kFileReadError);
  EXPECT_EQ(last_reported_path(), bad_png);
}

TEST_F(ImageSanitizerTest, InvalidImage) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("good.png");
  constexpr base::FilePath::CharType kBadPngName[] =
      FILE_PATH_LITERAL("bad.png");
  CreateValidImage(kGoodPngName);
  CreateInvalidImage(kBadPngName);
  base::FilePath good_png(kGoodPngName);
  base::FilePath bad_png(kBadPngName);
  CreateAndStartSanitizer({good_png, bad_png});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kDecodingError);
  EXPECT_EQ(last_reported_path(), bad_png);
}

}  // namespace extensions
