// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/browser/browser.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "mash/browser/debug_view.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/c/system/main.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
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

class NavMenuModel : public ui::MenuModel {
 public:
  class Delegate {
   public:
    virtual void NavigateToOffset(int offset) = 0;
  };

  struct Entry {
    Entry(const base::string16& title, int offset)
        : title(title), offset(offset) {}
    ~Entry() {}

    // Title of the entry in the menu.
    base::string16 title;
    // Offset from the currently visible page to navigate to this item.
    int offset;
  };

  NavMenuModel(const std::vector<Entry>& entries, Delegate* delegate)
      : navigation_delegate_(delegate), entries_(entries) {}
  ~NavMenuModel() override {}

 private:
  bool HasIcons() const override { return false; }
  int GetItemCount() const override {
    return static_cast<int>(entries_.size());
  }
  ui::MenuModel::ItemType GetTypeAt(int index) const override {
    return ui::MenuModel::TYPE_COMMAND;
  }
  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }
  int GetCommandIdAt(int index) const override {
    return index;
  }
  base::string16 GetLabelAt(int index) const override {
    return entries_[index].title;
  }
  base::string16 GetSublabelAt(int index) const override {
    return base::string16();
  }
  base::string16 GetMinorTextAt(int index) const override {
    return base::string16();
  }
  bool IsItemDynamicAt(int index) const override { return false; }
  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }
  bool IsItemCheckedAt(int index) const override { return false; }
  int GetGroupIdAt(int index) const override { return -1; }
  bool GetIconAt(int index, gfx::Image* icon) override { return false; }
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }
  bool IsEnabledAt(int index) const override { return true; }
  bool IsVisibleAt(int index) const override { return true; }
  ui::MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }
  void HighlightChangedTo(int index) override {}
  void ActivatedAt(int index) override {
    ActivatedAt(index, 0);
  }
  void ActivatedAt(int index, int event_flags) override {
    navigation_delegate_->NavigateToOffset(entries_[index].offset);
  }
  void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) override {
    delegate_ = delegate;
  }
  ui::MenuModelDelegate* GetMenuModelDelegate() const override {
    return delegate_;
  }

  ui::MenuModelDelegate* delegate_ = nullptr;
  Delegate* navigation_delegate_;
  std::vector<Entry> entries_;

  DISALLOW_COPY_AND_ASSIGN(NavMenuModel);
};

class NavButton : public views::LabelButton {
 public:
  enum class Type {
    BACK,
    FORWARD
  };

  class ModelProvider {
   public:
    virtual std::unique_ptr<ui::MenuModel> CreateMenuModel(Type type) = 0;
  };

  NavButton(Type type,
            ModelProvider* model_provider,
            views::ButtonListener* listener,
            const base::string16& label)
      : views::LabelButton(listener, label),
        type_(type),
        model_provider_(model_provider),
        show_menu_factory_(this) {}
  ~NavButton() override {}

 private:
  // views::LabelButton overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (IsTriggerableEvent(event) && enabled() &&
        HitTestPoint(event.location())) {
      y_pos_on_lbuttondown_ = event.y();
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&NavButton::ShowMenu, show_menu_factory_.GetWeakPtr(),
                     ui::GetMenuSourceTypeForEvent(event)),
          base::TimeDelta::FromMilliseconds(500));
    }
    return LabelButton::OnMousePressed(event);
  }
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    bool result = LabelButton::OnMouseDragged(event);
    if (show_menu_factory_.HasWeakPtrs()) {
      if (event.y() > y_pos_on_lbuttondown_ + GetHorizontalDragThreshold()) {
        show_menu_factory_.InvalidateWeakPtrs();
        ShowMenu(ui::GetMenuSourceTypeForEvent(event));
      }
    }
    return result;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (IsTriggerableEvent(event))
      show_menu_factory_.InvalidateWeakPtrs();
    LabelButton::OnMouseReleased(event);
  }

  void ShowMenu(ui::MenuSourceType source_type) {
    gfx::Rect local = GetLocalBounds();
    gfx::Point menu_position(local.origin());
    menu_position.Offset(0, local.height() - 1);
    View::ConvertPointToScreen(this, &menu_position);

    model_ = model_provider_->CreateMenuModel(type_);
    menu_model_adapter_.reset(new views::MenuModelAdapter(
        model_.get(),
        base::Bind(&NavButton::OnMenuClosed, base::Unretained(this))));
    menu_model_adapter_->set_triggerable_event_flags(triggerable_event_flags());
    menu_runner_.reset(new views::MenuRunner(
        menu_model_adapter_->CreateMenu(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::ASYNC));
    ignore_result(menu_runner_->RunMenuAt(
        GetWidget(), nullptr, gfx::Rect(menu_position, gfx::Size(0, 0)),
        views::MENU_ANCHOR_TOPLEFT, source_type));
  }

  void OnMenuClosed() {
    SetMouseHandler(nullptr);
    model_.reset();
    menu_runner_.reset();
    menu_model_adapter_.reset();
  }

  Type type_;
  ModelProvider* model_provider_;
  int y_pos_on_lbuttondown_ = 0;
  std::unique_ptr<ui::MenuModel> model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::WeakPtrFactory<NavButton> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavButton);
};

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
           public navigation::mojom::ViewClient,
           public NavButton::ModelProvider,
           public NavMenuModel::Delegate {
 public:
  enum class Type { WINDOW, POPUP };

  UI(Browser* browser,
     Type type,
     navigation::mojom::ViewPtr view,
     navigation::mojom::ViewClientRequest request)
      : browser_(browser),
        type_(type),
        back_button_(new NavButton(NavButton::Type::BACK, this, this,
                                   base::ASCIIToUTF16("Back"))),
        forward_button_(new NavButton(NavButton::Type::FORWARD, this, this,
                                      base::ASCIIToUTF16("Forward"))),
        reload_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Reload"))),
        prompt_(new views::Textfield),
        debug_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("DV"))),
        throbber_(new Throbber),
        progress_bar_(new ProgressBar),
        debug_view_(new DebugView),
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
    AddChildView(debug_button_);
    AddChildView(throbber_);
    AddChildView(progress_bar_);
    AddChildView(debug_view_);
    debug_view_->set_view(view_.get());
    view_->SetResizerSize(gfx::Size(16, 16));
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
    } else if (sender == debug_button_) {
      ToggleDebugView();
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
    int throbber_size = ps.height();
    gfx::Size debug_ps = debug_button_->GetPreferredSize();
    int prompt_y =
        bounds.y() + (reload_button_->bounds().height() - ps.height()) / 2;
    int width =
        bounds.width() - reload_button_->bounds().right() - throbber_size - 15 -
        debug_ps.width();
    prompt_->SetBoundsRect(gfx::Rect(reload_button_->bounds().right() + 5,
                                     prompt_y, width, ps.height()));

    debug_button_->SetBoundsRect(
        gfx::Rect(prompt_->bounds().right() + 5,
                  prompt_->bounds().y(), debug_ps.width(), debug_ps.height()));

    throbber_->SetBoundsRect(gfx::Rect(debug_button_->bounds().right() + 5,
                                       prompt_->bounds().y(), throbber_size,
                                       throbber_size));

    gfx::Rect progress_bar_rect(local_bounds.x(),
                                back_button_->bounds().bottom() + 5,
                                local_bounds.width(), 2);
    progress_bar_->SetBoundsRect(progress_bar_rect);

    int debug_view_height = 0;
    if (showing_debug_view_)
      debug_view_height = debug_view_->GetPreferredSize().height();
    debug_view_->SetBoundsRect(
        gfx::Rect(local_bounds.x(), local_bounds.height() - debug_view_height,
                  local_bounds.width(), debug_view_height));

    if (content_area_) {
      int x = local_bounds.x();
      int y = type_ == Type::POPUP ? 0 : progress_bar_->bounds().bottom();
      gfx::Point offset(x, y);
      ConvertPointToWidget(this, &offset);
      int width = local_bounds.width();
      int height = local_bounds.height() - y - debug_view_height;
      content_area_->SetBounds(
          gfx::Rect(offset.x(), offset.y(), width, height));
    }
  }
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && GetWidget() && !content_area_) {
      mus::Window* window = aura::GetMusWindow(GetWidget()->GetNativeWindow());
      content_area_ = window->window_tree()->NewWindow(nullptr);
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
    current_url_ = url;
    prompt_->SetText(base::UTF8ToUTF16(current_url_.spec()));
    current_title_ = base::UTF8ToUTF16(title.get());
    GetWidget()->UpdateWindowTitle();
  }
  void LoadProgressChanged(double progress) override {
    progress_bar_->SetProgress(progress);
  }
  void UpdateHoverURL(const GURL& url) override {
    if (url.is_valid())
      prompt_->SetText(base::UTF8ToUTF16(url.spec()));
    else
      prompt_->SetText(base::UTF8ToUTF16(current_url_.spec()));
  }
  void ViewCreated(navigation::mojom::ViewPtr view,
                   navigation::mojom::ViewClientRequest request,
                   bool is_popup,
                   const gfx::Rect& initial_rect,
                   bool user_gesture) override {
    views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
        new UI(browser_, is_popup ? UI::Type::POPUP : UI::Type::WINDOW,
               std::move(view), std::move(request)),
        nullptr, initial_rect);
    window->Show();
    browser_->AddWindow(window);
  }
  void Close() override { GetWidget()->Close(); }
  void NavigationPending(navigation::mojom::NavigationEntryPtr entry) override {
    pending_nav_ = std::move(entry);
  }
  void NavigationCommitted(
      navigation::mojom::NavigationCommittedDetailsPtr details,
      int current_index) override {
    switch (details->type) {
      case navigation::mojom::NavigationType::NEW_PAGE: {
        navigation_list_.push_back(std::move(pending_nav_));
        navigation_list_position_ = current_index;
        break;
      }
      case navigation::mojom::NavigationType::EXISTING_PAGE:
        navigation_list_position_ = current_index;
        break;
      default:
        break;
    }
  }
  void NavigationEntryChanged(navigation::mojom::NavigationEntryPtr entry,
                              int entry_index) override {
    navigation_list_[entry_index] = std::move(entry);
  }
  void NavigationListPruned(bool from_front, int count) override {
    DCHECK(count < static_cast<int>(navigation_list_.size()));
    if (from_front) {
      auto it = navigation_list_.begin() + count;
      navigation_list_.erase(navigation_list_.begin(), it);
    } else {
      auto it = navigation_list_.end() - count;
      navigation_list_.erase(it, navigation_list_.end());
    }
  }

  // NavButton::ModelProvider:
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      NavButton::Type type) override {
    std::vector<NavMenuModel::Entry> entries;
    if (type == NavButton::Type::BACK) {
      for (int i = navigation_list_position_ - 1, offset = -1;
           i >= 0; --i, --offset) {
        std::string title = navigation_list_[i]->title;
        entries.push_back(
            NavMenuModel::Entry(base::UTF8ToUTF16(title), offset));
      }
    } else {
      for (int i = navigation_list_position_ + 1, offset = 1;
           i < static_cast<int>(navigation_list_.size()); ++i, ++offset) {
        std::string title = navigation_list_[i]->title;
        entries.push_back(
            NavMenuModel::Entry(base::UTF8ToUTF16(title), offset));
      }
    }
    return base::WrapUnique(new NavMenuModel(entries, this));
  }

  // NavMenuModel::Delegate:
  void NavigateToOffset(int offset) override {
    view_->NavigateToOffset(offset);
  }

  void ToggleDebugView() {
    showing_debug_view_ = !showing_debug_view_;
    Layout();
  }

  Browser* browser_;

  Type type_;

  views::LabelButton* back_button_;
  views::LabelButton* forward_button_;
  views::LabelButton* reload_button_;
  views::Textfield* prompt_;
  views::LabelButton* debug_button_;
  Throbber* throbber_;
  ProgressBar* progress_bar_;

  mus::Window* content_area_ = nullptr;

  DebugView* debug_view_;

  navigation::mojom::ViewPtr view_;
  mojo::Binding<navigation::mojom::ViewClient> view_client_binding_;

  bool is_loading_ = false;
  base::string16 current_title_;
  GURL current_url_;

  navigation::mojom::NavigationEntryPtr pending_nav_;
  std::vector<navigation::mojom::NavigationEntryPtr> navigation_list_;
  int navigation_list_position_ = 0;

  bool showing_debug_view_ = false;

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
  window_manager_connection_ =
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
