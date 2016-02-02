// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/system/chromeos/screen_security/screen_tray_item.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/throbber_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "base/bind.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

const size_t kMaximumStatusStringLength = 100;
const int kStopButtonRightPadding = 18;

// Returns the active CastConfigDelegate instance.
ash::CastConfigDelegate* GetCastConfigDelegate() {
  return ash::Shell::GetInstance()
      ->system_tray_delegate()
      ->GetCastConfigDelegate();
}

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

}  // namespace

namespace tray {

// This view is displayed in the system tray when the cast extension is active.
// It asks the user if they want to cast the desktop. If they click on the
// chevron, then a detail view will replace this view where the user will
// actually pick the cast receiver.
class CastSelectDefaultView : public TrayItemMore {
 public:
  CastSelectDefaultView(SystemTrayItem* owner,
                        bool show_more);
  ~CastSelectDefaultView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSelectDefaultView);
};

CastSelectDefaultView::CastSelectDefaultView(SystemTrayItem* owner,
                                             bool show_more)
    : TrayItemMore(owner, show_more) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Update the image and label.
  SetImage(rb.GetImageNamed(IDR_AURA_UBER_TRAY_CAST).ToImageSkia());
  base::string16 label =
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAST_DESKTOP);
  SetLabel(label);
  SetAccessibleName(label);
}

CastSelectDefaultView::~CastSelectDefaultView() {}

// This view is displayed when the screen is actively being casted; it allows
// the user to easily stop casting. It fully replaces the
// |CastSelectDefaultView| view inside of the |CastDuplexView|.
class CastCastView : public views::View, public views::ButtonListener {
 public:
  CastCastView();
  ~CastCastView() override;

  void StopCasting();

  const std::string& displayed_activity_id() const {
    return displayed_activity_id_;
  }

  // Updates the label for the stop view to include information about the
  // current device that is being casted.
  void UpdateLabel(
      const CastConfigDelegate::ReceiversAndActivities& receivers_activities);

 private:
  // Overridden from views::View.
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  std::string displayed_activity_id_;

  views::ImageView* icon_;
  views::Label* label_;
  TrayPopupLabelButton* stop_button_;

  DISALLOW_COPY_AND_ASSIGN(CastCastView);
};

CastCastView::CastCastView() {
  // We will initialize the primary tray view which shows a stop button here.

  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kTrayPopupPaddingHorizontal, 0,
                                        kTrayPopupPaddingBetweenItems));
  icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
  icon_->SetImage(
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAST_ENABLED).ToImageSkia());
  AddChildView(icon_);

  // The label which describes both what we are casting (ie, the desktop) and
  // where we are casting it to.
  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  label_->SetText(
      bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN));
  AddChildView(label_);

  // Add the stop bottom on the far-right. We customize how this stop button is
  // displayed inside of |Layout()|.
  base::string16 stop_button_text =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_ASH_STATUS_TRAY_CAST_STOP);
  stop_button_ = new TrayPopupLabelButton(this, stop_button_text);
  AddChildView(stop_button_);
}

CastCastView::~CastCastView() {
}

int CastCastView::GetHeightForWidth(int width) const {
  // We are reusing the cached label_->bounds() calculation which was
  // done inside of Layout(). Due to the way this object is initialized,
  // Layout() will always get initially invoked with the dummy text
  // (which will compute the proper label width) and then when we know
  // the cast receiver we will update the label text, which will cause
  // this method to get invoked.
  return std::max(views::View::GetHeightForWidth(width),
                  kTrayPopupPaddingBetweenItems * 2 +
                      label_->GetHeightForWidth(label_->bounds().width()));
}

void CastCastView::Layout() {
  views::View::Layout();

  // Give the stop button the space it requests.
  gfx::Size stop_size = stop_button_->GetPreferredSize();
  gfx::Rect stop_bounds(stop_size);
  stop_bounds.set_x(width() - stop_size.width() - kStopButtonRightPadding);
  stop_bounds.set_y((height() - stop_size.height()) / 2);
  stop_button_->SetBoundsRect(stop_bounds);

  // Adjust the label's bounds in case it got cut off by |stop_button_|.
  if (label_->bounds().Intersects(stop_button_->bounds())) {
    gfx::Rect label_bounds = label_->bounds();
    label_bounds.set_width(stop_button_->x() - kTrayPopupPaddingBetweenItems -
                           label_->x());
    label_->SetBoundsRect(label_bounds);
  }
}

void CastCastView::StopCasting() {
  GetCastConfigDelegate()->StopCasting(displayed_activity_id_);
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_CAST_STOP_CAST);
}

void CastCastView::UpdateLabel(
    const CastConfigDelegate::ReceiversAndActivities& receivers_activities) {
  for (auto& i : receivers_activities) {
    const CastConfigDelegate::Receiver& receiver = i.receiver;
    const CastConfigDelegate::Activity& activity = i.activity;

    if (!activity.id.empty()) {
      displayed_activity_id_ = activity.id;

      // We want to display different labels inside of the title depending on
      // what we are actually casting - either the desktop, a tab, or a fallback
      // that catches everything else (ie, an extension tab).
      if (activity.tab_id == CastConfigDelegate::Activity::TabId::DESKTOP) {
        label_->SetText(ElideString(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP, receiver.name)));
      } else if (activity.tab_id >= 0) {
        label_->SetText(ElideString(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_CAST_CAST_TAB, activity.title, receiver.name)));
      } else {
        label_->SetText(
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN));
      }

      PreferredSizeChanged();
      Layout();

      // If this machine is the source of the activity, then we want to display
      // it over any other activity. There can be multiple activities if other
      // devices on the network are casting at the same time.
      if (activity.is_local_source)
        break;
    }
  }
}

void CastCastView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  DCHECK(sender == stop_button_);
  StopCasting();
}

// This view by itself does very little. It acts as a front-end for managing
// which of the two child views (|CastSelectDefaultView| and |CastCastView|)
// is active.
class CastDuplexView : public views::View {
 public:
  CastDuplexView(
      SystemTrayItem* owner,
      bool show_more,
      const CastConfigDelegate::ReceiversAndActivities& receivers_activities);
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
    bool show_more,
    const CastConfigDelegate::ReceiversAndActivities& receivers_activities) {
  select_view_ = new CastSelectDefaultView(owner, show_more);
  cast_view_ = new CastCastView();
  cast_view_->UpdateLabel(receivers_activities);
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

  // Called when the tray alignment changes so that the icon can recenter
  // itself.
  void UpdateAlignment(ShelfAlignment alignment);

 private:
  DISALLOW_COPY_AND_ASSIGN(CastTrayView);
};

CastTrayView::CastTrayView(SystemTrayItem* tray_item)
    : TrayItemView(tray_item) {
  CreateImageView();

  image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                             .GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE)
                             .ToImageSkia());
}

CastTrayView::~CastTrayView() {
}

void CastTrayView::UpdateAlignment(ShelfAlignment alignment) {
  // Center the item dependent on the orientation of the shelf.
  views::BoxLayout::Orientation layout = views::BoxLayout::kHorizontal;
  switch (alignment) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
    case ash::SHELF_ALIGNMENT_TOP:
      layout = views::BoxLayout::kHorizontal;
      break;
    case ash::SHELF_ALIGNMENT_LEFT:
    case ash::SHELF_ALIGNMENT_RIGHT:
      layout = views::BoxLayout::kVertical;
      break;
  }
  SetLayoutManager(new views::BoxLayout(layout, 0, 0, 0));
  Layout();
}

// This view displays a list of cast receivers that can be clicked on and casted
// to. It is activated by clicking on the chevron inside of
// |CastSelectDefaultView|.
class CastDetailedView : public TrayDetailsView, public ViewClickListener {
 public:
  CastDetailedView(SystemTrayItem* owner,
                   user::LoginStatus login,
                   const CastConfigDelegate::ReceiversAndActivities&
                       receivers_and_activities);
  ~CastDetailedView() override;

  // Makes the detail view think the view associated with the given receiver_id
  // was clicked. This will start a cast.
  void SimulateViewClickedForTest(const std::string& receiver_id);

  // Updates the list of available receivers.
  void UpdateReceiverList(const CastConfigDelegate::ReceiversAndActivities&
                              new_receivers_and_activities);

 private:
  void CreateItems();

  void UpdateReceiverListFromCachedData();
  views::View* AddToReceiverList(
      const CastConfigDelegate::ReceiverAndActivity& receiverActivity);

  void AppendSettingsEntries();
  void AppendHeaderEntry();

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;

  user::LoginStatus login_;
  views::View* options_ = nullptr;
  // A mapping from the receiver id to the receiver/activity data.
  std::map<std::string, CastConfigDelegate::ReceiverAndActivity>
      receivers_and_activities_;
  // A mapping from the view pointer to the associated activity id.
  std::map<views::View*, std::string> receiver_activity_map_;

  DISALLOW_COPY_AND_ASSIGN(CastDetailedView);
};

CastDetailedView::CastDetailedView(
    SystemTrayItem* owner,
    user::LoginStatus login,
    const CastConfigDelegate::ReceiversAndActivities& receivers_and_activities)
    : TrayDetailsView(owner), login_(login) {
  CreateItems();
  UpdateReceiverList(receivers_and_activities);
}

CastDetailedView::~CastDetailedView() {
}

void CastDetailedView::SimulateViewClickedForTest(
    const std::string& receiver_id) {
  for (auto& it : receiver_activity_map_) {
    if (it.second == receiver_id) {
      OnViewClicked(it.first);
      break;
    }
  }
}

void CastDetailedView::CreateItems() {
  CreateScrollableList();
  if (GetCastConfigDelegate()->HasOptions())
    AppendSettingsEntries();
  AppendHeaderEntry();
}

void CastDetailedView::UpdateReceiverList(
    const CastConfigDelegate::ReceiversAndActivities&
        new_receivers_and_activities) {
  // Add/update existing.
  for (auto i = new_receivers_and_activities.begin();
       i != new_receivers_and_activities.end(); ++i) {
    receivers_and_activities_[i->receiver.id] = *i;
  }

  // Remove non-existent receivers. Removing an element invalidates all existing
  // iterators.
  auto i = receivers_and_activities_.begin();
  while (i != receivers_and_activities_.end()) {
    bool has_receiver = false;
    for (auto receiver : new_receivers_and_activities) {
      if (i->first == receiver.receiver.id)
        has_receiver = true;
    }

    if (has_receiver)
      ++i;
    else
      i = receivers_and_activities_.erase(i);
  }

  // Update UI.
  UpdateReceiverListFromCachedData();
  Layout();
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  // Remove all of the existing views.
  receiver_activity_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  // Add a view for each receiver.
  for (auto& it : receivers_and_activities_) {
    const CastConfigDelegate::ReceiverAndActivity& receiver_activity =
        it.second;
    views::View* container = AddToReceiverList(receiver_activity);
    receiver_activity_map_[container] = it.first;
  }

  scroll_content()->SizeToPreferredSize();
  static_cast<views::View*>(scroller())->Layout();
}

views::View* CastDetailedView::AddToReceiverList(
    const CastConfigDelegate::ReceiverAndActivity& receiverActivity) {
  HoverHighlightView* container = new HoverHighlightView(this);

  const gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance()
          .GetImageNamed(IDR_AURA_UBER_TRAY_CAST_DEVICE_ICON)
          .ToImageSkia();
  const base::string16& name = receiverActivity.receiver.name;
  container->AddIndentedIconAndLabel(*image, name, false);

  scroll_content()->AddChildView(container);
  return container;
}

void CastDetailedView::AppendSettingsEntries() {
  // Settings requires a browser window, hide it for non logged in user.
  const bool userAddingRunning = Shell::GetInstance()
                                     ->session_state_delegate()
                                     ->IsInSecondaryLoginScreen();

  if (login_ == user::LOGGED_IN_NONE || login_ == user::LOGGED_IN_LOCKED ||
      userAddingRunning)
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(this);
  container->AddLabel(rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAST_OPTIONS),
                      gfx::ALIGN_LEFT, false /* highlight */);

  AddChildView(container);
  options_ = container;
}

void CastDetailedView::AppendHeaderEntry() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_CAST, this);
}

void CastDetailedView::OnViewClicked(views::View* sender) {
  ash::CastConfigDelegate* cast_config_delegate = GetCastConfigDelegate();

  if (sender == footer()->content()) {
    TransitionToDefaultView();
  } else if (sender == options_) {
    cast_config_delegate->LaunchCastOptions();
  } else {
    // Find the receiver we are going to cast to
    auto it = receiver_activity_map_.find(sender);
    if (it != receiver_activity_map_.end()) {
      cast_config_delegate->CastToReceiver(it->second);
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          ash::UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST);
    }
  }
}

}  // namespace tray

TrayCast::TrayCast(SystemTray* system_tray) : SystemTrayItem(system_tray) {
  Shell::GetInstance()->AddShellObserver(this);
}

TrayCast::~TrayCast() {
  Shell::GetInstance()->RemoveShellObserver(this);
  if (added_observer_)
    GetCastConfigDelegate()->RemoveObserver(this);
}

void TrayCast::StartCastForTest(const std::string& receiver_id) {
  if (detailed_ != nullptr)
    detailed_->SimulateViewClickedForTest(receiver_id);
}

void TrayCast::StopCastForTest() {
  default_->cast_view()->StopCasting();
}

const std::string& TrayCast::GetDisplayedCastId() {
  return default_->cast_view()->displayed_activity_id();
}

const views::View* TrayCast::GetDefaultView() const {
  return default_;
}

views::View* TrayCast::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == nullptr);
  tray_ = new tray::CastTrayView(this);
  tray_->SetVisible(is_casting_);
  return tray_;
}

views::View* TrayCast::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == nullptr);

  if (HasCastExtension()) {
    ash::CastConfigDelegate* cast_config_delegate = GetCastConfigDelegate();

    // Add the cast observer here instead of the ctor for two reasons:
    // - The ctor gets called too early in the initialization cycle (at least
    //   for the tests); the correct profile hasn't been setup yet.
    // - If we're using the cast extension backend (media router is disabled),
    //   then the user can install the extension at any point in time. The
    //   return value of HasCastExtension() can change, so only checking it in
    //   the ctor isn't enough.
    if (!added_observer_) {
      cast_config_delegate->AddObserver(this);
      added_observer_ = true;
    }

    // The extension updates its view model whenever the popup is opened, so we
    // probably should as well.
    cast_config_delegate->RequestDeviceRefresh();
  }

  default_ = new tray::CastDuplexView(this, status != user::LOGGED_IN_LOCKED,
                                      receivers_and_activities_);
  default_->set_id(TRAY_VIEW);
  default_->select_view()->set_id(SELECT_VIEW);
  default_->cast_view()->set_id(CAST_VIEW);

  UpdatePrimaryView();
  return default_;
}

views::View* TrayCast::CreateDetailedView(user::LoginStatus status) {
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_DETAILED_CAST_VIEW);
  CHECK(detailed_ == nullptr);
  detailed_ =
      new tray::CastDetailedView(this, status, receivers_and_activities_);
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

bool TrayCast::HasCastExtension() {
  ash::CastConfigDelegate* cast_config_delegate = GetCastConfigDelegate();
  return cast_config_delegate != nullptr &&
         cast_config_delegate->HasCastExtension();
}

void TrayCast::OnDevicesUpdated(
    const CastConfigDelegate::ReceiversAndActivities& receivers_activities) {
  receivers_and_activities_ = receivers_activities;

  if (default_) {
    bool has_receivers = !receivers_and_activities_.empty();
    default_->SetVisible(has_receivers);
    default_->cast_view()->UpdateLabel(receivers_and_activities_);
  }
  if (detailed_)
    detailed_->UpdateReceiverList(receivers_and_activities_);
}

void TrayCast::UpdatePrimaryView() {
  if (HasCastExtension() && !receivers_and_activities_.empty()) {
    if (default_) {
      if (is_casting_)
        default_->ActivateCastView();
      else
        default_->ActivateSelectView();
    }

    if (tray_)
      tray_->SetVisible(is_casting_);
  } else {
    if (default_)
      default_->SetVisible(false);
    if (tray_)
      tray_->SetVisible(false);
  }
}

void TrayCast::OnCastingSessionStartedOrStopped(bool started) {
  is_casting_ = started;
  UpdatePrimaryView();
}

void TrayCast::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (tray_)
    tray_->UpdateAlignment(alignment);
}

}  // namespace ash
