// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/browser/browser.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/c/system/main.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace mash {
namespace browser {

void EnableButton(views::CustomButton* button, bool enabled) {
  button->SetState(enabled ? views::Button::STATE_NORMAL
                           : views::Button::STATE_DISABLED);
}

class ProgressBar : public views::View {
 public:
  ProgressBar() {}
  ~ProgressBar() override {}

  void SetProgress(double progress) {
    progress_ = progress;
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect stroke_rect = GetLocalBounds();
    stroke_rect.set_y(stroke_rect.bottom() - 1);
    stroke_rect.set_height(1);
    canvas->FillRect(stroke_rect, SK_ColorGRAY);
    if (progress_ != 0.f) {
      gfx::Rect progress_rect = GetLocalBounds();
      progress_rect.set_width(progress_rect.width() * progress_);
      canvas->FillRect(progress_rect, SK_ColorRED);
    }
  }

  double progress_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(ProgressBar);
};

class Throbber : public views::View {
 public:
  Throbber() : timer_(false, true), weak_factory_(this) {}
  ~Throbber() override {}

  void Start() {
    throbbing_ = true;
    start_time_ = base::TimeTicks::Now();
    SchedulePaint();
    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(30),
        base::Bind(&Throbber::SchedulePaint, weak_factory_.GetWeakPtr()));
  }

  void Stop() {
    throbbing_ = false;
    if (timer_.IsRunning())
      timer_.Stop();
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    if (!throbbing_)
      return;

    gfx::PaintThrobberSpinning(
        canvas, GetLocalBounds(),
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_ThrobberSpinningColor),
        base::TimeTicks::Now() - start_time_);
  }

  bool throbbing_ = false;
  base::TimeTicks start_time_;
  base::Timer timer_;
  base::WeakPtrFactory<Throbber> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Throbber);
};

class UI : public views::WidgetDelegateView,
           public views::ButtonListener,
           public views::TextfieldController,
           public navigation::mojom::ViewClient {
 public:
  enum class Type { WINDOW, POPUP };

  UI(Browser* browser,
     Type type,
     navigation::mojom::ViewPtr view,
     navigation::mojom::ViewClientRequest request)
      : browser_(browser),
        type_(type),
        back_button_(new views::LabelButton(this, base::ASCIIToUTF16("Back"))),
        forward_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Forward"))),
        reload_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Reload"))),
        prompt_(new views::Textfield),
        throbber_(new Throbber),
        progress_bar_(new ProgressBar),
        view_(std::move(view)),
        view_client_binding_(this, std::move(request)) {
    set_background(views::Background::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    back_button_->set_request_focus_on_press(false);
    forward_button_->set_request_focus_on_press(false);
    reload_button_->set_request_focus_on_press(false);
    AddChildView(back_button_);
    AddChildView(forward_button_);
    AddChildView(reload_button_);
    AddChildView(prompt_);
    AddChildView(throbber_);
    AddChildView(progress_bar_);
  }
  ~UI() override { browser_->RemoveWindow(GetWidget()); }

  void NavigateTo(const GURL& url) { view_->NavigateTo(url); }

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    if (current_title_.empty())
      return base::ASCIIToUTF16("Browser");
    base::string16 format = base::ASCIIToUTF16("%s - Browser");
    base::ReplaceFirstSubstringAfterOffset(&format, 0, base::ASCIIToUTF16("%s"),
                                           current_title_);
    return format;
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == back_button_) {
      view_->GoBack();
    } else if (sender == forward_button_) {
      view_->GoForward();
    } else if (sender == reload_button_) {
      if (is_loading_)
        view_->Stop();
      else
        view_->Reload(false);
    }
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect local_bounds = GetLocalBounds();
    gfx::Rect bounds = local_bounds;
    bounds.Inset(5, 5);

    gfx::Size ps = back_button_->GetPreferredSize();
    back_button_->SetBoundsRect(
        gfx::Rect(bounds.x(), bounds.y(), ps.width(), ps.height()));
    ps = forward_button_->GetPreferredSize();
    forward_button_->SetBoundsRect(gfx::Rect(back_button_->bounds().right() + 5,
                                             bounds.y(), ps.width(),
                                             ps.height()));
    ps = reload_button_->GetPreferredSize();
    reload_button_->SetBoundsRect(
        gfx::Rect(forward_button_->bounds().right() + 5, bounds.y(), ps.width(),
                  ps.height()));

    ps = prompt_->GetPreferredSize();
    int prompt_y =
        bounds.y() + (reload_button_->bounds().height() - ps.height()) / 2;
    int width =
        bounds.width() - reload_button_->bounds().right() - ps.height() - 10;
    prompt_->SetBoundsRect(gfx::Rect(reload_button_->bounds().right() + 5,
                                     prompt_y, width, ps.height()));
    throbber_->SetBoundsRect(gfx::Rect(prompt_->bounds().right() + 5,
                                       prompt_->bounds().y(), ps.height(),
                                       ps.height()));

    gfx::Rect progress_bar_rect(local_bounds.x(),
                                back_button_->bounds().bottom() + 5,
                                local_bounds.width(), 2);
    progress_bar_->SetBoundsRect(progress_bar_rect);

    if (content_area_) {
      int x = local_bounds.x();
      int y = type_ == Type::POPUP ? 0 : progress_bar_->bounds().bottom();
      gfx::Point offset(x, y);
      ConvertPointToWidget(this, &offset);
      int width = local_bounds.width();
      int height = local_bounds.height() - y;
      content_area_->SetBounds(
          gfx::Rect(offset.x(), offset.y(), width, height));
    }
  }
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && GetWidget() && !content_area_) {
      mus::Window* window = aura::GetMusWindow(GetWidget()->GetNativeWindow());
      content_area_ = window->connection()->NewWindow(nullptr);
      window->AddChild(content_area_);

      mus::mojom::WindowTreeClientPtr client;
      view_->GetWindowTreeClient(GetProxy(&client));
      content_area_->Embed(std::move(client));
    }
  }

  // Overridden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    switch (key_event.key_code()) {
      case ui::VKEY_RETURN: {
        view_->NavigateTo(GURL(prompt_->text()));
      } break;
      default:
        break;
    }
    return false;
  }

  // navigation::mojom::ViewClient:
  void LoadingStateChanged(bool is_loading) override {
    is_loading_ = is_loading;
    if (is_loading_) {
      reload_button_->SetText(base::ASCIIToUTF16("Stop"));
      throbber_->Start();
    } else {
      reload_button_->SetText(base::ASCIIToUTF16("Reload"));
      throbber_->Stop();
      progress_bar_->SetProgress(0.f);
    }
  }
  void NavigationStateChanged(const GURL& url,
                              const mojo::String& title,
                              bool can_go_back,
                              bool can_go_forward) override {
    EnableButton(back_button_, can_go_back);
    EnableButton(forward_button_, can_go_forward);
    prompt_->SetText(base::UTF8ToUTF16(url.spec()));
    current_title_ = base::UTF8ToUTF16(title.get());
    GetWidget()->UpdateWindowTitle();
  }
  void LoadProgressChanged(double progress) override {
    progress_bar_->SetProgress(progress);
  }
  void ViewCreated(navigation::mojom::ViewPtr view,
                   navigation::mojom::ViewClientRequest request,
                   bool is_popup,
                   mojo::RectPtr initial_rect,
                   bool user_gesture) override {
    views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
        new UI(browser_, is_popup ? UI::Type::POPUP : UI::Type::WINDOW,
               std::move(view), std::move(request)),
        nullptr, initial_rect.To<gfx::Rect>());
    window->Show();
    browser_->AddWindow(window);
  }
  void Close() override { GetWidget()->Close(); }

  Browser* browser_;

  Type type_;

  views::LabelButton* back_button_;
  views::LabelButton* forward_button_;
  views::LabelButton* reload_button_;
  views::Textfield* prompt_;
  Throbber* throbber_;
  ProgressBar* progress_bar_;

  mus::Window* content_area_ = nullptr;

  navigation::mojom::ViewPtr view_;
  mojo::Binding<navigation::mojom::ViewClient> view_client_binding_;

  bool is_loading_ = false;
  base::string16 current_title_;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

Browser::Browser() {}
Browser::~Browser() {}

void Browser::AddWindow(views::Widget* window) {
  windows_.push_back(window);
}

void Browser::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

void Browser::Initialize(shell::Connector* connector,
                         const shell::Identity& identity,
                         uint32_t id) {
  connector_ = connector;
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  views::WindowManagerConnection::Create(connector, identity);
}

bool Browser::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Launchable>(this);
  return true;
}

void Browser::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse =
      how == mojom::LaunchMode::REUSE || how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }

  navigation::mojom::ViewFactoryPtr view_factory;
  connector_->ConnectToInterface("exe:navigation", &view_factory);
  navigation::mojom::ViewPtr view;
  navigation::mojom::ViewClientPtr view_client;
  navigation::mojom::ViewClientRequest view_client_request =
      GetProxy(&view_client);
  view_factory->CreateView(std::move(view_client), GetProxy(&view));
  UI* ui = new UI(this, UI::Type::WINDOW, std::move(view),
                  std::move(view_client_request));
  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      ui, nullptr, gfx::Rect(10, 10, 1024, 600));
  ui->NavigateTo(GURL("http://www.google.com/"));
  window->Show();
  AddWindow(window);
}

void Browser::Create(shell::Connection* connection,
                     mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace browser
}  // namespace mash
