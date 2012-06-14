// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources_standard.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Height of the list of networks in the popup.
const int kNetworkListHeight = 203;

// Create a label with the font size and color used in the network info bubble.
views::Label* CreateInfoBubbleLabel(const string16& text) {
  const SkColor text_color = SkColorSetARGB(127, 0, 0, 0);
  views::Label* label = new views::Label(text);
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
  return label;
}

// Create a row of labels for the network info bubble.
views::View* CreateInfoBubbleLine(const string16& text_label,
                                  const std::string& text_string) {
  views::View* view = new views::View;
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1));
  view->AddChildView(CreateInfoBubbleLabel(text_label));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(": ")));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(text_string)));
  return view;
}

// A bubble that cannot be activated.
class NonActivatableSettingsBubble : public views::BubbleDelegateView {
 public:
  NonActivatableSettingsBubble(views::View* anchor, views::View* content)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::TOP_RIGHT) {
    set_use_focusless(true);
    set_parent_window(ash::Shell::GetContainer(
        anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
        ash::internal::kShellWindowId_SettingBubbleContainer));
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  virtual ~NonActivatableSettingsBubble() {}

  virtual bool CanActivate() const OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonActivatableSettingsBubble);
};

using ash::internal::TrayNetwork;

int GetErrorIcon(TrayNetwork::ErrorType error_type) {
  switch(error_type) {
    case TrayNetwork::ERROR_CONNECT_FAILED:
      return IDR_AURA_UBER_TRAY_NETWORK_FAILED;
    case TrayNetwork::ERROR_DATA_LOW:
      return IDR_AURA_UBER_TRAY_NETWORK_DATA_LOW;
    case TrayNetwork::ERROR_DATA_NONE:
      return IDR_AURA_UBER_TRAY_NETWORK_DATA_NONE;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

namespace ash {
namespace internal {

namespace tray {

enum ColorTheme {
  LIGHT,
  DARK,
};

class NetworkErrors {
 public:
  struct Message {
    Message() : delegate(NULL) {}
    Message(NetworkTrayDelegate* in_delegate,
            const string16& in_title,
            const string16& in_message,
            const string16& in_link_text) :
        delegate(in_delegate),
        title(in_title),
        message(in_message),
        link_text(in_link_text) {}
    NetworkTrayDelegate* delegate;
    string16 title;
    string16 message;
    string16 link_text;
  };
  typedef std::map<TrayNetwork::ErrorType, Message> ErrorMap;

  ErrorMap& messages() { return messages_; }
  const ErrorMap& messages() const { return messages_; }

 private:
  ErrorMap messages_;
};

class NetworkTrayView : public TrayItemView {
 public:
  NetworkTrayView(ColorTheme size, bool tray_icon)
      : color_theme_(size), tray_icon_(tray_icon) {
    SetLayoutManager(new views::FillLayout());

    image_view_ = color_theme_ == DARK ?
        new FixedSizedImageView(0, kTrayPopupItemHeight) :
        new views::ImageView;
    AddChildView(image_view_);

    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, false);
    Update(info);
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    if (tray_icon_)
      SetVisible(info.tray_icon_visible);
    SchedulePaint();
  }

 private:
  views::ImageView* image_view_;
  ColorTheme color_theme_;
  bool tray_icon_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore {
 public:
  explicit NetworkDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    Update();
  }

  virtual ~NetworkDefaultView() {}

  void Update() {
    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, true);
    SetImage(&info.image);
    SetLabel(info.description);
    SetAccessibleName(info.description);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkDetailedView : public TrayDetailsView,
                            public views::ButtonListener,
                            public ViewClickListener {
 public:
  explicit NetworkDetailedView(user::LoginStatus login)
      : login_(login),
        airplane_(NULL),
        info_icon_(NULL),
        button_wifi_(NULL),
        button_mobile_(NULL),
        view_mobile_account_(NULL),
        setup_mobile_account_(NULL),
        other_wifi_(NULL),
        other_mobile_(NULL),
        settings_(NULL),
        proxy_settings_(NULL),
        info_bubble_(NULL) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    delegate->RequestNetworkScan();
    CreateItems();
    Update();
  }

  virtual ~NetworkDetailedView() {
    if (info_bubble_)
      info_bubble_->GetWidget()->CloseNow();
  }

  void CreateItems() {
    RemoveAllChildViews(true);

    airplane_ = NULL;
    info_icon_ = NULL;
    button_wifi_ = NULL;
    button_mobile_ = NULL;
    view_mobile_account_ = NULL;
    setup_mobile_account_ = NULL;
    other_wifi_ = NULL;
    other_mobile_ = NULL;
    settings_ = NULL;
    proxy_settings_ = NULL;

    AppendNetworkEntries();
    AppendNetworkExtra();
    AppendHeaderEntry();
    AppendHeaderButtons();

    Update();
  }

  void Update() {
    UpdateHeaderButtons();
    UpdateNetworkEntries();
    UpdateNetworkExtra();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_NETWORK, this);
  }

  void AppendHeaderButtons() {
    button_wifi_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER);
    footer()->AddButton(button_wifi_);

    button_mobile_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER);
    footer()->AddButton(button_mobile_);

    info_icon_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER);
    footer()->AddButton(info_icon_);
  }

  void UpdateHeaderButtons() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    button_wifi_->SetToggled(!delegate->GetWifiEnabled());
    button_mobile_->SetToggled(!delegate->GetMobileEnabled());
    button_mobile_->SetVisible(delegate->GetMobileAvailable());
    if (proxy_settings_)
      proxy_settings_->SetEnabled(delegate->IsNetworkConnected());
  }

  void AppendNetworkEntries() {
    CreateScrollableList();

    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT),
        gfx::Font::NORMAL);
    AddChildView(container);
    view_mobile_account_ = container;

    container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_SETUP_MOBILE),
        gfx::Font::NORMAL);
    AddChildView(container);
    setup_mobile_account_ = container;
  }

  void UpdateNetworkEntries() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    std::vector<NetworkIconInfo> list;
    delegate->GetAvailableNetworks(&list);

    network_map_.clear();
    std::set<std::string> new_service_paths;

    bool needs_relayout = false;
    views::View* highlighted_view = NULL;

    for (size_t i = 0; i < list.size(); ++i) {
      std::map<std::string, HoverHighlightView*>::const_iterator it =
          service_path_map_.find(list[i].service_path);
      HoverHighlightView* container = NULL;
      if (it == service_path_map_.end()) {
        // Create a new view.
        container = new HoverHighlightView(this);
        container->set_fixed_height(kTrayPopupItemHeight);
        container->AddIconAndLabel(list[i].image,
            list[i].description.empty() ? list[i].name : list[i].description,
            list[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
        scroll_content()->AddChildViewAt(container, i);
        container->set_border(views::Border::CreateEmptyBorder(0,
            kTrayPopupDetailsIconWidth, 0, 0));
        needs_relayout = true;
      } else {
        container = it->second;
        container->RemoveAllChildViews(true);
        container->AddIconAndLabel(list[i].image,
            list[i].description.empty() ? list[i].name : list[i].description,
            list[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
        container->Layout();

        // Reordering the view if necessary.
        views::View* child = scroll_content()->child_at(i);
        if (child != container) {
          scroll_content()->ReorderChildView(container, i);
          needs_relayout = true;
        }
      }

      if (list[i].highlight)
        highlighted_view = container;
      network_map_[container] = list[i].service_path;
      service_path_map_[list[i].service_path] = container;
      new_service_paths.insert(list[i].service_path);
    }

    std::set<std::string> remove_service_paths;
    for (std::map<std::string, HoverHighlightView*>::const_iterator it =
             service_path_map_.begin(); it != service_path_map_.end(); ++it) {
      if (new_service_paths.find(it->first) == new_service_paths.end()) {
        remove_service_paths.insert(it->first);
        scroll_content()->RemoveChildView(it->second);
        needs_relayout = true;
      }
    }

    for (std::set<std::string>::const_iterator remove_it =
             remove_service_paths.begin();
         remove_it != remove_service_paths.end(); ++remove_it) {
      service_path_map_.erase(*remove_it);
    }

    if (needs_relayout) {
      scroll_content()->SizeToPreferredSize();
      static_cast<views::View*>(scroller())->Layout();
      if (highlighted_view)
        scroll_content()->ScrollRectToVisible(highlighted_view->bounds());
    }

    view_mobile_account_->SetVisible(false);
    setup_mobile_account_->SetVisible(false);

    if (login_ == user::LOGGED_IN_NONE)
      return;

    std::string carrier_id, topup_url, setup_url;
    if (delegate->GetCellularCarrierInfo(&carrier_id,
                                         &topup_url,
                                         &setup_url)) {
      if (carrier_id != carrier_id_) {
        carrier_id_ = carrier_id;
        if (!topup_url.empty())
          topup_url_ = topup_url;
      }

      if (!setup_url.empty())
        setup_url_ = setup_url;

      if (!topup_url_.empty())
        view_mobile_account_->SetVisible(true);
      if (!setup_url_.empty())
        setup_mobile_account_->SetVisible(true);
    }
  }

  void AppendNetworkExtra() {
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    TrayPopupTextButtonContainer* bottom_row =
        new TrayPopupTextButtonContainer;

    other_wifi_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
    bottom_row->AddTextButton(other_wifi_);

    other_mobile_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
    bottom_row->AddTextButton(other_mobile_);

    CreateSettingsEntry();
    DCHECK(settings_ || proxy_settings_);
    bottom_row->AddTextButton(settings_ ? settings_ : proxy_settings_);

    AddChildView(bottom_row);
  }

  void UpdateNetworkExtra() {
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    other_wifi_->SetEnabled(delegate->GetWifiEnabled());
    other_mobile_->SetVisible(delegate->GetMobileAvailable() &&
                              delegate->GetMobileScanSupported());
    if (other_mobile_->visible())
      other_mobile_->SetEnabled(delegate->GetMobileEnabled());
  }

  void AppendAirplaneModeEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddIconAndLabel(
        *rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_AIRPLANE).ToImageSkia(),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_AIRPLANE_MODE),
        gfx::Font::NORMAL);
    AddChildView(container);
    airplane_ = container;
  }

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void CreateSettingsEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (login_ != user::LOGGED_IN_NONE) {
      // Settings, only if logged in.
      settings_ = new TrayPopupTextButton(this,
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
    } else {
      proxy_settings_ = new TrayPopupTextButton(this,
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS));
    }
  }

  views::View* CreateNetworkInfoView() {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::string ip_address, ethernet_address, wifi_address;
    Shell::GetInstance()->tray_delegate()->GetNetworkAddresses(&ip_address,
        &ethernet_address, &wifi_address);

    views::View* container = new views::View;
    container->SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    container->set_border(views::Border::CreateEmptyBorder(0, 5, 0, 5));

    // GetNetworkAddresses returns empty strings if no information is available.
    if (!ip_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_IP), ip_address));
    }
    if (!ethernet_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_ETHERNET), ethernet_address));
    }
    if (!wifi_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_WIFI), wifi_address));
    }

    // Avoid an empty bubble in the unlikely event that there is no network
    // information at all.
    if (!container->has_children()) {
      container->AddChildView(CreateInfoBubbleLabel(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NO_NETWORKS)));
    }

    return container;
  }

  void ToggleInfoBubble() {
    if (ResetInfoBubble())
      return;

    info_bubble_ = new NonActivatableSettingsBubble(
        info_icon_, CreateNetworkInfoView());
    views::BubbleDelegateView::CreateBubble(info_bubble_);
    info_bubble_->Show();
  }

  // Returns whether an existing info-bubble was closed.
  bool ResetInfoBubble() {
    if (!info_bubble_)
      return false;
    info_bubble_->GetWidget()->Close();
    info_bubble_ = NULL;
    return true;
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == info_icon_) {
      ToggleInfoBubble();
      return;
    }

    // If the info bubble was visible, close it when some other item is clicked
    // on.
    ResetInfoBubble();
    if (sender == button_wifi_)
      delegate->ToggleWifi();
    else if (sender == button_mobile_)
      delegate->ToggleMobile();
    else if (sender == settings_)
      delegate->ShowNetworkSettings();
    else if (sender == proxy_settings_)
      delegate->ChangeProxySettings();
    else if (sender == other_mobile_)
      delegate->ShowOtherCellular();
    else if (sender == other_wifi_)
      delegate->ShowOtherWifi();
    else
      NOTREACHED();
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    // If the info bubble was visible, close it when some other item is clicked
    // on.
    ResetInfoBubble();

    if (sender == footer()->content()) {
      Shell::GetInstance()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
      return;
    }

    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    if (sender == view_mobile_account_) {
      delegate->ShowCellularURL(topup_url_);
    } else if (sender == setup_mobile_account_) {
      delegate->ShowCellularURL(setup_url_);
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

  std::string carrier_id_;
  std::string topup_url_;
  std::string setup_url_;

  user::LoginStatus login_;
  std::map<views::View*, std::string> network_map_;
  std::map<std::string, HoverHighlightView*> service_path_map_;
  views::View* airplane_;
  TrayPopupHeaderButton* info_icon_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_mobile_;
  views::View* view_mobile_account_;
  views::View* setup_mobile_account_;
  TrayPopupTextButton* other_wifi_;
  TrayPopupTextButton* other_mobile_;
  TrayPopupTextButton* settings_;
  TrayPopupTextButton* proxy_settings_;

  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDetailedView);
};

class NetworkErrorView : public views::View,
                         public views::LinkListener {
 public:
  NetworkErrorView(TrayNetwork* tray,
                   TrayNetwork::ErrorType error_type,
                   const NetworkErrors::Message& error)
      : tray_(tray),
        error_type_(error_type) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    views::Label* title = new views::Label(error.title);
    title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
    AddChildView(title);

    views::Label* message = new views::Label(error.message);
    message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message->SetMultiLine(true);
    message->SizeToFit(kTrayNotificationContentsWidth);
    AddChildView(message);

    if (!error.link_text.empty()) {
      views::Link* link = new views::Link(error.link_text);
      link->set_listener(this);
      link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      link->SetMultiLine(true);
      link->SizeToFit(kTrayNotificationContentsWidth);
      AddChildView(link);
    }
  }

  virtual ~NetworkErrorView() {
  }

  // Overridden from views::LinkListener.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE {
    tray_->LinkClicked(error_type_);
  }

  TrayNetwork::ErrorType error_type() const { return error_type_; }

 private:
  TrayNetwork* tray_;
  TrayNetwork::ErrorType error_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkErrorView);
};

class NetworkNotificationView : public TrayNotificationView {
 public:
  explicit NetworkNotificationView(TrayNetwork* tray)
      : TrayNotificationView(0),
        tray_(tray) {
    CreateErrorView();
    InitView(network_error_view_);
    SetIconImage(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        GetErrorIcon(network_error_view_->error_type())));
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    tray_->ClearNetworkError(network_error_view_->error_type());
  }

  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    tray_->PopupDetailedView(0, true);
    return true;
  }

  void Update() {
    CreateErrorView();
    UpdateViewAndImage(network_error_view_,
                       *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                           GetErrorIcon(network_error_view_->error_type())));
  }

 private:
  void CreateErrorView() {
    // Display the first (highest priority) error.
    CHECK(!tray_->errors()->messages().empty());
    NetworkErrors::ErrorMap::const_iterator iter =
        tray_->errors()->messages().begin();
    network_error_view_ =
        new NetworkErrorView(tray_, iter->first, iter->second);
  }

  TrayNetwork* tray_;
  tray::NetworkErrorView* network_error_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkNotificationView);
};

}  // namespace tray

TrayNetwork::TrayNetwork()
    : tray_(NULL),
      default_(NULL),
      detailed_(NULL),
      notification_(NULL),
      errors_(new tray::NetworkErrors()) {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == NULL);
  tray_ = new tray::NetworkTrayView(tray::LIGHT, true /*tray_icon*/);
  return tray_;
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new tray::NetworkDefaultView(this);
  return default_;
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ = new tray::NetworkDetailedView(status);
  return detailed_;
}

views::View* TrayNetwork::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_ == NULL);
  if (errors_->messages().empty())
    return NULL;  // Error has already been cleared.
  notification_ = new tray::NetworkNotificationView(this);
  return notification_;
}

void TrayNetwork::DestroyTrayView() {
  tray_ = NULL;
}

void TrayNetwork::DestroyDefaultView() {
  default_ = NULL;
}

void TrayNetwork::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayNetwork::DestroyNotificationView() {
  notification_ = NULL;
}

void TrayNetwork::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_)
    tray_->Update(info);
  if (default_)
    default_->Update();
  if (detailed_)
    detailed_->Update();
}

void TrayNetwork::SetNetworkError(NetworkTrayDelegate* delegate,
                                  ErrorType error_type,
                                  const string16& title,
                                  const string16& message,
                                  const string16& link_text) {
  errors_->messages()[error_type] =
      tray::NetworkErrors::Message(delegate, title, message, link_text);
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::ClearNetworkError(ErrorType error_type) {
  errors_->messages().erase(error_type);
  if (errors_->messages().empty()) {
    if (notification_)
      HideNotificationView();
    return;
  }
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::LinkClicked(ErrorType error_type) {
  tray::NetworkErrors::ErrorMap::const_iterator iter =
      errors()->messages().find(error_type);
  if (iter != errors()->messages().end() && iter->second.delegate)
    iter->second.delegate->NotificationLinkClicked();
}

}  // namespace internal
}  // namespace ash
