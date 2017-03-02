// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/cast/tray_cast.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/cast_config_controller.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/screen_security/screen_tray_item.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/cast_config.mojom.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

const size_t kMaximumStatusStringLength = 100;

// Helper method to elide the given string to the maximum length. If a string is
// contains user-input and is displayed, we should elide it.
// TODO(jdufault): This does not properly trim unicode characters. We should
// implement this properly by using views::Label::SetElideBehavior(...). See
// crbug.com/532496.
base::string16 ElideString(const base::string16& text) {
  base::string16 elided;
  gfx::ElideString(text, kMaximumStatusStringLength, &elided);
  return elided;
}

// Returns a vectorized version of the Cast icon. The icon's interior region is
// filled in if |is_casting| is true.
gfx::ImageSkia GetCastIconForSystemMenu(bool is_casting) {
  return gfx::CreateVectorIcon(
      kSystemMenuCastIcon, is_casting ? kMenuIconColor : SK_ColorTRANSPARENT);
}

}  // namespace

namespace tray {

// This view is displayed in the system tray when the cast extension is active.
// It asks the user if they want to cast the desktop. If they click on the
// chevron, then a detail view will replace this view where the user will
// actually pick the cast receiver.
class CastSelectDefaultView : public TrayItemMore {
 public:
  explicit CastSelectDefaultView(SystemTrayItem* owner);
  ~CastSelectDefaultView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSelectDefaultView);
};

CastSelectDefaultView::CastSelectDefaultView(SystemTrayItem* owner)
    : TrayItemMore(owner) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Update the image and label.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    SetImage(GetCastIconForSystemMenu(false));
  else
    SetImage(*rb.GetImageNamed(IDR_AURA_UBER_TRAY_CAST).ToImageSkia());

  base::string16 label =
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAST_DESKTOP);
  SetLabel(label);
  SetAccessibleName(label);
}

CastSelectDefaultView::~CastSelectDefaultView() {}

// This view is displayed when the screen is actively being casted; it allows
// the user to easily stop casting. It fully replaces the
// |CastSelectDefaultView| view inside of the |CastDuplexView|.
class CastCastView : public ScreenStatusView {
 public:
  CastCastView();
  ~CastCastView() override;

  void StopCasting();

  const std::string& displayed_route_id() const { return displayed_route_->id; }

  // Updates the label for the stop view to include information about the
  // current device that is being casted.
  void UpdateLabel(const std::vector<mojom::SinkAndRoutePtr>& sinks_routes);

 private:
  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  mojom::CastRoutePtr displayed_route_;

  DISALLOW_COPY_AND_ASSIGN(CastCastView);
};

CastCastView::CastCastView()
    : ScreenStatusView(
          nullptr,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN),
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP)) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    icon()->SetImage(GetCastIconForSystemMenu(true));
  } else {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    icon()->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAST_ENABLED).ToImageSkia());
  }
}

CastCastView::~CastCastView() {}

void CastCastView::StopCasting() {
  WmShell::Get()->cast_config()->StopCasting(displayed_route_.Clone());
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_CAST_STOP_CAST);
}

void CastCastView::UpdateLabel(
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes) {
  for (auto& i : sinks_routes) {
    const mojom::CastSinkPtr& sink = i->sink;
    const mojom::CastRoutePtr& route = i->route;

    // We only want to display casts that came from this machine, since on a
    // busy network many other people could be casting.
    if (!route->id.empty() && route->is_local_source) {
      displayed_route_ = route.Clone();

      // We want to display different labels inside of the title depending on
      // what we are actually casting - either the desktop, a tab, or a fallback
      // that catches everything else (ie, an extension tab).
      switch (route->content_source) {
        case ash::mojom::ContentSource::UNKNOWN:
          label()->SetText(
              l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN));
          stop_button()->SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN_ACCESSIBILITY_STOP));
          break;
        case ash::mojom::ContentSource::TAB:
          label()->SetText(ElideString(l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_CAST_CAST_TAB,
              base::UTF8ToUTF16(route->title), base::UTF8ToUTF16(sink->name))));
          stop_button()->SetAccessibleName(
              ElideString(l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_CAST_CAST_TAB_ACCESSIBILITY_STOP,
                  base::UTF8ToUTF16(route->title),
                  base::UTF8ToUTF16(sink->name))));
          break;
        case ash::mojom::ContentSource::DESKTOP:
          label()->SetText(ElideString(
              l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP,
                                         base::UTF8ToUTF16(sink->name))));
          stop_button()->SetAccessibleName(
              ElideString(l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP_ACCESSIBILITY_STOP,
                  base::UTF8ToUTF16(sink->name))));
          break;
      }

      PreferredSizeChanged();
      Layout();
      // Only need to update labels once.
      break;
    }
  }
}

void CastCastView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  StopCasting();
}

// This view by itself does very little. It acts as a front-end for managing
// which of the two child views (|CastSelectDefaultView| and |CastCastView|)
// is active.
class CastDuplexView : public views::View {
 public:
  CastDuplexView(SystemTrayItem* owner,
                 bool enabled,
                 const std::vector<mojom::SinkAndRoutePtr>& sinks_routes);
  ~CastDuplexView() override;

  // Activate either the casting or select view.
  void ActivateCastView();
  void ActivateSelectView();

  CastSelectDefaultView* select_view() { return select_view_; }
  CastCastView* cast_view() { return cast_view_; }

 private:
  // Overridden from views::View.
  void ChildPreferredSizeChanged(views::View* child) override;
  void Layout() override;

  // Only one of |select_view_| or |cast_view_| will be displayed at any given
  // time. This will return the view is being displayed.
  views::View* ActiveChildView();

  CastSelectDefaultView* select_view_;
  CastCastView* cast_view_;

  DISALLOW_COPY_AND_ASSIGN(CastDuplexView);
};

CastDuplexView::CastDuplexView(
    SystemTrayItem* owner,
    bool enabled,
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes) {
  select_view_ = new CastSelectDefaultView(owner);
  select_view_->SetEnabled(enabled);
  cast_view_ = new CastCastView();
  cast_view_->UpdateLabel(sinks_routes);
  SetLayoutManager(new views::FillLayout());

  ActivateSelectView();
}

CastDuplexView::~CastDuplexView() {
  RemoveChildView(ActiveChildView());
  delete select_view_;
  delete cast_view_;
}

void CastDuplexView::ActivateCastView() {
  if (ActiveChildView() == cast_view_)
    return;

  RemoveChildView(select_view_);
  AddChildView(cast_view_);
  InvalidateLayout();
}

void CastDuplexView::ActivateSelectView() {
  if (ActiveChildView() == select_view_)
    return;

  RemoveChildView(cast_view_);
  AddChildView(select_view_);
  InvalidateLayout();
}

void CastDuplexView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void CastDuplexView::Layout() {
  views::View::Layout();

  select_view_->SetBoundsRect(GetContentsBounds());
  cast_view_->SetBoundsRect(GetContentsBounds());
}

views::View* CastDuplexView::ActiveChildView() {
  if (cast_view_->parent() == this)
    return cast_view_;
  if (select_view_->parent() == this)
    return select_view_;
  return nullptr;
}

// Exposes an icon in the tray. |TrayCast| manages the visiblity of this.
class CastTrayView : public TrayItemView {
 public:
  explicit CastTrayView(SystemTrayItem* tray_item);
  ~CastTrayView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastTrayView);
};

CastTrayView::CastTrayView(SystemTrayItem* tray_item)
    : TrayItemView(tray_item) {
  CreateImageView();
  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    image_view()->SetImage(
        gfx::CreateVectorIcon(kSystemTrayCastIcon, kTrayIconColor));
  } else {
    image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                               .GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE)
                               .ToImageSkia());
  }
}

CastTrayView::~CastTrayView() {}

// This view displays a list of cast receivers that can be clicked on and casted
// to. It is activated by clicking on the chevron inside of
// |CastSelectDefaultView|.
class CastDetailedView : public TrayDetailsView {
 public:
  CastDetailedView(SystemTrayItem* owner,
                   const std::vector<mojom::SinkAndRoutePtr>& sinks_and_routes);
  ~CastDetailedView() override;

  // Makes the detail view think the view associated with the given receiver_id
  // was clicked. This will start a cast.
  void SimulateViewClickedForTest(const std::string& receiver_id);

  // Updates the list of available receivers.
  void UpdateReceiverList(
      const std::vector<mojom::SinkAndRoutePtr>& sinks_routes);

 private:
  void CreateItems();

  void UpdateReceiverListFromCachedData();
  views::View* AddToReceiverList(const mojom::SinkAndRoutePtr& sink_route);

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override;

  // A mapping from the receiver id to the receiver/activity data.
  std::map<std::string, ash::mojom::SinkAndRoutePtr> sinks_and_routes_;
  // A mapping from the view pointer to the associated activity id.
  std::map<views::View*, ash::mojom::CastSinkPtr> view_to_sink_map_;

  DISALLOW_COPY_AND_ASSIGN(CastDetailedView);
};

CastDetailedView::CastDetailedView(
    SystemTrayItem* owner,
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes)
    : TrayDetailsView(owner) {
  CreateItems();
  UpdateReceiverList(sinks_routes);
}

CastDetailedView::~CastDetailedView() {}

void CastDetailedView::SimulateViewClickedForTest(
    const std::string& receiver_id) {
  for (const auto& it : view_to_sink_map_) {
    if (it.second->id == receiver_id) {
      HandleViewClicked(it.first);
      break;
    }
  }
}

void CastDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_CAST);
}

void CastDetailedView::UpdateReceiverList(
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes) {
  // Add/update existing.
  for (const auto& it : sinks_routes)
    sinks_and_routes_[it->sink->id] = it->Clone();

  // Remove non-existent sinks. Removing an element invalidates all existing
  // iterators.
  auto i = sinks_and_routes_.begin();
  while (i != sinks_and_routes_.end()) {
    bool has_receiver = false;
    for (auto& receiver : sinks_routes) {
      if (i->first == receiver->sink->id)
        has_receiver = true;
    }

    if (has_receiver)
      ++i;
    else
      i = sinks_and_routes_.erase(i);
  }

  // Update UI.
  UpdateReceiverListFromCachedData();
  Layout();
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  // Remove all of the existing views.
  view_to_sink_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  // Add a view for each receiver.
  for (auto& it : sinks_and_routes_) {
    const ash::mojom::SinkAndRoutePtr& sink_route = it.second;
    views::View* container = AddToReceiverList(sink_route);
    view_to_sink_map_[container] = sink_route->sink.Clone();
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

views::View* CastDetailedView::AddToReceiverList(
    const ash::mojom::SinkAndRoutePtr& sink_route) {
  const gfx::ImageSkia image =
      MaterialDesignController::IsSystemTrayMenuMaterial()
          ? gfx::CreateVectorIcon(kSystemMenuCastDeviceIcon, kMenuIconColor)
          : *ui::ResourceBundle::GetSharedInstance()
                 .GetImageNamed(IDR_AURA_UBER_TRAY_CAST_DEVICE_ICON)
                 .ToImageSkia();

  HoverHighlightView* container = new HoverHighlightView(this);
  container->AddIconAndLabelCustomSize(
      image, base::UTF8ToUTF16(sink_route->sink->name), false,
      kTrayPopupDetailsIconWidth, kTrayPopupPaddingHorizontal,
      kTrayPopupPaddingBetweenItems);

  scroll_content()->AddChildView(container);
  return container;
}

void CastDetailedView::HandleViewClicked(views::View* view) {
  // Find the receiver we are going to cast to.
  auto it = view_to_sink_map_.find(view);
  if (it != view_to_sink_map_.end()) {
    WmShell::Get()->cast_config()->CastToSink(it->second.Clone());
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST);
  }
}

}  // namespace tray

TrayCast::TrayCast(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_CAST) {
  WmShell::Get()->AddShellObserver(this);
  WmShell::Get()->cast_config()->AddObserver(this);
  WmShell::Get()->cast_config()->RequestDeviceRefresh();
}

TrayCast::~TrayCast() {
  WmShell::Get()->cast_config()->RemoveObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
}

void TrayCast::StartCastForTest(const std::string& receiver_id) {
  if (detailed_ != nullptr)
    detailed_->SimulateViewClickedForTest(receiver_id);
}

void TrayCast::StopCastForTest() {
  default_->cast_view()->StopCasting();
}

const std::string& TrayCast::GetDisplayedCastId() {
  return default_->cast_view()->displayed_route_id();
}

const views::View* TrayCast::GetDefaultView() const {
  return default_;
}

views::View* TrayCast::CreateTrayView(LoginStatus status) {
  CHECK(tray_ == nullptr);
  tray_ = new tray::CastTrayView(this);
  tray_->SetVisible(HasActiveRoute());
  return tray_;
}

views::View* TrayCast::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == nullptr);

  default_ = new tray::CastDuplexView(this, status != LoginStatus::LOCKED,
                                      sinks_and_routes_);
  default_->set_id(TRAY_VIEW);
  default_->select_view()->set_id(SELECT_VIEW);
  default_->cast_view()->set_id(CAST_VIEW);

  UpdatePrimaryView();
  return default_;
}

views::View* TrayCast::CreateDetailedView(LoginStatus status) {
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_DETAILED_CAST_VIEW);
  CHECK(detailed_ == nullptr);
  detailed_ = new tray::CastDetailedView(this, sinks_and_routes_);
  return detailed_;
}

void TrayCast::DestroyTrayView() {
  tray_ = nullptr;
}

void TrayCast::DestroyDefaultView() {
  default_ = nullptr;
}

void TrayCast::DestroyDetailedView() {
  detailed_ = nullptr;
}

void TrayCast::OnDevicesUpdated(std::vector<mojom::SinkAndRoutePtr> devices) {
  sinks_and_routes_ = std::move(devices);
  UpdatePrimaryView();

  if (default_) {
    bool has_receivers = !sinks_and_routes_.empty();
    default_->SetVisible(has_receivers);
    default_->cast_view()->UpdateLabel(sinks_and_routes_);
  }
  if (detailed_)
    detailed_->UpdateReceiverList(sinks_and_routes_);
}

void TrayCast::UpdatePrimaryView() {
  if (WmShell::Get()->cast_config()->Connected() &&
      !sinks_and_routes_.empty()) {
    if (default_) {
      if (HasActiveRoute())
        default_->ActivateCastView();
      else
        default_->ActivateSelectView();
    }

    if (tray_)
      tray_->SetVisible(is_mirror_casting_);
  } else {
    if (default_)
      default_->SetVisible(false);
    if (tray_)
      tray_->SetVisible(false);
  }
}

bool TrayCast::HasActiveRoute() {
  for (const auto& sr : sinks_and_routes_) {
    if (!sr->route->title.empty() && sr->route->is_local_source)
      return true;
  }

  return false;
}

void TrayCast::OnCastingSessionStartedOrStopped(bool started) {
  is_mirror_casting_ = started;
  UpdatePrimaryView();
}

}  // namespace ash
