// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/example/window_type_launcher/window_type_launcher.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "mash/shell/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::MenuItemView;
using views::MenuRunner;

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
  ~ModalWindow() override {}

  static void OpenModalWindow(aura::Window* parent, ui::ModalType modal_type) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new ModalWindow(modal_type),
                                              parent);
    widget->GetNativeView()->SetName("ModalWindow");
    widget->Show();
  }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size GetPreferredSize() const override { return gfx::Size(200, 200); }
  void Layout() override {
    gfx::Size open_ps = open_button_->GetPreferredSize();
    gfx::Rect local_bounds = GetLocalBounds();
    open_button_->SetBounds(
        5, local_bounds.bottom() - open_ps.height() - 5,
        open_ps.width(), open_ps.height());
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Modal Window");
  }
  ui::ModalType GetModalType() const override { return modal_type_; }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
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
  ~NonModalTransient() override {}

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
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size GetPreferredSize() const override { return gfx::Size(250, 250); }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Non-Modal Transient");
  }
  void DeleteDelegate() override {
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

// The contents view/delegate of a window that shows some buttons that create
// various window types.
class WindowTypeLauncherView : public views::WidgetDelegateView,
                               public views::ButtonListener,
                               public views::MenuDelegate,
                               public views::ContextMenuController {
 public:
  explicit WindowTypeLauncherView(mojo::ApplicationImpl* app)
      : app_(app),
        create_button_(new views::LabelButton(
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
    AddViewToLayout(layout, show_web_notification_);
    set_context_menu_controller(this);
  }
  ~WindowTypeLauncherView() override {}

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  enum MenuCommands {
    COMMAND_NEW_WINDOW = 1,
    COMMAND_TOGGLE_FULLSCREEN = 3,
  };

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
  }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Overridden so we get OnMouseReleased and can show the context menu.
    return true;
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this;  }
  bool CanResize() const override { return true;  }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Examples: Window Builder");
  }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == create_button_) {
      NOTIMPLEMENTED();
    } else if (sender == panel_button_) {
      NOTIMPLEMENTED();
    }
    else if (sender == create_nonresizable_button_) {
      NOTIMPLEMENTED();
    }
    else if (sender == bubble_button_) {
      NOTIMPLEMENTED();
    }
    else if (sender == lock_button_) {
      mash::shell::mojom::ShellPtr shell;
      app_->ConnectToService("mojo:mash_shell", &shell);
      shell->LockScreen();
    }
    else if (sender == widgets_button_) {
      NOTIMPLEMENTED();
    }
    else if (sender == system_modal_button_) {
      ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                   ui::MODAL_TYPE_SYSTEM);
    } else if (sender == window_modal_button_) {
      ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                   ui::MODAL_TYPE_WINDOW);
    } else if (sender == child_modal_button_) {
    } else if (sender == transient_button_) {
      NonModalTransient::OpenNonModalTransient(GetWidget()->GetNativeView());
    } else if (sender == show_hide_window_button_) {
      NonModalTransient::ToggleNonModalTransient(GetWidget()->GetNativeView());
    }
  }

  // Overridden from views::MenuDelegate:
  void ExecuteCommand(int id, int event_flags) override {
    switch (id) {
      case COMMAND_NEW_WINDOW:
        NOTIMPLEMENTED();
        break;
      case COMMAND_TOGGLE_FULLSCREEN:
        GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
        break;
      default:
        break;
    }
  }

  // Override from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override {
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

  mojo::ApplicationImpl* app_;
  views::LabelButton* create_button_;
  views::LabelButton* panel_button_;
  views::LabelButton* create_nonresizable_button_;
  views::LabelButton* bubble_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* widgets_button_;
  views::LabelButton* system_modal_button_;
  views::LabelButton* window_modal_button_;
  views::LabelButton* child_modal_button_;
  views::LabelButton* transient_button_;
  views::LabelButton* examples_button_;
  views::LabelButton* show_hide_window_button_;
  views::LabelButton* show_web_notification_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncherView);
};

}  // namespace

WindowTypeLauncher::WindowTypeLauncher() {}
WindowTypeLauncher::~WindowTypeLauncher() {}

bool WindowTypeLauncher::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}

void WindowTypeLauncher::Initialize(mojo::ApplicationImpl* app) {
  aura_init_.reset(new views::AuraInit(app, "views_mus_resources.pak"));

  views::WindowManagerConnection::Create(app);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new WindowTypeLauncherView(app);
  widget->Init(params);
  widget->Show();
}
