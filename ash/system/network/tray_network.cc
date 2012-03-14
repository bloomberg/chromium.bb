// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Width of the icon, and the padding on the right of the icon. These are used
// to make sure that all icons are of the same size so that they line up
// properly, including the items that don't have any icons.
const int kIconWidth = 27;
const int kIconPaddingLeft = 5;

// Height of the list of networks in the popup.
const int kNetworkListHeight = 160;

// An image view with that always has a fixed width (kIconWidth) so that
// all the items line up properly.
class FixedWidthImageView : public views::ImageView {
 public:
  FixedWidthImageView() {
    SetHorizontalAlignment(views::ImageView::CENTER);
    SetVerticalAlignment(views::ImageView::CENTER);
  }

  virtual ~FixedWidthImageView() {}

 private:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::ImageView::GetPreferredSize();
    return gfx::Size(kIconWidth, size.height());
  }

  DISALLOW_COPY_AND_ASSIGN(FixedWidthImageView);
};

class ViewClickListener {
 public:
  virtual ~ViewClickListener() {}
  virtual void ClickedOn(views::View* sender) = 0;
};

class HoverHighlightView : public views::View {
 public:
  explicit HoverHighlightView(ViewClickListener* listener)
      : listener_(listener) {
    set_notify_enter_exit_on_child(true);
  }

  virtual ~HoverHighlightView() {}

  // Convenience function for adding an icon and a label.
  void AddIconAndLabel(const SkBitmap& image, const string16& label) {
    SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));
    views::ImageView* image_view = new FixedWidthImageView;
    image_view->SetImage(image);
    AddChildView(image_view);
    AddChildView(new views::Label(label));
  }

  void AddLabel(const string16& text) {
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(text);
    label->set_border(views::Border::CreateEmptyBorder(
        5, kIconWidth + kIconPaddingLeft, 5, 0));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label);
  }

 private:
  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    if (!listener_)
      return false;
    listener_->ClickedOn(this);
    return true;
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    set_background(views::Background::CreateSolidBackground(
        SkColorSetARGB(10, 0, 0, 0)));
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    set_background(NULL);
    SchedulePaint();
  }

  ViewClickListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

// A custom scroll-view that has a specified dimension.
class FixedSizedScrollView : public views::ScrollView {
 public:
  FixedSizedScrollView() {}
  virtual ~FixedSizedScrollView() {}

  void SetContentsView(View* view) {
    SetContents(view);
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
  }

  void set_fixed_size(gfx::Size size) { fixed_size_ = size; }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return fixed_size_;
  }

  gfx::Size fixed_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedScrollView);
};

}

namespace ash {
namespace internal {

namespace tray {

enum ResourceSize {
  SMALL,
  LARGE,
};

class NetworkTrayView : public views::View {
 public:
  explicit NetworkTrayView(ResourceSize size) : resource_size_(size) {
    SetLayoutManager(new views::FillLayout());

    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    Update(Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(resource_size_ == LARGE));
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    SchedulePaint();
  }

 private:
  views::ImageView* image_view_;
  ResourceSize resource_size_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public views::View {
 public:
  explicit NetworkDefaultView(SystemTrayItem* owner) : owner_(owner) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        0, 0, 5));

    icon_ = new NetworkTrayView(LARGE);
    AddChildView(icon_);

    label_ = new views::Label();
    AddChildView(label_);

    views::ImageView* more = new views::ImageView;
    more->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_MORE).ToSkBitmap());
    AddChildView(more);

    Update(Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(true));
  }

  virtual ~NetworkDefaultView() {}

  void Update(const NetworkIconInfo& info) {
    icon_->Update(info);
    label_->SetText(info.description);
  }

 private:
  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    owner_->PopupDetailedView(0, true);
    return true;
  }

  SystemTrayItem* owner_;
  NetworkTrayView* icon_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkDetailedView : public views::View,
                            public ViewClickListener {
 public:
  explicit NetworkDetailedView(user::LoginStatus login)
      : login_(login),
        header_(NULL),
        airplane_(NULL),
        settings_(NULL),
        proxy_settings_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
    Update();
  }

  virtual ~NetworkDetailedView() {}

  void Update() {
    RemoveAllChildViews(true);

    AppendHeaderEntry();
    AppendNetworkEntries();
    AppendAirplaneModeEntry();
    AppendSettingsEntry();
  }

 private:
  void AppendHeaderEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3, 5));
    views::ImageView* back = new FixedWidthImageView;
    back->SetImage(rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToSkBitmap());
    container->AddChildView(back);
    views::Label* header = new views::Label(rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_NETWORK));
    header->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    header->SetFont(header->font().DeriveFont(4));
    container->AddChildView(header);
    AddChildView(container);
    header_ = container;
  }

  void AppendNetworkEntries() {
    std::vector<NetworkIconInfo> list;
    Shell::GetInstance()->tray_delegate()->GetAvailableNetworks(&list);
    FixedSizedScrollView* scroller = new FixedSizedScrollView;
    views::View* networks = new views::View;
    networks->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddIconAndLabel(list[i].image, list[i].name);
      networks->AddChildView(container);
      network_map_[container] = list[i].unique_id;
    }
    scroller->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        SkColorSetARGB(25, 0, 0, 0)));
    scroller->set_fixed_size(
        gfx::Size(networks->GetPreferredSize().width() +
                  scroller->GetScrollBarWidth(),
                  kNetworkListHeight));
    scroller->SetContentsView(networks);
    AddChildView(scroller);
  }

  void AppendAirplaneModeEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddIconAndLabel(
        *rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_AIRPLANE).ToSkBitmap(),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_AIRPLANE_MODE));
    AddChildView(container);
    airplane_ = container;
  }

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void AppendSettingsEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (login_ != user::LOGGED_IN_NONE) {
      // Settings, only if logged in.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
      AddChildView(container);
      settings_ = container;
    } else {
      // Allow changing proxy settings in the login screen.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS));
      AddChildView(container);
      proxy_settings_ = container;
    }
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == header_) {
      Shell::GetInstance()->tray()->ShowDefaultView();
    } else if (sender == settings_) {
      delegate->ShowNetworkSettings();
    } else if (sender == proxy_settings_) {
      delegate->ChangeProxySettings();
    } else if (sender == airplane_) {
      delegate->ToggleAirplaneMode();
    } else {
      std::map<views::View*, std::string>::iterator find;
      find = network_map_.find(sender);
      if (find != network_map_.end()) {
        std::string network_id = find->second;
        delegate->ConnectToNetwork(network_id);
      }
    }
  }

  user::LoginStatus login_;
  std::map<views::View*, std::string> network_map_;
  views::View* header_;
  views::View* airplane_;
  views::View* settings_;
  views::View* proxy_settings_;
  DISALLOW_COPY_AND_ASSIGN(NetworkDetailedView);
};

}  // namespace tray

TrayNetwork::TrayNetwork() {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  tray_.reset(new tray::NetworkTrayView(tray::SMALL));
  return tray_.get();
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::NetworkDefaultView(this));
  return default_.get();
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  detailed_.reset(new tray::NetworkDetailedView(status));
  return detailed_.get();
}

void TrayNetwork::DestroyTrayView() {
  tray_.reset();
}

void TrayNetwork::DestroyDefaultView() {
  default_.reset();
}

void TrayNetwork::DestroyDetailedView() {
  detailed_.reset();
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_.get())
    tray_->Update(info);
  if (default_.get())
    default_->Update(info);
  if (detailed_.get())
    detailed_->Update();
}

}  // namespace internal
}  // namespace ash
