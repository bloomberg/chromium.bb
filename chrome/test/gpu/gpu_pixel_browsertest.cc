// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/test/gfx_test_utils.h"

#if defined(USE_WEBKIT_COMPOSITOR)
#include "ui/gfx/compositor/compositor_setup.h"
#elif defined(VIEWS_COMPOSITOR)
#include "ui/gfx/compositor/compositor.h"
#endif

namespace {

// Command line flag for overriding the default location for putting generated
// test images that do not match references.
const char kGeneratedDir[] = "generated-dir";
// Command line flag for overriding the default location for reference images.
const char kReferenceDir[] = "reference-dir";


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
  browser->GetSelectedWebContents()->GetContainerBounds(&container_rect);
  // Size cannot be negative, so use a point.
  gfx::Point correction(desired_size.width() - container_rect.size().width(),
                        desired_size.height() - container_rect.size().height());

  gfx::Rect window_rect = browser->window()->GetRestoredBounds();
  gfx::Size new_size = window_rect.size();
  new_size.Enlarge(correction.x(), correction.y());
  window_rect.set_size(new_size);
  browser->window()->SetBounds(window_rect);
}

}  // namespace

// Test fixture for GPU image comparison tests.
// TODO(kkania): Document how to add to/modify these tests.
class GpuPixelBrowserTest : public InProcessBrowserTest {
 public:
  GpuPixelBrowserTest() : ref_img_revision_no_older_than_(0) {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // This enables DOM automation for tab contents.
    EnableDOMAutomation();
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("gpu");

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kGeneratedDir))
      generated_img_dir_ = command_line->GetSwitchValuePath(kGeneratedDir);
    else
      generated_img_dir_ = test_data_dir_.AppendASCII("generated");
    if (command_line->HasSwitch(kReferenceDir))
      ref_img_dir_ = command_line->GetSwitchValuePath(kReferenceDir);
    else
      ref_img_dir_ = test_data_dir_.AppendASCII("gpu_reference");

    test_name_ = testing::UnitTest::GetInstance()->current_test_info()->name();
    const char* test_status_prefixes[] = {"DISABLED_", "FLAKY_", "FAILS_"};
    for (size_t i = 0; i < arraysize(test_status_prefixes); ++i) {
      ReplaceFirstSubstringAfterOffset(
          &test_name_, 0, test_status_prefixes[i], "");
    }

#if defined(USE_WEBKIT_COMPOSITOR)
    ui::DisableTestCompositor();
#elif defined(VIEWS_COMPOSITOR)
    ui::Compositor::set_compositor_factory_for_testing(NULL);
#endif
  }

  // Compares the generated bitmap with the appropriate reference image on disk.
  // Returns true iff the images were the same.
  //
  // If no valid reference image exists, save the generated bitmap to the disk.
  // The image format is:
  //     <test_name>_<revision>.png
  // E.g.,
  //     WebGLTeapot_19762.png
  // The number is the chromium revision that generated the image.
  //
  // On failure or on ref image generation, the image and diff image will be
  // written to disk. The formats are:
  //     FAIL_<ref_image_name>, DIFF_<ref_image_name>
  // E.g.,
  //     FAIL_WebGLTeapot_19762.png, DIFF_WebGLTeapot_19762.png
  bool CompareImages(const SkBitmap& gen_bmp) {
    SkBitmap ref_bmp_on_disk;
    const SkBitmap* ref_bmp;
    bool save_gen = false;
    bool save_diff = false;
    bool rt = true;
    if (ref_img_path_.empty() ||
        !ReadPNGFile(ref_img_path_, &ref_bmp_on_disk)) {
      chrome::VersionInfo chrome_version_info;
      FilePath img_revision_path = ref_img_dir_.AppendASCII(
          test_name_ + "_" + chrome_version_info.LastChange() + ".png");
      if (!WritePNGFile(gen_bmp, img_revision_path)) {
        LOG(ERROR) << "Can't save generated image to: "
                   << img_revision_path.value()
                   << " as future reference.";
        rt = false;
      }
      if (!ref_img_path_.empty()) {
        LOG(ERROR) << "Can't read the local ref image: "
                   << ref_img_path_.value()
                   << ", reset it.";
        file_util::Delete(ref_img_path_, false);
        rt = false;
      }
      ref_img_path_ = img_revision_path;
      // If we re-generate the ref image, we save the gen and diff images so
      // the ref image can be uploaded to the server and be viewed later.
      save_gen = true;
      save_diff = true;
      ref_bmp = &gen_bmp;
    } else {
      ref_bmp = &ref_bmp_on_disk;
    }

    SkBitmap diff_bmp;
    if (ref_bmp->width() != gen_bmp.width() ||
        ref_bmp->height() != gen_bmp.height()) {
      LOG(ERROR)
          << "Dimensions do not match (Expected) vs (Actual):"
          << "(" << ref_bmp->width() << "x" << ref_bmp->height()
              << ") vs. "
          << "(" << gen_bmp.width() << "x" << gen_bmp.height() << ")";
      save_gen = true;
      rt = false;
    } else {
      // Compare pixels and create a simple diff image.
      int diff_pixels_count = 0;
      diff_bmp.setConfig(SkBitmap::kARGB_8888_Config,
                         gen_bmp.width(), gen_bmp.height());
      diff_bmp.allocPixels();
      diff_bmp.eraseColor(SK_ColorWHITE);
      SkAutoLockPixels lock_bmp(gen_bmp);
      SkAutoLockPixels lock_ref_bmp(*ref_bmp);
      SkAutoLockPixels lock_diff_bmp(diff_bmp);
      // The reference images were saved with no alpha channel. Use the mask to
      // set alpha to 0.
      uint32_t kAlphaMask = 0x00FFFFFF;
      for (int x = 0; x < gen_bmp.width(); ++x) {
        for (int y = 0; y < gen_bmp.height(); ++y) {
          if ((*gen_bmp.getAddr32(x, y) & kAlphaMask) !=
              (*ref_bmp->getAddr32(x, y) & kAlphaMask)) {
            ++diff_pixels_count;
            *diff_bmp.getAddr32(x, y) = 192 << 16;  // red
          }
        }
      }
      if (diff_pixels_count > 0) {
        LOG(ERROR) << diff_pixels_count
                   << " pixels do not match.";
        save_gen = true;
        save_diff = true;
        rt = false;
      }
    }

    std::string ref_img_filename = ref_img_path_.BaseName().MaybeAsASCII();
    if (save_gen) {
      FilePath img_fail_path = generated_img_dir_.AppendASCII(
          "FAIL_" + ref_img_filename);
      if (!WritePNGFile(gen_bmp, img_fail_path)) {
        LOG(ERROR) << "Can't save generated image to: "
                   << img_fail_path.value();
      }
    }
    if (save_diff) {
      FilePath img_diff_path = generated_img_dir_.AppendASCII(
          "DIFF_" + ref_img_filename);
      if (!WritePNGFile(diff_bmp, img_diff_path)) {
        LOG(ERROR) << "Can't save generated diff image to: "
                   << img_diff_path.value();
      }
    }
    return rt;
  }

  // This has to be called by every pixel test. If no specific revision is
  // required, just call it with 0.
  void SetRefImageRevisionNoOlderThan(int64 revision) {
    ref_img_revision_no_older_than_ = revision;
    ObtainLocalRefImageFilePath();
  }

 protected:
  FilePath test_data_dir_;

 private:
  FilePath generated_img_dir_;
  FilePath ref_img_dir_;
  FilePath ref_img_path_;
  // The name of the test, with any special prefixes dropped.
  std::string test_name_;

  // Any local ref image generated from older revision is ignored.
  int64 ref_img_revision_no_older_than_;

  // If no valid local ref image is located, the ref_img_path_ remains
  // empty.
  void ObtainLocalRefImageFilePath() {
    FilePath filter;
    filter = filter.AppendASCII(test_name_ + "_*.png");
    file_util::FileEnumerator locator(ref_img_dir_,
                                      false,  // non recursive
                                      file_util::FileEnumerator::FILES,
                                      filter.value());
    int64 max_revision = 0;
    std::vector<FilePath> outdated_ref_imgs;
    for (FilePath full_path = locator.Next();
         !full_path.empty();
         full_path = locator.Next()) {
      std::string filename =
          full_path.BaseName().RemoveExtension().MaybeAsASCII();
      std::string revision_string =
          filename.substr(test_name_.length() + 1);
      int64 revision = 0;
      bool converted = base::StringToInt64(revision_string, &revision);
      if (!converted)
        continue;
      if (revision < ref_img_revision_no_older_than_ ||
          revision < max_revision) {
        outdated_ref_imgs.push_back(full_path);
        continue;
      }
      ref_img_path_ = full_path;
      max_revision = revision;
    }
    for (size_t i = 0; i < outdated_ref_imgs.size(); ++i)
      file_util::Delete(outdated_ref_imgs[i], false);
  }

  DISALLOW_COPY_AND_ASSIGN(GpuPixelBrowserTest);
};

// Enable initially only on Windows and progressively enable on more
// platforms.
// Bug tracking test failure on Windows/Mac: http://crbug.com/95214
// Bug tracking test failure on Linux: http://crbug.com/95214
#if defined(USE_AURA)
#define MAYBE_WebGLTeapot DISABLED_WebGLTeapot
#else
#define MAYBE_WebGLTeapot WebGLTeapot
#endif

IN_PROC_BROWSER_TEST_F(GpuPixelBrowserTest, MAYBE_WebGLTeapot) {
  // If test baseline needs to be updated after a given revision, update the
  // revision number in SetRefImageNoOlderThan(#revision).
  SetRefImageRevisionNoOlderThan(116356);

  gfx::Size container_size(500, 500);
  ResizeTabContainer(browser(), container_size);

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::DOMMessageQueue message_queue;
  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(test_data_dir_.AppendASCII("webgl_teapot").
          AppendASCII("teapot.html")));

  // Wait for message from teapot page indicating the GL calls have been issued.
  ASSERT_TRUE(message_queue.WaitForMessage(NULL));

  std::vector<unsigned char> screenshot_png;

  gfx::Rect root_bounds = browser()->window()->GetBounds();
  gfx::Rect tab_contents_bounds;
  browser()->GetSelectedWebContents()->GetContainerBounds(&tab_contents_bounds);

  gfx::Rect snapshot_bounds(tab_contents_bounds.x() - root_bounds.x(),
                            tab_contents_bounds.y() - root_bounds.y(),
                            tab_contents_bounds.width(),
                            tab_contents_bounds.height());

  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  bool success = browser::GrabWindowSnapshot(native_window, &screenshot_png,
                                             snapshot_bounds);
  ASSERT_TRUE(success);

  SkBitmap bitmap;
  success = gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned char*>(&*screenshot_png.begin()),
      screenshot_png.size(),
      &bitmap);

  ASSERT_TRUE(success);
  ASSERT_TRUE(CompareImages(bitmap));
}
