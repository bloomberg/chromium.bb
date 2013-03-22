// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/common/gpu_feature_type.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOsVersion[] = "10.6.4";

namespace content {

class GpuBlacklistTest : public testing::Test {
 public:
  GpuBlacklistTest() { }

  virtual ~GpuBlacklistTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  void RunFeatureTest(
      const std::string feature_name, GpuFeatureType feature_type) {
    const std::string json =
        "{\n"
        "  \"name\": \"gpu blacklist\",\n"
        "  \"version\": \"0.1\",\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"id\": 1,\n"
        "      \"os\": {\n"
        "        \"type\": \"macosx\"\n"
        "      },\n"
        "      \"vendor_id\": \"0x10de\",\n"
        "      \"device_id\": [\"0x0640\"],\n"
        "      \"features\": [\n"
        "        \"" +
        feature_name +
        "\"\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}";

    scoped_ptr<GpuBlacklist> blacklist(GpuBlacklist::Create());
    EXPECT_TRUE(blacklist->LoadList(json, GpuBlacklist::kAllOs));
    int type = blacklist->MakeDecision(
        GpuBlacklist::kOsMacosx, kOsVersion, gpu_info());
    EXPECT_EQ(static_cast<int>(feature_type), type);
  }

 protected:
  virtual void SetUp() {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.machine_model = "MacBookPro 7.1";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    gpu_info_.performance_stats.graphics = 5.0;
    gpu_info_.performance_stats.gaming = 5.0;
    gpu_info_.performance_stats.overall = 5.0;
  }

  virtual void TearDown() {
  }

 private:
  GPUInfo gpu_info_;
};

TEST_F(GpuBlacklistTest, CurrentBlacklistValidation) {
  base::FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("content"))
               .Append(FILE_PATH_LITERAL("browser"))
               .Append(FILE_PATH_LITERAL("gpu"))
               .Append(FILE_PATH_LITERAL("software_rendering_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(data_file_size,
            file_util::ReadFile(data_file, data.get(), data_file_size));
  std::string json_string(data.get(), data_file_size);
  scoped_ptr<GpuBlacklist> blacklist(GpuBlacklist::Create());
  EXPECT_TRUE(blacklist->LoadList(json_string, GpuBlacklist::kAllOs));
  EXPECT_FALSE(blacklist->contains_unknown_fields());
}

#define GPU_BLACKLIST_FEATURE_TEST(test_name, feature_name, feature_type) \
TEST_F(GpuBlacklistTest, test_name) {                                     \
  RunFeatureTest(feature_name, feature_type);                             \
}

GPU_BLACKLIST_FEATURE_TEST(Accelerated2DCanvas,
                           "accelerated_2d_canvas",
                           GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)

GPU_BLACKLIST_FEATURE_TEST(AcceleratedCompositing,
                           "accelerated_compositing",
                           GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)

GPU_BLACKLIST_FEATURE_TEST(WebGL,
                           "webgl",
                           GPU_FEATURE_TYPE_WEBGL)

GPU_BLACKLIST_FEATURE_TEST(Multisampling,
                           "multisampling",
                           GPU_FEATURE_TYPE_MULTISAMPLING)

GPU_BLACKLIST_FEATURE_TEST(Flash3D,
                           "flash_3d",
                           GPU_FEATURE_TYPE_FLASH3D)

GPU_BLACKLIST_FEATURE_TEST(FlashStage3D,
                           "flash_stage3d",
                           GPU_FEATURE_TYPE_FLASH_STAGE3D)

GPU_BLACKLIST_FEATURE_TEST(FlashStage3DBaseline,
                           "flash_stage3d_baseline",
                           GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE)

GPU_BLACKLIST_FEATURE_TEST(TextureSharing,
                           "texture_sharing",
                           GPU_FEATURE_TYPE_TEXTURE_SHARING)

GPU_BLACKLIST_FEATURE_TEST(AcceleratedVideoDecode,
                           "accelerated_video_decode",
                           GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE)

GPU_BLACKLIST_FEATURE_TEST(Css3D,
                           "3d_css",
                           GPU_FEATURE_TYPE_3D_CSS)

GPU_BLACKLIST_FEATURE_TEST(AcceleratedVideo,
                           "accelerated_video",
                           GPU_FEATURE_TYPE_ACCELERATED_VIDEO)

GPU_BLACKLIST_FEATURE_TEST(PanelFitting,
                           "panel_fitting",
                           GPU_FEATURE_TYPE_PANEL_FITTING)

GPU_BLACKLIST_FEATURE_TEST(ForceCompositingMode,
                           "force_compositing_mode",
                           GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE)

GPU_BLACKLIST_FEATURE_TEST(All,
                           "all",
                           GPU_FEATURE_TYPE_ALL)

}  // namespace content
