// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace {

// WebContentsObserver that tracks the lifetime of the WebContents to avoid
// potential use after destruction.
class InitiatorWebContentsObserver
    : public content::WebContentsObserver {
  public:
   explicit InitiatorWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
   }

  private:
    DISALLOW_COPY_AND_ASSIGN(InitiatorWebContentsObserver);
};

class WebDialogWebContentsDelegateViews
    : public ui::WebDialogWebContentsDelegate {
 public:
  WebDialogWebContentsDelegateViews(content::BrowserContext* browser_context,
                                    InitiatorWebContentsObserver* observer,
                                    views::WebView* web_view)
      : ui::WebDialogWebContentsDelegate(browser_context,
                                         new ChromeWebContentsHandler()),
        initiator_observer_(observer),
        web_view_(web_view) {
  }
  ~WebDialogWebContentsDelegateViews() override {}

  // ui::WebDialogWebContentsDelegate:
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    // Forward shortcut keys in dialog to our initiator's delegate.
    // http://crbug.com/104586
    // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
    if (!initiator_observer_->web_contents())
      return;

    auto* delegate = initiator_observer_->web_contents()->GetDelegate();
    if (!delegate)
      return;
    delegate->HandleKeyboardEvent(initiator_observer_->web_contents(), event);
#endif
  }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& preferred_size) override {
    if (source != web_view_->GetWebContents())
      return;

    if (!initiator_observer_->web_contents())
      return;

    // Sets WebView's preferred size based on auto-resized contents.
    web_view_->SetPreferredSize(preferred_size);

    content::WebContents* top_level_web_contents =
        constrained_window::GetTopLevelWebContents(
            initiator_observer_->web_contents());
    if (top_level_web_contents) {
      constrained_window::UpdateWebContentsModalDialogPosition(
          web_view_->GetWidget(),
          web_modal::WebContentsModalDialogManager::FromWebContents(
              top_level_web_contents)
              ->delegate()
              ->GetWebContentsModalDialogHost());
    }
  }

 private:
  InitiatorWebContentsObserver* const initiator_observer_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogWebContentsDelegateViews);
};

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(content::BrowserContext* context,
                                    ui::WebDialogDelegate* delegate,
                                    InitiatorWebContentsObserver* observer,
                                    views::WebView* view)
      : ConstrainedWebDialogDelegateBase(context, delegate,
            new WebDialogWebContentsDelegateViews(context, observer, view)),
        view_(view) {}

  ~ConstrainedWebDialogDelegateViews() override {}

  // ui::WebDialogWebContentsDelegate:
  void CloseContents(content::WebContents* source) override {
    view_->GetWidget()->Close();
  }

  // contents::WebContentsDelegate:
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

  // ConstrainedWebDialogDelegate:
  gfx::NativeWindow GetNativeDialog() override {
    return view_->GetWidget()->GetNativeWindow();
  }

 private:
  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  views::WebView* view_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViews);
};

class ConstrainedWebDialogDelegateViewViews
    : public views::WebView,
      public ConstrainedWebDialogDelegate,
      public views::WidgetDelegate {
 public:
  ConstrainedWebDialogDelegateViewViews(
      content::BrowserContext* browser_context,
      ui::WebDialogDelegate* delegate,
      content::WebContents* web_contents,
      const gfx::Size& min_size,
      const gfx::Size& max_size)
      : views::WebView(browser_context),
        initiator_observer_(web_contents),
        impl_(new ConstrainedWebDialogDelegateViews(browser_context, delegate,
                                                    &initiator_observer_,
                                                    this)),
        min_size_(min_size),
        max_size_(max_size) {
    SetWebContents(GetWebContents());
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    if (!max_size_.IsEmpty())
      EnableAutoResize();
  }
  ~ConstrainedWebDialogDelegateViewViews() override {}

  // ConstrainedWebDialogDelegate:
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override {
    return impl_->GetWebDialogDelegate();
  }
  ui::WebDialogDelegate* GetWebDialogDelegate() override {
    return impl_->GetWebDialogDelegate();
  }
  void OnDialogCloseFromWebUI() override {
    return impl_->OnDialogCloseFromWebUI();
  }
  void ReleaseWebContentsOnDialogClose() override {
    return impl_->ReleaseWebContentsOnDialogClose();
  }
  gfx::NativeWindow GetNativeDialog() override {
    return impl_->GetNativeDialog();
  }
  content::WebContents* GetWebContents() override {
    return impl_->GetWebContents();
  }

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override { return this; }
  void WindowClosing() override {
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed(std::string());
  }
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }
  base::string16 GetWindowTitle() const override {
    return impl_->closed_via_webui() ? base::string16() :
        GetWebDialogDelegate()->GetDialogTitle();
  }
  views::View* GetContentsView() override { return this; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return views::DialogDelegate::CreateDialogFrameView(widget, gfx::Insets());
  }
  bool ShouldShowCloseButton() const override {
    // No close button if the dialog doesn't want a title bar.
    return impl_->GetWebDialogDelegate()->ShouldShowDialogTitle();
  }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  // views::WebView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    // Pressing ESC closes the dialog.
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    GetWidget()->Close();
    return true;
  }
  gfx::Size GetPreferredSize() const override {
    gfx::Size size;
    if (!impl_->closed_via_webui()) {
      // If auto-resizing is enabled and the dialog has been auto-resized,
      // GetPreferredSize() will return the appropriate current size.  In this
      // case, GetDialogSize() should leave its argument untouched.  In all
      // other cases, GetDialogSize() will overwrite the passed-in size.
      size = WebView::GetPreferredSize();
      GetWebDialogDelegate()->GetDialogSize(&size);
    }
    return size;
  }
  gfx::Size GetMinimumSize() const override {
    return min_size_;
  }
  gfx::Size GetMaximumSize() const override {
    return !max_size_.IsEmpty() ? max_size_ : WebView::GetMaximumSize();
  }
  void RenderViewCreated(content::RenderViewHost* render_view_host) override {
    if (!max_size_.IsEmpty())
      EnableAutoResize();
  }
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override {
    if (!max_size_.IsEmpty())
      EnableAutoResize();
  }
  void DocumentOnLoadCompletedInMainFrame() override {
    if (!max_size_.IsEmpty() && initiator_observer_.web_contents()) {
      content::WebContents* top_level_web_contents =
          constrained_window::GetTopLevelWebContents(
              initiator_observer_.web_contents());
      if (top_level_web_contents) {
        constrained_window::ShowModalDialog(GetWidget()->GetNativeWindow(),
                                            top_level_web_contents);
      }
    }
  }

 private:
  void EnableAutoResize() {
    content::RenderViewHost* render_view_host =
        GetWebContents()->GetRenderViewHost();
    render_view_host->EnableAutoResize(min_size_, max_size_);
  }

  InitiatorWebContentsObserver initiator_observer_;

  std::unique_ptr<ConstrainedWebDialogDelegateViews> impl_;

  // Minimum and maximum sizes to determine dialog bounds for auto-resizing.
  const gfx::Size min_size_;
  const gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewViews);
};

}  // namespace

ConstrainedWebDialogDelegate* ShowConstrainedWebDialog(
    content::BrowserContext* browser_context,
    ui::WebDialogDelegate* delegate,
    content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewViews* dialog =
      new ConstrainedWebDialogDelegateViewViews(
          browser_context, delegate, web_contents,
          gfx::Size(), gfx::Size());
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  return dialog;
}

ConstrainedWebDialogDelegate* ShowConstrainedWebDialogWithAutoResize(
    content::BrowserContext* browser_context,
    ui::WebDialogDelegate* delegate,
    content::WebContents* web_contents,
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  DCHECK(!min_size.IsEmpty());
  DCHECK(!max_size.IsEmpty());
  ConstrainedWebDialogDelegateViewViews* dialog =
      new ConstrainedWebDialogDelegateViewViews(
          browser_context, delegate, web_contents,
          min_size, max_size);

  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  content::WebContents* top_level_web_contents =
      constrained_window::GetTopLevelWebContents(web_contents);
  DCHECK(top_level_web_contents);
  constrained_window::CreateWebModalDialogViews(dialog, top_level_web_contents);
  return dialog;
}
