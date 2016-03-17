// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/quick_launch/quick_launch_application.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/connector.h"
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
  QuickLaunchUI(mojo::Connector* connector)
      : connector_(connector), prompt_(new views::Textfield) {
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
      connections_.push_back(connector_->Connect(url));
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

  mojo::Connector* connector_;
  views::Textfield* prompt_;
  std::vector<scoped_ptr<mojo::Connection>> connections_;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchUI);
};

QuickLaunchApplication::QuickLaunchApplication() {}
QuickLaunchApplication::~QuickLaunchApplication() {}

void QuickLaunchApplication::Initialize(mojo::Connector* connector,
                                        const mojo::Identity& identity,
                                        uint32_t id) {
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  views::WindowManagerConnection::Create(connector);

  views::Widget* window = views::Widget::CreateWindowWithBounds(
      new QuickLaunchUI(connector), gfx::Rect(10, 640, 0, 0));
  window->Show();
}

bool QuickLaunchApplication::AcceptConnection(mojo::Connection* connection) {
  return true;
}

void QuickLaunchApplication::ShellConnectionLost() {
  base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace quick_launch
}  // namespace mash
