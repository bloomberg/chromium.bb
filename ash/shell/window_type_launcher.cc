// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_type_launcher.h"

#include "ash/root_window_controller.h"
#include "ash/screensaver/screensaver_view.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/panel_window.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/test/child_modal_window.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/examples/examples_window_with_content.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

using views::MenuItemView;
using views::MenuRunner;

namespace ash {
namespace shell {

namespace {

SkColor g_colors[] = { SK_ColorRED,
                       SK_ColorYELLOW,
                       SK_ColorBLUE,
                       SK_ColorGREEN };
int g_color_index = 0;

class ModalWindow : public views::WidgetDelegateView,
                    public views::ButtonListener {
 public:
  explicit ModalWindow(ui::ModalType modal_type)
      : modal_type_(modal_type),
        color_(g_colors[g_color_index]),
        open_button_(new views::LabelButton(this,
                                            base::ASCIIToUTF16("Moar!"))) {
    ++g_color_index %= arraysize(g_colors);
    open_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(open_button_);
  }
  virtual ~ModalWindow() {
  }

  static void OpenModalWindow(aura::Window* parent, ui::ModalType modal_type) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new ModalWindow(modal_type),
                                              parent);
    widget->GetNativeView()->SetName("ModalWindow");
    widget->Show();
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(200, 200);
  }
  virtual void Layout() OVERRIDE {
    gfx::Size open_ps = open_button_->GetPreferredSize();
    gfx::Rect local_bounds = GetLocalBounds();
    open_button_->SetBounds(
        5, local_bounds.bottom() - open_ps.height() - 5,
        open_ps.width(), open_ps.height());
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return base::ASCIIToUTF16("Modal Window");
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return modal_type_;
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK(sender == open_button_);
    OpenModalWindow(GetWidget()->GetNativeView(), modal_type_);
  }

 private:
  ui::ModalType modal_type_;
  SkColor color_;
  views::LabelButton* open_button_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class NonModalTransient : public views::WidgetDelegateView {
 public:
  NonModalTransient()
      : color_(g_colors[g_color_index]) {
    ++g_color_index %= arraysize(g_colors);
  }
  virtual ~NonModalTransient() {
  }

  static void OpenNonModalTransient(aura::Window* parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
    widget->GetNativeView()->SetName("NonModalTransient");
    widget->Show();
  }

  static void ToggleNonModalTransient(aura::Window* parent) {
    if (!non_modal_transient_) {
      non_modal_transient_ =
          views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
      non_modal_transient_->GetNativeView()->SetName("NonModalTransient");
    }
    if (non_modal_transient_->IsVisible())
      non_modal_transient_->Hide();
    else
      non_modal_transient_->Show();
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(250, 250);
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return base::ASCIIToUTF16("Non-Modal Transient");
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (GetWidget() == non_modal_transient_)
      non_modal_transient_ = NULL;

    delete this;
  }

 private:
  SkColor color_;

  static views::Widget* non_modal_transient_;

  DISALLOW_COPY_AND_ASSIGN(NonModalTransient);
};

// static
views::Widget* NonModalTransient::non_modal_transient_ = NULL;

void AddViewToLayout(views::GridLayout* layout, views::View* view) {
  layout->StartRow(0, 0);
  layout->AddView(view);
  layout->AddPaddingRow(0, 5);
}

}  // namespace

void InitWindowTypeLauncher() {
  views::Widget* widget =
      views::Widget::CreateWindowWithContextAndBounds(
          new WindowTypeLauncher,
          Shell::GetPrimaryRootWindow(),
          gfx::Rect(120, 150, 300, 410));
  widget->GetNativeView()->SetName("WindowTypeLauncher");
  wm::SetShadowType(widget->GetNativeView(),
                               wm::SHADOW_TYPE_RECTANGULAR);
  widget->Show();
}

WindowTypeLauncher::WindowTypeLauncher()
    : create_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Create Window"))),
      panel_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Create Panel"))),
      create_nonresizable_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Create Non-Resizable Window"))),
      bubble_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Create Pointy Bubble"))),
      lock_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Lock Screen"))),
      widgets_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Show Example Widgets"))),
      system_modal_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Open System Modal Window"))),
      window_modal_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Open Window Modal Window"))),
      child_modal_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Open Child Modal Window"))),
      transient_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Open Non-Modal Transient Window"))),
      examples_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Open Views Examples Window"))),
      show_hide_window_button_(new views::LabelButton(
          this, base::ASCIIToUTF16("Show/Hide a Window"))),
      show_screensaver_(new views::LabelButton(
          this, base::ASCIIToUTF16("Show the Screensaver [for 5 seconds]"))),
      show_web_notification_(new views::LabelButton(
          this, base::ASCIIToUTF16("Show a web/app notification"))) {
  create_button_->SetStyle(views::Button::STYLE_BUTTON);
  panel_button_->SetStyle(views::Button::STYLE_BUTTON);
  create_nonresizable_button_->SetStyle(views::Button::STYLE_BUTTON);
  bubble_button_->SetStyle(views::Button::STYLE_BUTTON);
  lock_button_->SetStyle(views::Button::STYLE_BUTTON);
  widgets_button_->SetStyle(views::Button::STYLE_BUTTON);
  system_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
  window_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
  child_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
  transient_button_->SetStyle(views::Button::STYLE_BUTTON);
  examples_button_->SetStyle(views::Button::STYLE_BUTTON);
  show_hide_window_button_->SetStyle(views::Button::STYLE_BUTTON);
  show_screensaver_->SetStyle(views::Button::STYLE_BUTTON);
  show_web_notification_->SetStyle(views::Button::STYLE_BUTTON);

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::CENTER,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  AddViewToLayout(layout, create_button_);
  AddViewToLayout(layout, panel_button_);
  AddViewToLayout(layout, create_nonresizable_button_);
  AddViewToLayout(layout, bubble_button_);
  AddViewToLayout(layout, lock_button_);
  AddViewToLayout(layout, widgets_button_);
  AddViewToLayout(layout, system_modal_button_);
  AddViewToLayout(layout, window_modal_button_);
  AddViewToLayout(layout, child_modal_button_);
  AddViewToLayout(layout, transient_button_);
  AddViewToLayout(layout, examples_button_);
  AddViewToLayout(layout, show_hide_window_button_);
  AddViewToLayout(layout, show_screensaver_);
  AddViewToLayout(layout, show_web_notification_);
  set_context_menu_controller(this);
}

WindowTypeLauncher::~WindowTypeLauncher() {
}

void WindowTypeLauncher::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
}

bool WindowTypeLauncher::OnMousePressed(const ui::MouseEvent& event) {
  // Overridden so we get OnMouseReleased and can show the context menu.
  return true;
}

views::View* WindowTypeLauncher::GetContentsView() {
  return this;
}

bool WindowTypeLauncher::CanResize() const {
  return true;
}

base::string16 WindowTypeLauncher::GetWindowTitle() const {
  return base::ASCIIToUTF16("Examples: Window Builder");
}

bool WindowTypeLauncher::CanMaximize() const {
  return true;
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == create_button_) {
    ToplevelWindow::CreateParams params;
    params.can_resize = true;
    params.can_maximize = true;
    ToplevelWindow::CreateToplevelWindow(params);
  } else if (sender == panel_button_) {
    PanelWindow::CreatePanelWindow(gfx::Rect());
  } else if (sender == create_nonresizable_button_) {
    ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
  } else if (sender == bubble_button_) {
    CreatePointyBubble(sender);
  } else if (sender == lock_button_) {
    Shell::GetInstance()->session_state_delegate()->LockScreen();
  } else if (sender == widgets_button_) {
    CreateWidgetsWindow();
  } else if (sender == system_modal_button_) {
    ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                 ui::MODAL_TYPE_SYSTEM);
  } else if (sender == window_modal_button_) {
    ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                 ui::MODAL_TYPE_WINDOW);
  } else if (sender == child_modal_button_) {
    ash::test::CreateChildModalParent(
        GetWidget()->GetNativeView()->GetRootWindow());
  } else if (sender == transient_button_) {
    NonModalTransient::OpenNonModalTransient(GetWidget()->GetNativeView());
  } else if (sender == show_hide_window_button_) {
    NonModalTransient::ToggleNonModalTransient(GetWidget()->GetNativeView());
  } else if (sender == show_screensaver_) {
    ash::ShowScreensaver(GURL("http://www.google.com"));
    content::BrowserThread::PostDelayedTask(content::BrowserThread::UI,
                                            FROM_HERE,
                                            base::Bind(&ash::CloseScreensaver),
                                            base::TimeDelta::FromSeconds(5));

  } else if (sender == show_web_notification_) {
    scoped_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        "id0",
        base::ASCIIToUTF16("Test Shell Web Notification"),
        base::ASCIIToUTF16("Notification message body."),
        gfx::Image(),
        base::ASCIIToUTF16("www.testshell.org"),
        message_center::NotifierId(
            message_center::NotifierId::APPLICATION, "test-id"),
        message_center::RichNotificationData(),
        NULL /* delegate */));

    ash::Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget()
        ->web_notification_tray()->message_center()
        ->AddNotification(notification.Pass());
  } else if (sender == examples_button_) {
    views::examples::ShowExamplesWindowWithContent(
        views::examples::DO_NOTHING_ON_CLOSE,
        Shell::GetInstance()->delegate()->GetActiveBrowserContext(),
        NULL);
  }
}

void WindowTypeLauncher::ExecuteCommand(int id, int event_flags) {
  switch (id) {
    case COMMAND_NEW_WINDOW:
      InitWindowTypeLauncher();
      break;
    case COMMAND_TOGGLE_FULLSCREEN:
      GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
      break;
    default:
      break;
  }
}

void WindowTypeLauncher::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  MenuItemView* root = new MenuItemView(this);
  root->AppendMenuItem(COMMAND_NEW_WINDOW,
                       base::ASCIIToUTF16("New Window"),
                       MenuItemView::NORMAL);
  root->AppendMenuItem(COMMAND_TOGGLE_FULLSCREEN,
                       base::ASCIIToUTF16("Toggle FullScreen"),
                       MenuItemView::NORMAL);
  // MenuRunner takes ownership of root.
  menu_runner_.reset(new MenuRunner(
      root, MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  if (menu_runner_->RunMenuAt(GetWidget(),
                              NULL,
                              gfx::Rect(point, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) == MenuRunner::MENU_DELETED) {
    return;
  }
}

}  // namespace shell
}  // namespace ash
