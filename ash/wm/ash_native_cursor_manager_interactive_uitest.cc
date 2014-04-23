// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>

#include "ui/gfx/x/x11_types.h"
#endif

namespace ash {

class AshNativeCursorManagerTest : public test::AshTestBase {
 public:
  AshNativeCursorManagerTest() {}
  virtual ~AshNativeCursorManagerTest() {}

  virtual void SetUp() OVERRIDE {
    gfx::GLSurface::InitializeOneOffForTests();

    ui::RegisterPathProvider();
    ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
    base::FilePath resources_pack_path;
    PathService::Get(base::DIR_MODULE, &resources_pack_path);
    resources_pack_path =
        resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);

    test::AshTestBase::SetUp();
  }
};

namespace {

DisplayInfo CreateDisplayInfo(int64 id,
                              const gfx::Rect& bounds,
                              float device_scale_factor) {
  DisplayInfo info(id, "", false);
  info.SetBounds(bounds);
  info.set_device_scale_factor(device_scale_factor);
  return info;
}

void MoveMouseSync(aura::Window* window, int x, int y) {
#if defined(USE_X11)
  XWarpPointer(gfx::GetXDisplay(),
               None,
               window->GetHost()->GetAcceleratedWidget(),
               0, 0, 0, 0,
               x, y);
#endif
  // Send and wait for a key event to make sure that mouse
  // events are fully processed.
  base::RunLoop loop;
  ui_controls::SendKeyPressNotifyWhenDone(
      window,
      ui::VKEY_SPACE,
      false,
      false,
      false,
      false,
      loop.QuitClosure());
  loop.Run();
}

}  // namespace

#if defined(USE_X11)
#define MAYBE_CursorChangeOnEnterNotify CursorChangeOnEnterNotify
#else
#define MAYBE_CursorChangeOnEnterNotify DISABLED_CursorChangeOnEnterNotify
#endif

TEST_F(AshNativeCursorManagerTest, MAYBE_CursorChangeOnEnterNotify) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  test::CursorManagerTestApi test_api(cursor_manager);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayInfo display_info1 =
      CreateDisplayInfo(10, gfx::Rect(0, 0, 500, 300), 1.0f);
  DisplayInfo display_info2 =
      CreateDisplayInfo(20, gfx::Rect(500, 0, 500, 300), 2.0f);
  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);
  display_manager->OnNativeDisplaysChanged(display_info_list);

  MoveMouseSync(Shell::GetAllRootWindows()[0], 10, 10);
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());

  MoveMouseSync(Shell::GetAllRootWindows()[0], 600, 10);
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
}

}  // namespace ash
