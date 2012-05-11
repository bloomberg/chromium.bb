// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "base/basictypes.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/views/examples/examples_window.h"

namespace ash {
namespace shell {

namespace {

class WindowTypeLauncherItem : public app_list::AppListItemModel {
 public:
  enum Type {
    TOPLEVEL_WINDOW = 0,
    NON_RESIZABLE_WINDOW,
    LOCK_SCREEN,
    WIDGETS_WINDOW,
    EXAMPLES_WINDOW,
    LAST_TYPE,
  };

  explicit WindowTypeLauncherItem(Type type) : type_(type) {
    SetIcon(GetIcon(type));
    SetTitle(GetTitle(type));
  }

  static SkBitmap GetIcon(Type type) {
    static const SkColor kColors[] = {
        SK_ColorRED,
        SK_ColorGREEN,
        SK_ColorBLUE,
        SK_ColorYELLOW,
        SK_ColorCYAN,
    };

    const int kIconSize = 128;
    SkBitmap icon;
    icon.setConfig(SkBitmap::kARGB_8888_Config, kIconSize, kIconSize);
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
        Shell::GetInstance()->delegate()->LockScreen();
        break;
      }
      case WIDGETS_WINDOW: {
        CreateWidgetsWindow();
        break;
      }
      case EXAMPLES_WINDOW: {
#if !defined(OS_MACOSX)
        views::examples::ShowExamplesWindow(
            views::examples::DO_NOTHING_ON_CLOSE,
            ash::Shell::GetInstance()->browser_context());
#endif
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

class ExampleAppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  ExampleAppListViewDelegate() : model_(NULL) {}

 private:
  // Overridden from ash::AppListViewDelegate:
  virtual void SetModel(app_list::AppListModel* model) OVERRIDE {
    model_ = model;
  }

  virtual void UpdateModel(const std::string& query) OVERRIDE {
    DCHECK(model_ && model_->item_count() == 0);

    for (int i = 0;
         i < static_cast<int>(WindowTypeLauncherItem::LAST_TYPE);
         ++i) {
      WindowTypeLauncherItem::Type type =
          static_cast<WindowTypeLauncherItem::Type>(i);

      std::string title = WindowTypeLauncherItem::GetTitle(type);
      if (title.find(query) != std::string::npos)
        model_->AddItem(new WindowTypeLauncherItem(type));
    }
  }

  virtual void OnAppListItemActivated(app_list::AppListItemModel* item,
                                      int event_flags) OVERRIDE {
    static_cast<WindowTypeLauncherItem*>(item)->Activate(event_flags);
  }

  virtual void Close() OVERRIDE {
    DCHECK(ash::Shell::HasInstance());
    if (Shell::GetInstance()->GetAppListTargetVisibility())
      Shell::GetInstance()->ToggleAppList();
  }

  app_list::AppListModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListViewDelegate);
};

}  // namespace

app_list::AppListViewDelegate* CreateAppListViewDelegate() {
  return new ExampleAppListViewDelegate;
}

}  // namespace shell
}  // namespace ash
