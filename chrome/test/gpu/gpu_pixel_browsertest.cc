// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "app/app_switches.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/gpu_data_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/gpu_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/gpu_info.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"

namespace {

// Command line flag for forcing the machine's GPU to be used instead of OSMesa.
const char kUseGpuInTests[] = "use-gpu-in-tests";

// Command line flag for overriding the default location for putting generated
// test images that do not match references.
const char kGeneratedDir[] = "generated-dir";

// Reads and decodes a PNG image to a bitmap. Returns true on success. The PNG
// should have been encoded using |gfx::PNGCodec::Encode|.
bool ReadPNGFile(const FilePath& file_path, SkBitmap* bitmap) {
  DCHECK(bitmap);
  std::string png_data;
  return file_util::ReadFileToString(file_path, &png_data) &&
         gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&png_data[0]),
                               png_data.length(),
                               bitmap);
}

// Encodes a bitmap into a PNG and write to disk. Returns true on success. The
// parent directory does not have to exist.
bool WritePNGFile(const SkBitmap& bitmap, const FilePath& file_path) {
  std::vector<unsigned char> png_data;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data) &&
      file_util::CreateDirectory(file_path.DirName())) {
    int bytes_written = file_util::WriteFile(
        file_path, reinterpret_cast<char*>(&png_data[0]), png_data.size());
    if (bytes_written == static_cast<int>(png_data.size()))
      return true;
  }
  return false;
}

// Resizes the browser window so that the tab's contents are at a given size.
void ResizeTabContainer(Browser* browser, const gfx::Size& desired_size) {
  gfx::Rect container_rect;
  browser->GetSelectedTabContents()->GetContainerBounds(&container_rect);
  // Size cannot be negative, so use a point.
  gfx::Point correction(desired_size.width() - container_rect.size().width(),
                        desired_size.height() - container_rect.size().height());

  gfx::Rect window_rect = browser->window()->GetRestoredBounds();
  gfx::Size new_size = window_rect.size();
  new_size.Enlarge(correction.x(), correction.y());
  window_rect.set_size(new_size);
  browser->window()->SetBounds(window_rect);
}

// Obtains info about the GPU. Iff the info is collected, |client_info| will be
// set and true will be returned. Here we only need vendor_id and device_id,
// which should be always collected during browser startup, so no need to run
// GPU information collection here.
// This will return false if we are running in a virtualized environment.
bool GetGPUInfo(GPUInfo* client_info) {
  CHECK(client_info);
  const GPUInfo& info = GpuDataManager::GetInstance()->gpu_info();
  if (info.vendor_id == 0 || info.device_id == 0)
    return false;
  *client_info = info;
  return true;
}

}  // namespace

// Test fixture for GPU image comparison tests.
// TODO(kkania): Document how to add to/modify these tests.
class GpuPixelBrowserTest : public InProcessBrowserTest {
 public:
  GpuPixelBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // This enables DOM automation for tab contents.
    EnableDOMAutomation();

    // These tests by default use OSMesa. This can be changed if the |kUseGL|
    // switch is explicitly set to something else or if |kUseGpuInTests| is
    // present.
    if (command_line->HasSwitch(switches::kUseGL)) {
      using_gpu_ = command_line->GetSwitchValueASCII(switches::kUseGL) !=
          gfx::kGLImplementationOSMesaName;
    } else if (command_line->HasSwitch(kUseGpuInTests)) {
      using_gpu_ = true;
    } else {
      // OSMesa will be used by default.
      EXPECT_TRUE(test_launcher_utils::OverrideGLImplementation(
          command_line,
          gfx::kGLImplementationOSMesaName));
#if defined(OS_MACOSX)
      // Accelerated compositing does not work with OSMesa. AcceleratedSurface
      // assumes GL contexts are native.
      command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
#endif
      using_gpu_ = false;
    }
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("gpu");

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kGeneratedDir))
      generated_img_dir_ = command_line->GetSwitchValuePath(kGeneratedDir);
    else
      generated_img_dir_ = test_data_dir_.AppendASCII("generated");

    if (using_gpu_)
      reference_img_dir_ = test_data_dir_.AppendASCII("gpu_reference");
    else
      reference_img_dir_ = test_data_dir_.AppendASCII("sw_reference");

    test_name_ = testing::UnitTest::GetInstance()->current_test_info()->name();
    const char* test_status_prefixes[] = {"DISABLED_", "FLAKY_", "FAILS_"};
    for (size_t i = 0; i < arraysize(test_status_prefixes); ++i) {
      ReplaceFirstSubstringAfterOffset(
          &test_name_, 0, test_status_prefixes[i], "");
    }
  }

  // Compares the generated bitmap with the appropriate reference image on disk.
  // The reference image is determined by the name of the test, the |postfix|,
  // and the vendor and device ID. Returns true iff the images were the same.
  //
  // |postfix|, is an optional name that can be appended onto the images name to
  // distinguish between images from a single test.
  //
  // On failure, the image will be written to disk, and can be collected as a
  // baseline. The image format is:
  //     <test_name>_<postfix>_<os>_<vendor_id>-<device_id>.png
  // E.g.,
  //     WebGLFishTank_FirstTab_120981-1201.png
  bool CompareImages(const SkBitmap& gen_bmp, const std::string& postfix) {
    // Determine the name of the image.
    std::string img_name = test_name_;
    if (postfix.length())
      img_name += "_" + postfix;
#if defined(OS_WIN)
      const char* os_label = "win";
#elif defined(OS_MACOSX)
      const char* os_label = "mac";
#elif defined(OS_LINUX)
      const char* os_label = "linux";
#else
#error "Not implemented for this platform"
#endif
    if (using_gpu_) {
      GPUInfo info;
      if (!GetGPUInfo(&info)) {
        LOG(ERROR) << "Could not get gpu info";
        return false;
      }
      img_name = base::StringPrintf("%s_%s_%04x-%04x.png",
          img_name.c_str(), os_label, info.vendor_id, info.device_id);
    } else {
      img_name = base::StringPrintf("%s_%s_mesa.png",
          img_name.c_str(), os_label);
    }

    // Read the reference image and verify the images' dimensions are equal.
    FilePath ref_img_path = reference_img_dir_.AppendASCII(img_name);
    SkBitmap ref_bmp;
    bool should_compare = true;
    if (!ReadPNGFile(ref_img_path, &ref_bmp)) {
      LOG(ERROR) << "Cannot read reference image: " << ref_img_path.value();
      should_compare = false;
    }
    if (should_compare &&
        (ref_bmp.width() != gen_bmp.width() ||
         ref_bmp.height() != gen_bmp.height())) {
      LOG(ERROR)
          << "Dimensions do not match (Expected) vs (Actual):"
          << "(" << ref_bmp.width() << "x" << ref_bmp.height()
              << ") vs. "
          << "(" << gen_bmp.width() << "x" << gen_bmp.height() << ")";
      should_compare = false;
    }

    // Compare pixels and create a simple diff image.
    int diff_pixels_count = 0;
    SkBitmap diff_bmp;
    if (should_compare) {
      diff_bmp.setConfig(SkBitmap::kARGB_8888_Config,
                         gen_bmp.width(), gen_bmp.height());
      diff_bmp.allocPixels();
      diff_bmp.eraseColor(SK_ColorWHITE);
      SkAutoLockPixels lock_bmp(gen_bmp);
      SkAutoLockPixels lock_ref_bmp(ref_bmp);
      SkAutoLockPixels lock_diff_bmp(diff_bmp);
      // The reference images were saved with no alpha channel. Use the mask to
      // set alpha to 0.
      uint32_t kAlphaMask = 0x00FFFFFF;
      for (int x = 0; x < gen_bmp.width(); ++x) {
        for (int y = 0; y < gen_bmp.height(); ++y) {
          if ((*gen_bmp.getAddr32(x, y) & kAlphaMask) !=
              (*ref_bmp.getAddr32(x, y) & kAlphaMask)) {
            ++diff_pixels_count;
            *diff_bmp.getAddr32(x, y) = 192 << 16;  // red
          }
        }
      }
    }

    // Write the generated and diff images if the comparison failed.
    if (!should_compare || diff_pixels_count != 0) {
      FilePath gen_img_path = generated_img_dir_.AppendASCII(img_name);
      if (WritePNGFile(gen_bmp, gen_img_path)) {
        // Output the generated image path to the log for easy parsing later.
        std::cout << "*GEN_IMG=" << gen_img_path.value() << std::endl;
      } else {
        LOG(WARNING) << "Could not write generated image: "
                     << gen_img_path.value();
      }
      if (should_compare) {
        WritePNGFile(diff_bmp,
                     generated_img_dir_.AppendASCII("diff-" + img_name));
        LOG(ERROR) << "Images differ by pixel count: " << diff_pixels_count;
      }
      return false;
    }
    return true;
  }

 protected:
  FilePath test_data_dir_;

 private:
  FilePath reference_img_dir_;
  FilePath generated_img_dir_;
  // The name of the test, with any special prefixes dropped.
  std::string test_name_;
  // Whether the gpu, or OSMesa is being used for rendering.
  bool using_gpu_;

  DISALLOW_COPY_AND_ASSIGN(GpuPixelBrowserTest);
};

IN_PROC_BROWSER_TEST_F(GpuPixelBrowserTest, WebGLTeapot) {
  ui_test_utils::DOMMessageQueue message_queue;
  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(test_data_dir_.AppendASCII("webgl_teapot").
          AppendASCII("teapot.html")));

  // Wait for message from teapot page indicating the GL calls have been issued.
  ASSERT_TRUE(message_queue.WaitForMessage(NULL));

  SkBitmap bitmap;
  gfx::Size container_size(500, 500);
  ResizeTabContainer(browser(), container_size);
  ASSERT_TRUE(ui_test_utils::TakeRenderWidgetSnapshot(
      browser()->GetSelectedTabContents()->render_view_host(),
      container_size, &bitmap));
  ASSERT_TRUE(CompareImages(bitmap, ""));
}
