// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace quick_launch {

class QuickLaunchUI : public views::WidgetDelegateView,
                      public views::TextfieldController {
 public:
  QuickLaunchUI(mojo::Shell* shell)
      : shell_(shell),
        prompt_(new views::Textfield) {
    set_background(views::Background::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    AddChildView(prompt_);
  }
  ~QuickLaunchUI() override {}

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("QuickLaunch");
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(5, 5);
    prompt_->SetBoundsRect(bounds);
  }
  gfx::Size GetPreferredSize() const override {
    gfx::Size ps = prompt_->GetPreferredSize();
    ps.Enlarge(500, 10);
    return ps;
  }

  // Overridden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      std::string url = Canonicalize(prompt_->text());
      connections_.push_back(shell_->ConnectToApplication(url));
      prompt_->SetText(base::string16());
    }
    return false;
  }

  std::string Canonicalize(const base::string16& input) const {
    base::string16 working;
    base::TrimWhitespace(input, base::TRIM_ALL, &working);
    GURL url(working);
    if (url.scheme() != "mojo" && url.scheme() != "exe")
      working = base::ASCIIToUTF16("mojo:") + working;
    return base::UTF16ToUTF8(working);
  }

  mojo::Shell* shell_;
  views::Textfield* prompt_;
  std::vector<scoped_ptr<mojo::Connection>> connections_;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchUI);
};

class QuickLaunchApplicationDelegate : public mojo::ShellClient {
 public:
  QuickLaunchApplicationDelegate() {}
  ~QuickLaunchApplicationDelegate() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override {
    tracing_.Initialize(shell, url);

    aura_init_.reset(new views::AuraInit(shell, "views_mus_resources.pak"));
    views::WindowManagerConnection::Create(shell);

    views::Widget* window = views::Widget::CreateWindowWithBounds(
        new QuickLaunchUI(shell), gfx::Rect(10, 640, 0, 0));
    window->Show();
  }

  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchApplicationDelegate);
};

}  // namespace quick_launch
}  // namespace mash

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(
      new mash::quick_launch::QuickLaunchApplicationDelegate);
  return runner.Run(shell_handle);
}
