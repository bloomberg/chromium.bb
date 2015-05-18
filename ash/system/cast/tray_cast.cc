// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include "ash/cast_config_delegate.h"
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
#include "base/memory/weak_ptr.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {
const int kStopButtonRightPadding = 18;

// Callback helper for StopCast().
void StopCastCallback(
    CastConfigDelegate* cast_config,
    const CastConfigDelegate::ReceiversAndActivites& receivers_activities) {
  for (auto& item : receivers_activities) {
    CastConfigDelegate::Activity activity = item.second.activity;
    if (activity.allow_stop && activity.id.empty() == false)
      cast_config->StopCasting(activity.id);
  }
}

// Stops currently casting device.
void StopCast() {
  CastConfigDelegate* cast_config =
      Shell::GetInstance()->system_tray_delegate()->GetCastConfigDelegate();
  if (cast_config && cast_config->HasCastExtension()) {
    cast_config->GetReceiversAndActivities(
        base::Bind(&StopCastCallback, cast_config));
  }
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
                        CastConfigDelegate* cast_config_delegate,
                        bool show_more);
  ~CastSelectDefaultView() override;

  // Updates the label based on the current set of receivers (if there are or
  // are not any available receivers).
  void UpdateLabel();

 private:
  void UpdateLabelCallback(
      const CastConfigDelegate::ReceiversAndActivites& receivers_activities);

  CastConfigDelegate* cast_config_delegate_;
  DISALLOW_COPY_AND_ASSIGN(CastSelectDefaultView);
};

CastSelectDefaultView::CastSelectDefaultView(
    SystemTrayItem* owner,
    CastConfigDelegate* cast_config_delegate,
    bool show_more)
    : TrayItemMore(owner, show_more),
      cast_config_delegate_(cast_config_delegate) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(rb.GetImageNamed(IDR_AURA_UBER_TRAY_CAST).ToImageSkia());

  // We first set a default label before we actually know what the label will
  // be, because it could take awhile before UpdateLabel() actually applies
  // the correct label.
  SetLabel(rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAST_NO_DEVICE));
  UpdateLabel();
}

CastSelectDefaultView::~CastSelectDefaultView() {
}

void CastSelectDefaultView::UpdateLabelCallback(
    const CastConfigDelegate::ReceiversAndActivites& receivers_activities) {
  // The label needs to reflect if there are no cast receivers
  const base::string16 label =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          receivers_activities.empty() ? IDS_ASH_STATUS_TRAY_CAST_NO_DEVICE
                                       : IDS_ASH_STATUS_TRAY_CAST_DESKTOP);
  SetLabel(label);
  SetAccessibleName(label);
  SetVisible(true);
}

void CastSelectDefaultView::UpdateLabel() {
  if (cast_config_delegate_ == nullptr ||
      cast_config_delegate_->HasCastExtension() == false)
    return;

  cast_config_delegate_->GetReceiversAndActivities(base::Bind(
      &CastSelectDefaultView::UpdateLabelCallback, base::Unretained(this)));
}

// This view is displayed when the screen is actively being casted; it allows
// the user to easily stop casting. It fully replaces the
// |CastSelectDefaultView| view inside of the |CastDuplexView|.
class CastCastView : public views::View, public views::ButtonListener {
 public:
  explicit CastCastView(CastConfigDelegate* cast_config_delegate);
  ~CastCastView() override;

  // Updates the label for the stop view to include information about the
  // current device that is being casted.
  void UpdateLabel();

 private:
  void UpdateLabelCallback(
      const CastConfigDelegate::ReceiversAndActivites& receivers_activities);

  // Overridden from views::View.
  void Layout() override;
  // Overridden from views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  CastConfigDelegate* cast_config_delegate_;
  views::ImageView* icon_;
  views::View* label_container_;
  views::Label* title_;
  views::Label* details_;
  TrayPopupLabelButton* stop_button_;

  DISALLOW_COPY_AND_ASSIGN(CastCastView);
};

CastCastView::CastCastView(CastConfigDelegate* cast_config_delegate)
    : cast_config_delegate_(cast_config_delegate) {
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

  // The view has two labels, one above the other. The top label (|title_|)
  // specifies that we are, say, "Casting desktop". The bottom label
  // (|details_|) specifies where we are casting to, ie, "SomeRandom cast"
  label_container_ = new views::View;
  label_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  title_ = new views::Label;
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetFontList(bundle.GetFontList(ui::ResourceBundle::BoldFont));
  label_container_->AddChildView(title_);

  details_ = new views::Label;
  details_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details_->SetMultiLine(false);
  details_->SetEnabledColor(kHeaderTextColorNormal);
  label_container_->AddChildView(details_);

  AddChildView(label_container_);

  // Add the stop bottom on the far-right. We customize how this stop button is
  // displayed inside of |Layout()|.
  base::string16 stop_button_text =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_ASH_STATUS_TRAY_CAST_STOP);
  stop_button_ = new TrayPopupLabelButton(this, stop_button_text);
  AddChildView(stop_button_);

  UpdateLabel();
}

CastCastView::~CastCastView() {
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
  if (label_container_->bounds().Intersects(stop_button_->bounds())) {
    gfx::Rect label_bounds = label_container_->bounds();
    label_bounds.set_width(stop_button_->x() - kTrayPopupPaddingBetweenItems -
                           label_container_->x());
    label_container_->SetBoundsRect(label_bounds);
  }

  // Center the label.
  // TODO(jdufault): Why doesn't this happen automatically?
  const int extra_height =
      height() - label_container_->GetPreferredSize().height();
  label_container_->SetY(extra_height / 2);
}

void CastCastView::UpdateLabel() {
  if (cast_config_delegate_ == nullptr ||
      cast_config_delegate_->HasCastExtension() == false)
    return;

  cast_config_delegate_->GetReceiversAndActivities(
      base::Bind(&CastCastView::UpdateLabelCallback, base::Unretained(this)));
}

void CastCastView::UpdateLabelCallback(
    const CastConfigDelegate::ReceiversAndActivites& receivers_activities) {
  for (auto& i : receivers_activities) {
    const CastConfigDelegate::Receiver receiver = i.second.receiver;
    const CastConfigDelegate::Activity activity = i.second.activity;
    if (!activity.id.empty()) {
      // We want to display different labels inside of the title depending on
      // what we are actually casting - either the desktop, a tab, or a fallback
      // that catches everything else (ie, an extension tab).
      if (activity.tab_id == CastConfigDelegate::Activity::TabId::DESKTOP) {
        title_->SetText(
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP));
      } else if (activity.tab_id >= 0) {
        title_->SetText(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_CAST_CAST_TAB, activity.title));
      } else {
        // We will fallback to whatever the extension provides us
        title_->SetText(activity.title);
      }

      details_->SetText(receiver.name);
      Layout();
      break;
    }
  }
}

void CastCastView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  DCHECK(sender == stop_button_);
  StopCast();
}

// This view by itself does very little. It acts as a front-end for managing
// which of the two child views (|CastSelectDefaultView| and |CastCastView|)
// is active.
class CastDuplexView : public views::View {
 public:
  CastDuplexView(SystemTrayItem* owner,
                 CastConfigDelegate* config_delegate,
                 bool show_more);
  ~CastDuplexView() override;

  // Activate either the casting or select view.
  void ActivateCastView();
  void ActivateSelectView();

  CastSelectDefaultView* select_view() { return select_view_; }
  CastCastView* cast_view() { return cast_view_; }

 private:
  // Overridden from views::View.
  void Layout() override;

  CastSelectDefaultView* select_view_;
  CastCastView* cast_view_;

  DISALLOW_COPY_AND_ASSIGN(CastDuplexView);
};

CastDuplexView::CastDuplexView(SystemTrayItem* owner,
                               CastConfigDelegate* config_delegate,
                               bool show_more) {
  select_view_ = new CastSelectDefaultView(owner, config_delegate, show_more);
  cast_view_ = new CastCastView(config_delegate);
  SetLayoutManager(new views::FillLayout());
  AddChildView(select_view_);
  AddChildView(cast_view_);

  ActivateSelectView();
}

CastDuplexView::~CastDuplexView() {
}

void CastDuplexView::ActivateCastView() {
  select_view_->SetVisible(false);
  cast_view_->SetVisible(true);
  InvalidateLayout();
}

void CastDuplexView::ActivateSelectView() {
  select_view_->SetVisible(true);
  cast_view_->SetVisible(false);
  InvalidateLayout();
}

void CastDuplexView::Layout() {
  views::View::Layout();

  if (select_view_->IsDrawn())
    select_view_->SetBoundsRect(GetContentsBounds());
  if (cast_view_->IsDrawn())
    cast_view_->SetBoundsRect(GetContentsBounds());
}

// Exposes an icon in the tray. |TrayCast| manages the visiblity of this.
class CastTrayView : public TrayItemView {
 public:
  CastTrayView(SystemTrayItem* tray_item);
  ~CastTrayView() override;

  // Called when the tray alignment changes so that the icon can recenter
  // itself.
  void UpdateAlignment(ShelfAlignment alignment);

  // Returns a weak pointer to this instance.
  base::WeakPtr<CastTrayView> AsWeakPtr();

 private:
  base::WeakPtrFactory<CastTrayView> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastTrayView);
};

CastTrayView::CastTrayView(SystemTrayItem* tray_item)
    : TrayItemView(tray_item), weak_ptr_factory_(this) {
  CreateImageView();

  image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                             .GetImageNamed(IDR_AURA_UBER_TRAY_CAST_STATUS)
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

base::WeakPtr<CastTrayView> CastTrayView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// This view displays a list of cast receivers that can be clicked on and casted
// to. It is activated by clicking on the chevron inside of
// |CastSelectDefaultView|.
class CastDetailedView : public TrayDetailsView, public ViewClickListener {
 public:
  CastDetailedView(SystemTrayItem* owner,
                   CastConfigDelegate* cast_config_delegate,
                   user::LoginStatus login);
  ~CastDetailedView() override;

 private:
  void CreateItems();

  void UpdateReceiverList();
  void UpdateReceiverListCallback(
      const CastConfigDelegate::ReceiversAndActivites&
          new_receivers_and_activities);
  void UpdateReceiverListFromCachedData();
  views::View* AddToReceiverList(
      const CastConfigDelegate::ReceiverAndActivity& receiverActivity);

  void AppendSettingsEntries();
  void AppendHeaderEntry();

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;

  CastConfigDelegate* cast_config_delegate_;
  user::LoginStatus login_;
  views::View* options_;
  CastConfigDelegate::ReceiversAndActivites receivers_and_activities_;
  // A mapping from the view pointer to the associated activity id
  std::map<views::View*, std::string> receiver_activity_map_;

  DISALLOW_COPY_AND_ASSIGN(CastDetailedView);
};

CastDetailedView::CastDetailedView(SystemTrayItem* owner,
                                   CastConfigDelegate* cast_config_delegate,
                                   user::LoginStatus login)
    : TrayDetailsView(owner),
      cast_config_delegate_(cast_config_delegate),
      login_(login),
      options_(nullptr) {
  CreateItems();
  UpdateReceiverList();
}

CastDetailedView::~CastDetailedView() {
}

void CastDetailedView::CreateItems() {
  CreateScrollableList();
  AppendSettingsEntries();
  AppendHeaderEntry();
}

void CastDetailedView::UpdateReceiverList() {
  cast_config_delegate_->GetReceiversAndActivities(base::Bind(
      &CastDetailedView::UpdateReceiverListCallback, base::Unretained(this)));
}

void CastDetailedView::UpdateReceiverListCallback(
    const CastConfigDelegate::ReceiversAndActivites&
        new_receivers_and_activities) {
  // Add/update existing.
  for (auto i = new_receivers_and_activities.begin();
       i != new_receivers_and_activities.end(); ++i) {
    receivers_and_activities_[i->first] = i->second;
  }
  // Remove non-existent.
  for (auto i = receivers_and_activities_.begin();
       i != receivers_and_activities_.end(); ++i) {
    if (new_receivers_and_activities.count(i->first) == 0)
      receivers_and_activities_.erase(i->first);
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
  if (sender == footer()->content()) {
    TransitionToDefaultView();
  } else if (sender == options_) {
    cast_config_delegate_->LaunchCastOptions();
  } else {
    // Find the receiver we are going to cast to
    auto it = receiver_activity_map_.find(sender);
    if (it != receiver_activity_map_.end()) {
      cast_config_delegate_->CastToReceiver(it->second);
    }
  }
}

}  // namespace tray

TrayCast::TrayCast(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      cast_config_delegate_(ash::Shell::GetInstance()
                                ->system_tray_delegate()
                                ->GetCastConfigDelegate()) {
  Shell::GetInstance()->AddShellObserver(this);
}

TrayCast::~TrayCast() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

views::View* TrayCast::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == nullptr);
  tray_ = new tray::CastTrayView(this);
  tray_->SetVisible(is_casting_);
  return tray_;
}

views::View* TrayCast::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == nullptr);
  default_ = new tray::CastDuplexView(this, cast_config_delegate_,
                                      status != user::LOGGED_IN_LOCKED);
  UpdatePrimaryView();
  return default_;
}

views::View* TrayCast::CreateDetailedView(user::LoginStatus status) {
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_DETAILED_CAST_VIEW);
  CHECK(detailed_ == nullptr);
  detailed_ = new tray::CastDetailedView(this, cast_config_delegate_, status);
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
  return cast_config_delegate_ != nullptr &&
         cast_config_delegate_->HasCastExtension();
}

void TrayCast::UpdatePrimaryView() {
  if (HasCastExtension() == false) {
    if (default_)
      default_->SetVisible(false);
    if (tray_) {
      base::MessageLoopForUI::current()->PostTask(
          FROM_HERE, base::Bind(&tray::CastTrayView::SetVisible,
                                tray_->AsWeakPtr(), false));
    }
  } else {
    if (default_) {
      if (is_casting_)
        default_->ActivateCastView();
      else
        default_->ActivateSelectView();
    }

    if (tray_)
      tray_->SetVisible(is_casting_);
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
