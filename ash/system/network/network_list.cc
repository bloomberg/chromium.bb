// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list.h"

#include <memory>
#include <utility>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/networking_config_delegate.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"

#include "ui/views/view.h"

using chromeos::LoginState;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::ManagedNetworkConfigurationHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

const int64_t kBluetoothTimeoutDelaySeconds = 2;

bool IsProhibitedByPolicy(const chromeos::NetworkState* network) {
  if (!NetworkTypePattern::WiFi().MatchesType(network->type()))
    return false;
  if (!LoginState::IsInitialized() || !LoginState::Get()->IsUserLoggedIn())
    return false;
  ManagedNetworkConfigurationHandler* managed_configuration_handler =
      NetworkHandler::Get()->managed_network_configuration_handler();
  const base::DictionaryValue* global_network_config =
      managed_configuration_handler->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  bool policy_prohibites_unmanaged = false;
  if (global_network_config) {
    global_network_config->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
        &policy_prohibites_unmanaged);
  }
  if (!policy_prohibites_unmanaged)
    return false;
  return !managed_configuration_handler->FindPolicyByGuidAndProfile(
      network->guid(), network->profile_path(), nullptr /* onc_source */);
}

}  // namespace

// A header row for sections in network detailed view which contains a title and
// a toggle button to turn on/off the section. Subclasses are given the
// opportunity to add extra buttons before the toggle button is added.
class NetworkListView::SectionHeaderRowView : public views::View,
                                              public views::ButtonListener {
 public:
  explicit SectionHeaderRowView(int title_id)
      : title_id_(title_id),
        container_(nullptr),
        toggle_(nullptr),
        style_(
            new TrayPopupItemStyle(TrayPopupItemStyle::FontStyle::SUB_HEADER)) {
  }

  ~SectionHeaderRowView() override {}

  void Init(bool enabled) {
    InitializeLayout();
    AddExtraButtons(enabled);
    AddToggleButton(enabled);
  }

  virtual void SetIsOn(bool enabled) {
    toggle_->set_accepts_events(true);
    toggle_->SetIsOn(enabled, true);
  }

 protected:
  // This is called before the toggle button is added to give subclasses an
  // opportunity to add more buttons before the toggle button. Subclasses can
  // add buttons to container() using AddChildView().
  virtual void AddExtraButtons(bool enabled) {}

  // Called when |toggle_| is clicked and toggled. Subclasses can override to
  // enabled/disable their respective technology, for example.
  virtual void OnToggleToggled(bool is_on) = 0;

  TriView* container() const { return container_; }
  TrayPopupItemStyle* style() const { return style_.get(); }

  int GetHeightForWidth(int w) const override {
    // Make row height fixed avoiding layout manager adjustments.
    return GetPreferredSize().height();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(toggle_, sender);
    // In the event of frequent clicks, helps to prevent a toggle button state
    // from becoming inconsistent with the async operation of enabling /
    // disabling of mobile radio. The toggle will get unlocked in the next
    // call to NetworkListView::Update(). Note that we don't disable/enable
    // because that would clear focus.
    toggle_->set_accepts_events(false);
    OnToggleToggled(toggle_->is_on());
  }

 private:
  void InitializeLayout() {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);
    container_ = TrayPopupUtils::CreateSubHeaderRowView(false);
    AddChildView(container_);

    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    style()->SetupLabel(label);
    label->SetText(l10n_util::GetStringUTF16(title_id_));
    container_->AddView(TriView::Container::CENTER, label);
  }

  void AddToggleButton(bool enabled) {
    toggle_ = TrayPopupUtils::CreateToggleButton(this, title_id_);
    toggle_->SetIsOn(enabled, false);
    container_->AddView(TriView::Container::END, toggle_);
  }

  // Resource ID for the string to use as the title of the section and for the
  // accessible text on the section header toggle button.
  const int title_id_;

  // View containing header row views, including title, toggle, and extra
  // buttons.
  TriView* container_;

  // ToggleButton to toggle section on or off.
  views::ToggleButton* toggle_;

  // TrayPopupItemStyle used to configure labels and buttons.
  std::unique_ptr<TrayPopupItemStyle> style_;

  DISALLOW_COPY_AND_ASSIGN(SectionHeaderRowView);
};

namespace {

// "Mobile Data" header row.
class MobileHeaderRowView : public NetworkListView::SectionHeaderRowView,
                            public chromeos::NetworkStateHandlerObserver {
 public:
  MobileHeaderRowView(NetworkStateHandler* network_state_handler)
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE),
        network_state_handler_(network_state_handler),
        weak_ptr_factory_(this) {
    network_state_handler_->AddObserver(this, FROM_HERE);
  }

  ~MobileHeaderRowView() override {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  }

  void SetIsOn(bool enabled) override {
    // If Mobile data is in the process of being enabled, keep the toggle
    // enabled. This ensures that the user does not see any UI jank in which the
    // toggle changes values during this process.
    enabled |= (status_ != Status::IDLE);
    SectionHeaderRowView::SetIsOn(enabled);
  }

  const char* GetClassName() const override { return "MobileHeaderRowView"; }

 protected:
  enum class Status {
    IDLE,
    WAITING_FOR_DEVICE_LIST_CHANGE
  };

  // NetworkListView::SectionHeaderRowView:
  void OnToggleToggled(bool is_on) override {
    // The Mobile network type contains both Cellular and Tether technologies,
    // though one or both of these may be unavailable. When Cellular technology
    // is available, the enabled value of Tether depends on the enabled value of
    // Cellular, so the toggle should only explicitly change the enabled value
    // of Cellular.
    if (network_state_handler_->IsTechnologyAvailable(
            NetworkTypePattern::Cellular())) {
      network_state_handler_->SetTechnologyEnabled(
          NetworkTypePattern::Cellular(), is_on,
          chromeos::network_handler::ErrorCallback());
      return;
    }

    // However, if Cellular technology is not available but Tether technology is
    // available, the toggle should explicitly change the enabled value of
    // Tether.
    DCHECK(network_state_handler_->IsTechnologyAvailable(
        NetworkTypePattern::Tether()));

    // If Tether is uninitialized, it is disabled because Bluetooth is off. In
    // this case, enabling the toggle should enable Bluetooth and Tether.
    if (network_state_handler_->IsTechnologyUninitialized(
            NetworkTypePattern::Tether())) {
      DCHECK(is_on);

      // If Bluetooth is in the process of being enabled, continue waiting for
      // this to occur.
      if (status_ != Status::IDLE)
        return;

      EnableBluetooth();
      return;
    }

    // Otherwise, simply set the value of the toggle.
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::Tether(), is_on,
        chromeos::network_handler::ErrorCallback());
  }

  // chromeos::NetworkStateHandlerObserver:
  void DeviceListChanged() override {
    if (network_state_handler_->IsTechnologyAvailable(
            NetworkTypePattern::Cellular())) {
      status_ = Status::IDLE;
      SetIsOn(network_state_handler_->IsTechnologyEnabled(
          NetworkTypePattern::Cellular()));
      return;
    }

    if (!network_state_handler_->IsTechnologyAvailable(
            NetworkTypePattern::Tether())) {
      // If Tether has become unavailable, it is disabled for some other reason
      // (e.g., the device could be in the process of being shut down).
      status_ = Status::IDLE;
      SetIsOn(false);
      return;
    }

    if (status_ != Status::WAITING_FOR_DEVICE_LIST_CHANGE ||
        network_state_handler_->IsTechnologyUninitialized(
            NetworkTypePattern::Tether())) {
      // If the device list change was unrelated to Tether, keep waiting.
      return;
    }

    OnEnableBluetoothSuccess();
  }

 private:
  // When Tether is disabled because Bluetooth is off, then enabling Bluetooth
  // will enable Tether. If enabling Bluetooth takes longer than some timeout
  // period, it is assumed that there was an error. In that case, Tether will
  // remain uninitialized and Mobile Data will remain toggled off.
  void EnableBluetooth() {
    DCHECK(status_ == Status::IDLE);

    Shell::Get()
        ->bluetooth_power_controller()
        ->SetPrimaryUserBluetoothPowerSetting(true /* enabled */);
    status_ = Status::WAITING_FOR_DEVICE_LIST_CHANGE;
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kBluetoothTimeoutDelaySeconds),
                 base::Bind(&MobileHeaderRowView::OnEnableBluetoothTimeout,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnEnableBluetoothTimeout() {
    DCHECK(status_ == Status::WAITING_FOR_DEVICE_LIST_CHANGE);
    status_ = Status::IDLE;
    SetIsOn(false);

    LOG(ERROR) << "Error enabling Bluetooth adapter. Cannot enable Mobile "
               << "data.";
  }

  void OnEnableBluetoothSuccess() {
    DCHECK(timer_.IsRunning());
    timer_.Stop();

    status_ = Status::IDLE;
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::Tether(), true /* enabled */,
        chromeos::network_handler::ErrorCallback());
  }

  NetworkStateHandler* network_state_handler_;

  Status status_ = Status::IDLE;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<MobileHeaderRowView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MobileHeaderRowView);
};


class WifiHeaderRowView : public NetworkListView::SectionHeaderRowView {
 public:
  WifiHeaderRowView()
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
        join_(nullptr) {}

  ~WifiHeaderRowView() override {}

  void SetIsOn(bool enabled) override {
    join_->SetEnabled(enabled);
    SectionHeaderRowView::SetIsOn(enabled);
  }

  const char* GetClassName() const override { return "WifiHeaderRowView"; }

 protected:
  // SectionHeaderRowView:
  void OnToggleToggled(bool is_on) override {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), is_on,
                                  chromeos::network_handler::ErrorCallback());
  }

  void AddExtraButtons(bool enabled) override {
    const SkColor prominent_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    gfx::ImageSkia normal_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kJoinIconAlpha),
        SkColorSetA(prominent_color, kJoinBadgeAlpha));
    gfx::ImageSkia disabled_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kDisabledJoinIconAlpha),
        SkColorSetA(prominent_color, kDisabledJoinBadgeAlpha));
    join_ = new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                                 normal_image, disabled_image,
                                 IDS_ASH_STATUS_TRAY_OTHER_WIFI);
    join_->SetInkDropColor(prominent_color);
    join_->SetEnabled(enabled);
    container()->AddView(TriView::Container::END, join_);
  }

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == join_) {
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
      Shell::Get()->system_tray_controller()->ShowNetworkCreate(
          shill::kTypeWifi);
      return;
    }
    SectionHeaderRowView::ButtonPressed(sender, event);
  }

 private:
  // Full opacity for badge.
  static constexpr int kJoinBadgeAlpha = 0xFF;

  // .30 opacity for icon.
  static constexpr int kJoinIconAlpha = 0x4D;

  // .38 opacity for disabled badge.
  static constexpr int kDisabledJoinBadgeAlpha = 0x61;

  // .30 * .38 opacity for disabled icon.
  static constexpr int kDisabledJoinIconAlpha = 0x1D;

  // A button to invoke "Join Wi-Fi network" dialog.
  SystemMenuButton* join_;

  DISALLOW_COPY_AND_ASSIGN(WifiHeaderRowView);
};

}  // namespace

// NetworkListView:

NetworkListView::NetworkListView(SystemTrayItem* owner, LoginStatus login)
    : NetworkStateListDetailedView(owner, LIST_TYPE_NETWORK, login),
      needs_relayout_(false),
      no_wifi_networks_view_(nullptr),
      no_mobile_networks_view_(nullptr),
      mobile_header_view_(nullptr),
      wifi_header_view_(nullptr),
      mobile_separator_view_(nullptr),
      wifi_separator_view_(nullptr),
      connection_warning_(nullptr) {}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(scroll_content());

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  NetworkStateHandler::NetworkStateList network_list;
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkIcons();
  UpdateNetworkListInternal();
}

bool NetworkListView::IsNetworkEntry(views::View* view,
                                     std::string* guid) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *guid = found->second;
  return true;
}

void NetworkListView::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  SCOPED_NET_LOG_IF_SLOW();
  // |network_list_| contains all the info and is going to be cleared and
  // recreated. Save them to |last_network_info_map_|.
  last_network_info_map_.clear();
  for (auto& info : network_list_)
    last_network_info_map_[info->guid] = std::move(info);

  network_list_.clear();
  for (const auto* network : networks) {
    if (!NetworkTypePattern::NonVirtual().MatchesType(network->type()))
      continue;
    network_list_.push_back(base::MakeUnique<NetworkInfo>(network->guid()));
  }
}

void NetworkListView::UpdateNetworkIcons() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First, update state for all networks.
  bool animating = false;

  for (auto& info : network_list_) {
    const chromeos::NetworkState* network =
        handler->GetNetworkStateFromGuid(info->guid);
    if (!network)
      continue;
    bool prohibited_by_policy = IsProhibitedByPolicy(network);
    info->label = network_icon::GetLabelForNetwork(
        network, network_icon::ICON_TYPE_MENU_LIST);
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    info->connected = network->IsConnectedState();
    info->connecting = network->IsConnectingState();
    if (network->Matches(NetworkTypePattern::WiFi()))
      info->type = NetworkInfo::Type::WIFI;
    else if (network->Matches(NetworkTypePattern::Mobile()))
      info->type = NetworkInfo::Type::MOBILE;
    if (prohibited_by_policy) {
      info->tooltip =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED);
    }
    if (!animating && network->IsConnectingState())
      animating = true;
  }
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkListInternal() {
  SCOPED_NET_LOG_IF_SLOW();
  // Get the updated list entries.
  needs_relayout_ = false;
  network_map_.clear();
  std::unique_ptr<std::set<std::string>> new_guids = UpdateNetworkListEntries();

  // Remove old children.
  std::set<std::string> remove_guids;
  for (const auto& iter : network_guid_map_) {
    if (new_guids->find(iter.first) == new_guids->end()) {
      remove_guids.insert(iter.first);
      network_map_.erase(iter.second);
      delete iter.second;
      needs_relayout_ = true;
    }
  }

  for (const auto& remove_iter : remove_guids)
    network_guid_map_.erase(remove_iter);

  if (!needs_relayout_)
    return;

  views::View* selected_view = nullptr;
  for (const auto& iter : network_guid_map_) {
    if (iter.second->IsMouseHovered()) {
      selected_view = iter.second;
      break;
    }
  }
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
  if (selected_view)
    scroll_content()->ScrollRectToVisible(selected_view->bounds());
}

std::unique_ptr<std::set<std::string>>
NetworkListView::UpdateNetworkListEntries() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // Keep an index where the next child should be inserted.
  int index = 0;

  // Show a warning that the connection might be monitored if connected to a VPN
  // or if the default network has a proxy installed.
  const bool using_vpn =
      !!NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::VPN());
  bool using_proxy = false;
  // TODO(jamescook): Create UIProxyConfigService under mash. This will require
  // the mojo pref service to work with prefs in Local State.
  // http://crbug.com/718072
  if (Shell::GetAshConfig() != Config::MASH) {
    using_proxy = NetworkHandler::Get()
                      ->ui_proxy_config_service()
                      ->HasDefaultNetworkProxyConfigured();
  }
  if (using_vpn || using_proxy) {
    if (!connection_warning_)
      connection_warning_ = CreateConnectionWarning();
    PlaceViewAtIndex(connection_warning_, index++);
  }

  // First add high-priority networks (neither Wi-Fi nor Mobile).
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkInfo::Type::UNKNOWN, index);
  index += new_guids->size();

  if (ShouldMobileDataSectionBeShown()) {
    bool cellular_enabled =
        handler->IsTechnologyEnabled(NetworkTypePattern::Cellular());
    bool tether_enabled =
        handler->IsTechnologyEnabled(NetworkTypePattern::Tether());
    int mobile_message_id = 0;

    if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular()) &&
        !handler->IsTechnologyAvailable(NetworkTypePattern::Tether())) {
      // If Cellular is available and Tether is not, display cellular-specific
      // messages if necessary. Note that GetCellularUninitializedMsg() returns
      // 0 if no special message should be displayed.
      mobile_message_id = network_icon::GetCellularUninitializedMsg();
      if (!mobile_message_id &&
          !handler->FirstNetworkByType(NetworkTypePattern::Cellular())) {
        mobile_message_id = IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
      }
    } else {
      if (handler->IsTechnologyUninitialized(NetworkTypePattern::Tether())) {
        // If Tether is uninitialized, it is disabled due to Bluetooth being
        // off. If Cellular is available, the Mobile toggle is on, so display a
        // message to enable Bluetooth. If Cellular is unavailable, the Mobile
        // toggle is off, so display a message stating that toggling it on will
        // enable Bluetooth.
        mobile_message_id =
            cellular_enabled
                ? IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH
                : IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH;
      } else {
        if (!cellular_enabled && !tether_enabled)
          mobile_message_id = IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED;
      }

      if (!mobile_message_id &&
          !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
        mobile_message_id = IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND;
      }
    }

    index = UpdateSectionHeaderRow(
        NetworkTypePattern::Mobile(),
        cellular_enabled || tether_enabled /* enabled */, index,
        &mobile_header_view_, &mobile_separator_view_);

    UpdateInfoLabel(mobile_message_id, index, &no_mobile_networks_view_);
    if (mobile_message_id)
      ++index;

    std::unique_ptr<std::set<std::string>> new_cellular_guids =
        UpdateNetworkChildren(NetworkInfo::Type::MOBILE, index);
    index += new_cellular_guids->size();
    new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());
  }

  index = UpdateSectionHeaderRow(
      NetworkTypePattern::WiFi(),
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
      &wifi_header_view_, &wifi_separator_view_);

  // "Wifi Enabled / Disabled".
  int wifi_message_id = 0;
  if (!handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    wifi_message_id = IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
  else if (!handler->FirstNetworkByType(NetworkTypePattern::WiFi()))
    wifi_message_id = IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED;
  UpdateInfoLabel(wifi_message_id, index, &no_wifi_networks_view_);
  if (wifi_message_id)
    ++index;

  // Add Wi-Fi networks.
  std::unique_ptr<std::set<std::string>> new_wifi_guids =
      UpdateNetworkChildren(NetworkInfo::Type::WIFI, index);
  index += new_wifi_guids->size();
  new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &no_wifi_networks_view_);
  }

  return new_guids;
}

bool NetworkListView::ShouldMobileDataSectionBeShown() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // The section should always be shown if Cellular networks are available.
  if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular()))
    return true;

  // Hide the section if both Cellular and Tether are UNAVAILABLE.
  if (!handler->IsTechnologyAvailable(NetworkTypePattern::Tether()))
    return false;

  // Hide the section if Tether is PROHIBITED.
  if (handler->IsTechnologyProhibited(NetworkTypePattern::Tether()))
    return false;

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (handler->IsTechnologyUninitialized(NetworkTypePattern::Tether()) &&
      !Shell::Get()->session_controller()->IsUserPrimary()) {
    return false;
  }

  return true;
}

void NetworkListView::UpdateViewForNetwork(HoverHighlightView* view,
                                           const NetworkInfo& info) {
  view->Reset();
  view->AddIconAndLabel(info.image, info.label);
  if (info.connected)
    SetupConnectedScrollListItem(view);
  else if (info.connecting)
    SetupConnectingScrollListItem(view);
  view->SetTooltipText(info.tooltip);

  // Add an additional icon to the right of the label for networks
  // that require it (e.g. Tether, controlled by extension).
  views::View* power_icon = CreatePowerStatusView(info);
  if (power_icon) {
    view->AddRightView(power_icon);
  } else {
    views::View* controlled_icon = CreateControlledByExtensionView(info);
    if (controlled_icon)
      view->AddRightView(controlled_icon);
  }

  needs_relayout_ = true;
}

views::View* NetworkListView::CreatePowerStatusView(const NetworkInfo& info) {
  // Mobile can be Cellular or Tether.
  if (info.type != NetworkInfo::Type::MOBILE)
    return nullptr;

  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);

  // Only return a battery icon for Tether network type.
  if (!NetworkTypePattern::Tether().MatchesType(network->type()))
    return nullptr;

  views::ImageView* icon = TrayPopupUtils::CreateMoreImageView();
  PowerStatus::BatteryImageInfo icon_info;
  icon_info.charge_percent = network->battery_percentage();
  icon->SetImage(PowerStatus::GetBatteryImage(
      icon_info, kMenuIconSize, kMenuIconColorDisabled, kMenuIconColor));

  // Show the numeric battery percentage on hover.
  icon->SetTooltipText(base::FormatPercent(network->battery_percentage()));

  return icon;
}

views::View* NetworkListView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  NetworkingConfigDelegate* networking_config_delegate =
      Shell::Get()->shell_delegate()->GetNetworkingConfigDelegate();
  if (!networking_config_delegate)
    return nullptr;
  std::unique_ptr<const NetworkingConfigDelegate::ExtensionInfo>
      extension_info =
          networking_config_delegate->LookUpExtensionForNetwork(info.guid);
  if (!extension_info)
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kCaptivePortalIcon, kMenuIconColor));
  controlled_icon->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(extension_info->extension_name)));
  return controlled_icon;
}

std::unique_ptr<std::set<std::string>> NetworkListView::UpdateNetworkChildren(
    NetworkInfo::Type type,
    int index) {
  std::unique_ptr<std::set<std::string>> new_guids(new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (info->type != type)
      continue;
    UpdateNetworkChild(index++, info.get());
    new_guids->insert(info->guid);
  }
  return new_guids;
}

void NetworkListView::UpdateNetworkChild(int index, const NetworkInfo* info) {
  HoverHighlightView* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);
  if (found == network_guid_map_.end()) {
    network_view = new HoverHighlightView(this);
    UpdateViewForNetwork(network_view, *info);
  } else {
    network_view = found->second;
    if (NeedUpdateViewForNetwork(*info))
      UpdateViewForNetwork(network_view, *info);
  }
  PlaceViewAtIndex(network_view, index);
  if (info->disable)
    network_view->SetEnabled(false);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
}

void NetworkListView::PlaceViewAtIndex(views::View* view, int index) {
  if (view->parent() != scroll_content()) {
    scroll_content()->AddChildViewAt(view, index);
  } else {
    // No re-order and re-layout is necessary if |view| is already at |index|.
    if (scroll_content()->child_at(index) == view)
      return;
    scroll_content()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListView::UpdateInfoLabel(int message_id,
                                      int insertion_index,
                                      TrayInfoLabel** info_label_ptr) {
  TrayInfoLabel* info_label = *info_label_ptr;
  if (!message_id) {
    if (info_label) {
      needs_relayout_ = true;
      delete info_label;
      *info_label_ptr = nullptr;
    }
    return;
  }
  if (!info_label)
    info_label = new TrayInfoLabel(this /* delegate */, message_id);
  else
    info_label->Update(message_id);

  PlaceViewAtIndex(info_label, insertion_index);
  *info_label_ptr = info_label;
}

void NetworkListView::OnLabelClicked(int message_id) {
  if (message_id == IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH)
    Shell::Get()->system_tray_controller()->ShowBluetoothSettings();
}

bool NetworkListView::IsLabelClickable(int message_id) const {
  return message_id == IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH;
}

int NetworkListView::UpdateSectionHeaderRow(NetworkTypePattern pattern,
                                            bool enabled,
                                            int child_index,
                                            SectionHeaderRowView** view,
                                            views::Separator** separator_view) {
  if (!*view) {
    if (pattern.MatchesPattern(NetworkTypePattern::Mobile()))
      *view = new MobileHeaderRowView(
          NetworkHandler::Get()->network_state_handler());
    else if (pattern.Equals(NetworkTypePattern::WiFi()))
      *view = new WifiHeaderRowView();
    else
      NOTREACHED();
    (*view)->Init(enabled);
  }
  // Show or hide a separator above the header. The separator should only be
  // visible when the header row is not at the top of the list.
  if (child_index > 0) {
    if (!*separator_view)
      *separator_view = TrayPopupUtils::CreateListSubHeaderSeparator();
    PlaceViewAtIndex(*separator_view, child_index++);
  } else {
    if (*separator_view)
      delete *separator_view;
    *separator_view = nullptr;
  }

  (*view)->SetIsOn(enabled);
  PlaceViewAtIndex(*view, child_index++);
  return child_index;
}

void NetworkListView::NetworkIconChanged() {
  Update();
}

bool NetworkListView::NeedUpdateViewForNetwork(const NetworkInfo& info) const {
  NetworkInfoMap::const_iterator found = last_network_info_map_.find(info.guid);
  if (found == last_network_info_map_.end()) {
    // If we cannot find |info| in |last_network_info_map_|, just return true
    // since this is a new network so we have nothing to compare.
    return true;
  } else {
    return *found->second != info;
  }
}

TriView* NetworkListView::CreateConnectionWarning() {
  // Set up layout and apply sticky row property.
  TriView* connection_warning = TrayPopupUtils::CreateDefaultRowView();
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning);
  connection_warning->SetBackground(
      views::CreateSolidBackground(kHeaderBackgroundColor));

  // Set 'info' icon on left side.
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, kMenuIconColor));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view);

  // Set message label in middle of row.
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.SetupLabel(label);
  connection_warning->AddView(TriView::Container::CENTER, label);
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  return connection_warning;
}

}  // namespace tray
}  // namespace ash
