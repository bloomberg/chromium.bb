// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/quick_launch/quick_launch_application.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
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
  QuickLaunchUI(mojo::Connector* connector, catalog::mojom::CatalogPtr catalog)
      : connector_(connector),
        prompt_(new views::Textfield),
        catalog_(std::move(catalog)) {
    set_background(views::Background::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    AddChildView(prompt_);

    UpdateEntries();
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
    suggestion_rejected_ = false;
    switch (key_event.key_code()) {
      case ui::VKEY_RETURN: {
        std::string url = Canonicalize(prompt_->text());
        connections_.push_back(connector_->Connect(url));
        prompt_->SetText(base::string16());
        UpdateEntries();
      } break;
      case ui::VKEY_BACK:
      case ui::VKEY_DELETE:
        // The user didn't like our suggestion, don't make another until they
        // type another character.
        suggestion_rejected_ = true;
        break;
      default:
        break;
    }
    return false;
  }
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    // Don't keep making a suggestion if the user didn't like what we offered.
    if (suggestion_rejected_)
      return;

    // TODO(beng): it'd be nice if we persisted some history/scoring here.
    for (const auto& name : app_names_) {
      if (base::StartsWith(name, new_contents,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        base::string16 suffix = name;
        base::ReplaceSubstringsAfterOffset(&suffix, 0, new_contents,
                                           base::string16());
        gfx::Range range(new_contents.size(), name.size());
        prompt_->SetText(name);
        prompt_->SelectRange(range);
        break;
      }
    }
  }

  std::string Canonicalize(const base::string16& input) const {
    base::string16 working;
    base::TrimWhitespace(input, base::TRIM_ALL, &working);
    GURL url(working);
    if (url.scheme() != "mojo" && url.scheme() != "exe")
      working = base::ASCIIToUTF16("mojo:") + working;
    return base::UTF16ToUTF8(working);
  }

  void UpdateEntries() {
    catalog_->GetEntries(nullptr,
                         base::Bind(&QuickLaunchUI::OnGotCatalogEntries,
                                    base::Unretained(this)));
  }

  void OnGotCatalogEntries(
      mojo::Map<mojo::String, catalog::mojom::CatalogEntryPtr> entries) {
    for (const auto& entry : entries)
      app_names_.insert(base::UTF8ToUTF16(entry.first.get()));
  }

  mojo::Connector* connector_;
  views::Textfield* prompt_;
  std::vector<std::unique_ptr<mojo::Connection>> connections_;
  catalog::mojom::CatalogPtr catalog_;
  std::set<base::string16> app_names_;
  bool suggestion_rejected_ = false;

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

  catalog::mojom::CatalogPtr catalog;
  connector->ConnectToInterface("mojo:catalog", &catalog);

  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      new QuickLaunchUI(connector, std::move(catalog)), nullptr,
      gfx::Rect(10, 640, 0, 0));
  window->Show();
}

bool QuickLaunchApplication::AcceptConnection(mojo::Connection* connection) {
  return true;
}

}  // namespace quick_launch
}  // namespace mash
