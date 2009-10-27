#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

static SkBitmap LoadIcon(const std::string& filename) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions").AppendASCII(filename);

  std::string file_contents;
  file_util::ReadFileToString(path, &file_contents);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());

  SkBitmap bitmap;
  webkit_glue::ImageDecoder decoder;
  bitmap = decoder.Decode(data, file_contents.length());

  return bitmap;
}

static bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2) {
  void* addr1 = NULL;
  void* addr2 = NULL;

  bitmap1.lockPixels();
  addr1 = bitmap1.getAddr32(0, 0);
  bitmap1.unlockPixels();

  bitmap2.lockPixels();
  addr2 = bitmap2.getAddr32(0, 0);
  bitmap2.unlockPixels();

  return 0 == memcmp(addr1, addr2, bitmap1.getSize());
}

TEST(ExtensionAction2Test, TabSpecificState) {
  ExtensionAction2 action;

  // title
  ASSERT_EQ("", action.GetTitle(1));
  action.SetTitle(ExtensionAction2::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action.GetTitle(1));
  ASSERT_EQ("foo", action.GetTitle(100));
  action.SetTitle(100, "bar");
  ASSERT_EQ("foo", action.GetTitle(1));
  ASSERT_EQ("bar", action.GetTitle(100));
  action.SetTitle(ExtensionAction2::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action.GetTitle(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action.GetTitle(100));

  // icon
  SkBitmap icon1 = LoadIcon("icon1.png");
  SkBitmap icon2 = LoadIcon("icon2.png");
  ASSERT_TRUE(action.GetIcon(1).isNull());
  action.SetIcon(ExtensionAction2::kDefaultTabId, icon1);
  ASSERT_TRUE(BitmapsAreEqual(icon1, action.GetIcon(100)));
  action.SetIcon(100, icon2);
  ASSERT_TRUE(BitmapsAreEqual(icon1, action.GetIcon(1)));
  ASSERT_TRUE(BitmapsAreEqual(icon2, action.GetIcon(100)));

  // icon index
  ASSERT_EQ(-1, action.GetIconIndex(1));
  action.icon_paths()->push_back("foo.png");
  action.icon_paths()->push_back("bar.png");
  action.SetIconIndex(ExtensionAction2::kDefaultTabId, 1);
  ASSERT_EQ(1, action.GetIconIndex(1));
  ASSERT_EQ(1, action.GetIconIndex(100));
  action.SetIconIndex(100, 0);
  ASSERT_EQ(0, action.GetIconIndex(100));
  ASSERT_EQ(1, action.GetIconIndex(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(1, action.GetIconIndex(100));
  ASSERT_EQ(1, action.GetIconIndex(1));

  // visibility
  ASSERT_EQ(false, action.GetIsVisible(1));
  action.SetIsVisible(ExtensionAction2::kDefaultTabId, true);
  ASSERT_EQ(true, action.GetIsVisible(1));
  ASSERT_EQ(true, action.GetIsVisible(100));
  action.SetIsVisible(ExtensionAction2::kDefaultTabId, false);
  ASSERT_EQ(false, action.GetIsVisible(1));
  ASSERT_EQ(false, action.GetIsVisible(100));
  action.SetIsVisible(100, true);
  ASSERT_EQ(false, action.GetIsVisible(1));
  ASSERT_EQ(true, action.GetIsVisible(100));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(false, action.GetIsVisible(1));
  ASSERT_EQ(false, action.GetIsVisible(100));

  // badge text
  ASSERT_EQ("", action.GetBadgeText(1));
  action.SetBadgeText(ExtensionAction2::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action.GetBadgeText(1));
  ASSERT_EQ("foo", action.GetBadgeText(100));
  action.SetBadgeText(100, "bar");
  ASSERT_EQ("foo", action.GetBadgeText(1));
  ASSERT_EQ("bar", action.GetBadgeText(100));
  action.SetBadgeText(ExtensionAction2::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action.GetBadgeText(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action.GetBadgeText(100));

  // badge text color
  ASSERT_EQ(0x00000000u, action.GetBadgeTextColor(1));
  action.SetBadgeTextColor(ExtensionAction2::kDefaultTabId, 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(1));
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(100));
  action.SetBadgeTextColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(1));
  ASSERT_EQ(0xFF00FF00u, action.GetBadgeTextColor(100));
  action.SetBadgeTextColor(ExtensionAction2::kDefaultTabId, 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeTextColor(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeTextColor(100));

  // badge background color
  ASSERT_EQ(0x00000000u, action.GetBadgeBackgroundColor(1));
  action.SetBadgeBackgroundColor(ExtensionAction2::kDefaultTabId,
                                 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(100));
  action.SetBadgeBackgroundColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFF00FF00u, action.GetBadgeBackgroundColor(100));
  action.SetBadgeBackgroundColor(ExtensionAction2::kDefaultTabId,
                                 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeBackgroundColor(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeBackgroundColor(100));
}
