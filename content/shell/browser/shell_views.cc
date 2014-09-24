// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/shell/browser/shell_platform_data_aura.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/wm/test/wm_test_helper.h"
#else  // !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace content {

namespace {
// ViewDelegate implementation for aura content shell
class ShellViewsDelegateAura : public views::DesktopTestViewsDelegate {
 public:
  ShellViewsDelegateAura() : use_transparent_windows_(false) {
  }

  virtual ~ShellViewsDelegateAura() {
  }

  void SetUseTransparentWindows(bool transparent) {
    use_transparent_windows_ = transparent;
  }

 private:
  bool use_transparent_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegateAura);
};

// Model for the "Debug" menu
class ContextMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  explicit ContextMenuModel(
      Shell* shell, const content::ContextMenuParams& params)
    : ui::SimpleMenuModel(this),
      shell_(shell),
      params_(params) {
    AddItem(COMMAND_OPEN_DEVTOOLS, base::ASCIIToUTF16("Inspect Element"));
  }

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE { return false; }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    switch (command_id) {
      case COMMAND_OPEN_DEVTOOLS:
        shell_->ShowDevToolsForElementAt(params_.x, params_.y);
        break;
    };
  }

 private:
  enum CommandID {
    COMMAND_OPEN_DEVTOOLS
  };

  Shell* shell_;
  content::ContextMenuParams params_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuModel);
};

// Maintain the UI controls and web view for content shell
class ShellWindowDelegateView : public views::WidgetDelegateView,
                                public views::TextfieldController,
                                public views::ButtonListener {
 public:
  enum UIControl {
    BACK_BUTTON,
    FORWARD_BUTTON,
    STOP_BUTTON
  };

  ShellWindowDelegateView(Shell* shell)
    : shell_(shell),
      toolbar_view_(new View),
      contents_view_(new View) {
  }
  virtual ~ShellWindowDelegateView() {}

  // Update the state of UI controls
  void SetAddressBarURL(const GURL& url) {
    url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
  }
  void SetWebContents(WebContents* web_contents, const gfx::Size& size) {
    contents_view_->SetLayoutManager(new views::FillLayout());
    web_view_ = new views::WebView(web_contents->GetBrowserContext());
    web_view_->SetWebContents(web_contents);
    web_view_->SetPreferredSize(size);
    web_contents->Focus();
    contents_view_->AddChildView(web_view_);
    Layout();

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);

    // Resizing a widget on chromeos doesn't automatically resize the root, need
    // to explicitly do that.
#if defined(OS_CHROMEOS)
    GetWidget()->GetNativeWindow()->GetHost()->SetBounds(bounds);
#endif
  }

  void SetWindowTitle(const base::string16& title) { title_ = title; }
  void EnableUIControl(UIControl control, bool is_enabled) {
    if (control == BACK_BUTTON) {
      back_button_->SetState(is_enabled ? views::CustomButton::STATE_NORMAL
          : views::CustomButton::STATE_DISABLED);
    } else if (control == FORWARD_BUTTON) {
      forward_button_->SetState(is_enabled ? views::CustomButton::STATE_NORMAL
          : views::CustomButton::STATE_DISABLED);
    } else if (control == STOP_BUTTON) {
      stop_button_->SetState(is_enabled ? views::CustomButton::STATE_NORMAL
          : views::CustomButton::STATE_DISABLED);
    }
  }

  void ShowWebViewContextMenu(const content::ContextMenuParams& params) {
    gfx::Point screen_point(params.x, params.y);

    // Convert from content coordinates to window coordinates.
    // This code copied from chrome_web_contents_view_delegate_views.cc
    aura::Window* web_contents_window =
        shell_->web_contents()->GetNativeView();
    aura::Window* root_window = web_contents_window->GetRootWindow();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client) {
        screen_position_client->ConvertPointToScreen(web_contents_window,
                &screen_point);
    }

    context_menu_model_.reset(new ContextMenuModel(shell_, params));
    context_menu_runner_.reset(new views::MenuRunner(
        context_menu_model_.get(), views::MenuRunner::CONTEXT_MENU));

    if (context_menu_runner_->RunMenuAt(web_view_->GetWidget(),
                                        NULL,
                                        gfx::Rect(screen_point, gfx::Size()),
                                        views::MENU_ANCHOR_TOPRIGHT,
                                        ui::MENU_SOURCE_NONE) ==
        views::MenuRunner::MENU_DELETED) {
      return;
    }
  }

  void OnWebContentsFocused(content::WebContents* web_contents) {
    if (web_view_->GetWebContents() == web_contents)
      web_view_->OnWebContentsFocused(web_contents);
  }

 private:
  // Initialize the UI control contained in shell window
  void InitShellWindow() {
    set_background(views::Background::CreateStandardPanelBackground());

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddPaddingColumn(0, 2);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, 2);

    layout->AddPaddingRow(0, 2);

    // Add toolbar buttons and URL text field
    {
      layout->StartRow(0, 0);
      views::GridLayout* toolbar_layout = new views::GridLayout(toolbar_view_);
      toolbar_view_->SetLayoutManager(toolbar_layout);

      views::ColumnSet* toolbar_column_set =
          toolbar_layout->AddColumnSet(0);
      // Back button
      back_button_ = new views::LabelButton(this, base::ASCIIToUTF16("Back"));
      back_button_->SetStyle(views::Button::STYLE_BUTTON);
      gfx::Size back_button_size = back_button_->GetPreferredSize();
      toolbar_column_set->AddColumn(views::GridLayout::CENTER,
                                    views::GridLayout::CENTER, 0,
                                    views::GridLayout::FIXED,
                                    back_button_size.width(),
                                    back_button_size.width() / 2);
      // Forward button
      forward_button_ =
          new views::LabelButton(this, base::ASCIIToUTF16("Forward"));
      forward_button_->SetStyle(views::Button::STYLE_BUTTON);
      gfx::Size forward_button_size = forward_button_->GetPreferredSize();
      toolbar_column_set->AddColumn(views::GridLayout::CENTER,
                                    views::GridLayout::CENTER, 0,
                                    views::GridLayout::FIXED,
                                    forward_button_size.width(),
                                    forward_button_size.width() / 2);
      // Refresh button
      refresh_button_ =
          new views::LabelButton(this, base::ASCIIToUTF16("Refresh"));
      refresh_button_->SetStyle(views::Button::STYLE_BUTTON);
      gfx::Size refresh_button_size = refresh_button_->GetPreferredSize();
      toolbar_column_set->AddColumn(views::GridLayout::CENTER,
                                    views::GridLayout::CENTER, 0,
                                    views::GridLayout::FIXED,
                                    refresh_button_size.width(),
                                    refresh_button_size.width() / 2);
      // Stop button
      stop_button_ = new views::LabelButton(this, base::ASCIIToUTF16("Stop"));
      stop_button_->SetStyle(views::Button::STYLE_BUTTON);
      gfx::Size stop_button_size = stop_button_->GetPreferredSize();
      toolbar_column_set->AddColumn(views::GridLayout::CENTER,
                                    views::GridLayout::CENTER, 0,
                                    views::GridLayout::FIXED,
                                    stop_button_size.width(),
                                    stop_button_size.width() / 2);
      toolbar_column_set->AddPaddingColumn(0, 2);
      // URL entry
      url_entry_ = new views::Textfield();
      url_entry_->set_controller(this);
      toolbar_column_set->AddColumn(views::GridLayout::FILL,
                                    views::GridLayout::FILL, 1,
                                    views::GridLayout::USE_PREF, 0, 0);
      toolbar_column_set->AddPaddingColumn(0, 2);

      // Fill up the first row
      toolbar_layout->StartRow(0, 0);
      toolbar_layout->AddView(back_button_);
      toolbar_layout->AddView(forward_button_);
      toolbar_layout->AddView(refresh_button_);
      toolbar_layout->AddView(stop_button_);
      toolbar_layout->AddView(url_entry_);

      layout->AddView(toolbar_view_);
    }

    layout->AddPaddingRow(0, 5);

    // Add web contents view as the second row
    {
      layout->StartRow(1, 0);
      layout->AddView(contents_view_);
    }

    layout->AddPaddingRow(0, 5);

    InitAccelerators();
  }
  void InitAccelerators() {
    static const ui::KeyboardCode keys[] = { ui::VKEY_F5,
                                             ui::VKEY_BROWSER_BACK,
                                             ui::VKEY_BROWSER_FORWARD };
    for (size_t i = 0; i < arraysize(keys); ++i) {
      GetFocusManager()->RegisterAccelerator(
        ui::Accelerator(keys[i], ui::EF_NONE),
        ui::AcceleratorManager::kNormalPriority,
        this);
    }
  }
  // Overridden from TextfieldController
  virtual void ContentsChanged(views::Textfield* sender,
                               const base::string16& new_contents) OVERRIDE {
  }
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
   if (sender == url_entry_ && key_event.key_code() == ui::VKEY_RETURN) {
     std::string text = base::UTF16ToUTF8(url_entry_->text());
     GURL url(text);
     if (!url.has_scheme()) {
       url = GURL(std::string("http://") + std::string(text));
       url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
     }
     shell_->LoadURL(url);
     return true;
   }
   return false;
  }

  // Overridden from ButtonListener
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == back_button_)
      shell_->GoBackOrForward(-1);
    else if (sender == forward_button_)
      shell_->GoBackOrForward(1);
    else if (sender == refresh_button_)
      shell_->Reload();
    else if (sender == stop_button_)
      shell_->Stop();
  }

  // Overridden from WidgetDelegateView
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual bool CanMaximize() const OVERRIDE { return true; }
  virtual bool CanMinimize() const OVERRIDE { return true; }
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return title_;
  }
  virtual void WindowClosing() OVERRIDE {
    if (shell_) {
      delete shell_;
      shell_ = NULL;
    }
  }
  virtual View* GetContentsView() OVERRIDE { return this; }

  // Overridden from View
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    // We want to be able to make the window smaller than its initial
    // (preferred) size.
    return gfx::Size();
  }
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    if (details.is_add && details.child == this) {
      InitShellWindow();
    }
  }

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE {
    switch (accelerator.key_code()) {
    case ui::VKEY_F5:
      shell_->Reload();
      return true;
    case ui::VKEY_BROWSER_BACK:
      shell_->GoBackOrForward(-1);
      return true;
    case ui::VKEY_BROWSER_FORWARD:
      shell_->GoBackOrForward(1);
      return true;
    default:
      return views::WidgetDelegateView::AcceleratorPressed(accelerator);
    }
  }

 private:
  // Hold a reference of Shell for deleting it when the window is closing
  Shell* shell_;

  // Window title
  base::string16 title_;

  // Toolbar view contains forward/backward/reload button and URL entry
  View* toolbar_view_;
  views::LabelButton* back_button_;
  views::LabelButton* forward_button_;
  views::LabelButton* refresh_button_;
  views::LabelButton* stop_button_;
  views::Textfield* url_entry_;
  scoped_ptr<ContextMenuModel> context_menu_model_;
  scoped_ptr<views::MenuRunner> context_menu_runner_;

  // Contents view contains the web contents view
  View* contents_view_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowDelegateView);
};

}  // namespace

#if defined(OS_CHROMEOS)
wm::WMTestHelper* Shell::wm_test_helper_ = NULL;
gfx::Screen* Shell::test_screen_ = NULL;
#endif
views::ViewsDelegate* Shell::views_delegate_ = NULL;

// static
void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
#if defined(OS_WIN)
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Initialize();
  test_screen_ = aura::TestScreen::Create(gfx::Size());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_);
  wm_test_helper_ = new wm::WMTestHelper(default_window_size,
                                         GetContextFactory());
#else
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, views::CreateDesktopScreen());
#endif
  views_delegate_ = new ShellViewsDelegateAura();
}

void Shell::PlatformExit() {
#if defined(OS_CHROMEOS)
  delete wm_test_helper_;
  wm_test_helper_ = NULL;

  delete test_screen_;
  test_screen_ = NULL;
#endif
  delete views_delegate_;
  views_delegate_ = NULL;
  delete platform_;
  platform_ = NULL;
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Shutdown();
#endif
  aura::Env::DeleteInstance();
}

void Shell::PlatformCleanUp() {
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  if (headless_)
    return;
  ShellWindowDelegateView* delegate_view =
    static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  if (control == BACK_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::BACK_BUTTON,
        is_enabled);
  } else if (control == FORWARD_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::FORWARD_BUTTON,
        is_enabled);
  } else if (control == STOP_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::STOP_BUTTON,
        is_enabled);
  }
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  if (headless_)
    return;
  ShellWindowDelegateView* delegate_view =
    static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->SetAddressBarURL(url);
}

void Shell::PlatformSetIsLoading(bool loading) {
}

void Shell::PlatformCreateWindow(int width, int height) {
  if (headless_) {
    content_size_ = gfx::Size(width, height);
    if (!platform_)
      platform_ = new ShellPlatformDataAura(content_size_);
    else
      platform_->ResizeWindow(content_size_);
    return;
  }
#if defined(OS_CHROMEOS)
  window_widget_ = views::Widget::CreateWindowWithContextAndBounds(
      new ShellWindowDelegateView(this),
      wm_test_helper_->GetDefaultParent(NULL, NULL, gfx::Rect()),
      gfx::Rect(0, 0, width, height));
#else
  window_widget_ = new views::Widget;
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(0, 0, width, height);
  params.delegate = new ShellWindowDelegateView(this);
  window_widget_->Init(params);
#endif

  content_size_ = gfx::Size(width, height);

  window_ = window_widget_->GetNativeWindow();
  // Call ShowRootWindow on RootWindow created by WMTestHelper without
  // which XWindow owned by RootWindow doesn't get mapped.
  window_->GetHost()->Show();
  window_widget_->Show();
}

void Shell::PlatformSetContents() {
  if (headless_) {
    CHECK(platform_);
    aura::Window* content = web_contents_->GetNativeView();
    aura::Window* parent = platform_->host()->window();
    if (!parent->Contains(content)) {
      parent->AddChild(content);
      content->Show();
    }
    content->SetBounds(gfx::Rect(content_size_));
    RenderWidgetHostView* host_view = web_contents_->GetRenderWidgetHostView();
    if (host_view)
      host_view->SetSize(content_size_);
  } else {
    views::WidgetDelegate* widget_delegate = window_widget_->widget_delegate();
    ShellWindowDelegateView* delegate_view =
        static_cast<ShellWindowDelegateView*>(widget_delegate);
    delegate_view->SetWebContents(web_contents_.get(), content_size_);
  }
}

void Shell::PlatformResizeSubViews() {
}

void Shell::Close() {
  if (headless_)
    delete this;
  else
    window_widget_->CloseNow();
}

void Shell::PlatformSetTitle(const base::string16& title) {
  if (headless_)
    return;
  ShellWindowDelegateView* delegate_view =
    static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->SetWindowTitle(title);
  window_widget_->UpdateWindowTitle();
}

bool Shell::PlatformHandleContextMenu(
    const content::ContextMenuParams& params) {
  if (headless_)
    return true;
  ShellWindowDelegateView* delegate_view =
    static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->ShowWebViewContextMenu(params);
  return true;
}

void Shell::PlatformWebContentsFocused(WebContents* contents) {
  if (headless_)
    return;
  ShellWindowDelegateView* delegate_view =
    static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->OnWebContentsFocused(contents);
}

}  // namespace content
