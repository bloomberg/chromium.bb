// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_group_model.h"
#include "ash/app_list/app_list_item_model.h"
#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/app_list_view.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "base/basictypes.h"
#include "ui/views/examples/examples_window.h"

namespace ash {
namespace shell {

namespace {

class WindowTypeLauncherItem : public ash::AppListItemModel {
 public:
  enum Type {
    TOPLEVEL_WINDOW = 0,
    NON_RESIZABLE_WINDOW,
    LOCK_SCREEN,
    WIDGETS_WINDOW,
    EXAMPLES_WINDOW,
    LAST_TYPE,
  };

  WindowTypeLauncherItem(Type type) : type_(type) {
    SetIcon(GetIcon(type));
    SetTitle(GetTitle(type));
  }

  static SkBitmap GetIcon(Type type) {
    static const SkColor kColors[] = {
        SkColorSetA(SK_ColorRED, 0x4F),
        SkColorSetA(SK_ColorGREEN, 0x4F),
        SkColorSetA(SK_ColorBLUE, 0x4F),
        SkColorSetA(SK_ColorYELLOW, 0x4F),
        SkColorSetA(SK_ColorCYAN, 0x4F),
    };

    SkBitmap icon;
    icon.setConfig(SkBitmap::kARGB_8888_Config,
                   ash::AppListItemView::kIconSize,
                   ash::AppListItemView::kIconSize);
    icon.allocPixels();
    icon.eraseColor(kColors[static_cast<int>(type) % arraysize(kColors)]);
    return icon;
  }

  static std::string GetTitle(Type type) {
    switch (type) {
      case TOPLEVEL_WINDOW:
        return "Create Window";
      case NON_RESIZABLE_WINDOW:
        return "Create Non-Resizable Window";
      case LOCK_SCREEN:
        return "Lock Screen";
      case WIDGETS_WINDOW:
        return "Show Example Widgets";
      case EXAMPLES_WINDOW:
        return "Open Views Examples Window";
      default:
        return "Unknown window type.";
    }
  }

  void Activate(int event_flags) {
     switch (type_) {
      case TOPLEVEL_WINDOW: {
        ToplevelWindow::CreateParams params;
        params.can_resize = true;
        ToplevelWindow::CreateToplevelWindow(params);
        break;
      }
      case NON_RESIZABLE_WINDOW: {
        ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
        break;
      }
      case LOCK_SCREEN: {
        CreateLockScreen();
        break;
      }
      case WIDGETS_WINDOW: {
        CreateWidgetsWindow();
        break;
      }
      case EXAMPLES_WINDOW: {
        views::examples::ShowExamplesWindow(false);
        break;
      }
      default:
        break;
    }
  }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncherItem);
};

class ExampleAppListViewDelegate : public ash::AppListViewDelegate {
 public:
  ExampleAppListViewDelegate() {}

 private:
  virtual void OnAppListItemActivated(ash::AppListItemModel* item,
                                      int event_flags) OVERRIDE {
    static_cast<WindowTypeLauncherItem*>(item)->Activate(event_flags);
  }

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListViewDelegate);
};

ash::AppListItemGroupModel* CreateGroup(
    const std::string& title,
    WindowTypeLauncherItem::Type start_type,
    WindowTypeLauncherItem::Type end_type) {
  ash::AppListItemGroupModel* group =
      new ash::AppListItemGroupModel(title);
  for (int i = static_cast<int>(start_type);
       i < static_cast<int>(end_type);
       ++i) {
    WindowTypeLauncherItem::Type type =
      static_cast<WindowTypeLauncherItem::Type>(i);
    group->AddItem(new WindowTypeLauncherItem(type));
  }
  return group;
}

}  // namespace

void BuildAppListModel(ash::AppListModel* model) {
  model->AddGroup(CreateGroup("Windows",
      WindowTypeLauncherItem::TOPLEVEL_WINDOW,
      WindowTypeLauncherItem::WIDGETS_WINDOW));
  model->AddGroup(CreateGroup("Samples",
      WindowTypeLauncherItem::WIDGETS_WINDOW,
      WindowTypeLauncherItem::LAST_TYPE));
}

ash::AppListViewDelegate* CreateAppListViewDelegate() {
  return new ExampleAppListViewDelegate;
}

}  // namespace shell
}  // namespace ash
