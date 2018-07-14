// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include "base/location.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace media_router {

namespace {

// This value is negative so that it doesn't overlap with a sink index.
constexpr int kAlternativeSourceButtonId = -1;

// In the sources menu, we have a single item for "tab", which includes both
// presenting and mirroring a tab.
constexpr int kTabSource = PRESENTATION | TAB_MIRROR;

bool SupportsTabSource(const UIMediaSink& sink) {
  return base::ContainsKey(sink.cast_modes, PRESENTATION) ||
         base::ContainsKey(sink.cast_modes, TAB_MIRROR);
}

bool SupportsDesktopSource(const UIMediaSink& sink) {
  return base::ContainsKey(sink.cast_modes, DESKTOP_MIRROR);
}

}  // namespace

// static
void CastDialogView::ShowDialog(views::View* anchor_view,
                                CastDialogController* controller,
                                Browser* browser,
                                const base::Time& start_time) {
  DCHECK(!instance_);
  DCHECK(!start_time.is_null());
  instance_ = new CastDialogView(anchor_view, controller, browser, start_time);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void CastDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We also set |instance_| to nullptr in WindowClosing() which is called
  // asynchronously, because not all paths to close the dialog go through
  // HideDialog(). We set it here because IsShowing() should be false after
  // HideDialog() is called.
  instance_ = nullptr;
}

// static
bool CastDialogView::IsShowing() {
  return instance_ != nullptr;
}

// static
views::Widget* CastDialogView::GetCurrentDialogWidget() {
  return instance_ ? instance_->GetWidget() : nullptr;
}

bool CastDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 CastDialogView::GetWindowTitle() const {
  return dialog_title_;
}

base::string16 CastDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return sink_buttons_.empty()
             ? l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_START_CASTING_BUTTON)
             : sink_buttons_.at(selected_sink_index_)->GetActionText();
}

int CastDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

bool CastDialogView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return !sink_buttons_.empty() &&
         GetSelectedSink().state != UIMediaSinkState::CONNECTING;
}

views::View* CastDialogView::CreateExtraView() {
  sources_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ALTERNATIVE_SOURCES_BUTTON));
  sources_button_->set_id(kAlternativeSourceButtonId);
  sources_button_->SetEnabled(false);
  return sources_button_;
}

bool CastDialogView::Accept() {
  scroll_position_ = scroll_view_->GetVisibleRect().y();
  const UIMediaSink& sink = GetSelectedSink();
  if (!sink.route_id.empty()) {
    controller_->StopCasting(sink.route_id);
    metrics_.OnStopCasting();
  } else {
    // Go through cast modes in the order of preference to find one that is
    // supported and selected.
    for (MediaCastMode cast_mode : {PRESENTATION, TAB_MIRROR, DESKTOP_MIRROR}) {
      if ((cast_mode & selected_source_) &&
          base::ContainsKey(sink.cast_modes, cast_mode)) {
        controller_->StartCasting(sink.id, cast_mode);
        metrics_.OnStartCasting(base::Time::Now(), selected_sink_index_);
        break;
      }
    }
  }
  return false;
}

bool CastDialogView::Close() {
  return Cancel();
}

void CastDialogView::OnModelUpdated(const CastDialogModel& model) {
  if (model.media_sinks().empty()) {
    ShowNoSinksView();
  } else {
    // If |sink_buttons_| is empty, the sink list was empty before this update.
    // In that case, select the first active sink, so that its session can be
    // stopped with one click.
    if (sink_buttons_.empty())
      selected_sink_index_ = model.GetFirstActiveSinkIndex().value_or(0);
    ShowScrollView();
    PopulateScrollView(model.media_sinks());
    RestoreSinkListState();
    metrics_.OnSinksLoaded(base::Time::Now());
  }
  dialog_title_ = model.dialog_header();
  MaybeSizeToContents();
  // Update the main action button.
  DialogModelChanged();
}

void CastDialogView::OnControllerInvalidated() {
  controller_ = nullptr;
  MaybeSizeToContents();
}

void CastDialogView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender->tag() == kAlternativeSourceButtonId)
    ShowSourcesMenu();
  else
    SelectSinkAtIndex(sender->tag());
}

gfx::Size CastDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

void CastDialogView::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  metrics_.OnPaint(base::Time::Now());
}

bool CastDialogView::IsCommandIdChecked(int command_id) const {
  return command_id == selected_source_;
}

bool CastDialogView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void CastDialogView::ExecuteCommand(int command_id, int event_flags) {
  selected_source_ = command_id;
  metrics_.OnCastModeSelected();
}

CastDialogView::CastDialogView(views::View* anchor_view,
                               CastDialogController* controller,
                               Browser* browser,
                               const base::Time& start_time)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      selected_source_(kTabSource),
      controller_(controller),
      browser_(browser),
      metrics_(start_time),
      weak_factory_(this) {
  ShowNoSinksView();
}

CastDialogView::~CastDialogView() {
  if (controller_)
    controller_->RemoveObserver(this);
}

void CastDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  controller_->AddObserver(this);
  RecordSinkCountWithDelay();
}

void CastDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
  metrics_.OnCloseDialog(base::Time::Now());
}

void CastDialogView::ShowNoSinksView() {
  if (no_sinks_view_)
    return;
  if (scroll_view_) {
    // The dtor of |scroll_view_| removes it from the dialog.
    delete scroll_view_;
    scroll_view_ = nullptr;
    sink_buttons_.clear();
    selected_sink_index_ = 0;
  }
  no_sinks_view_ = new CastDialogNoSinksView(browser_);
  AddChildView(no_sinks_view_);
}

void CastDialogView::ShowScrollView() {
  if (scroll_view_)
    return;
  if (no_sinks_view_) {
    // The dtor of |no_sinks_view_| removes it from the dialog.
    delete no_sinks_view_;
    no_sinks_view_ = nullptr;
  }
  scroll_view_ = new views::ScrollView();
  AddChildView(scroll_view_);
  constexpr int kSinkButtonHeight = 50;
  scroll_view_->ClipHeightTo(0, kSinkButtonHeight * 10);
}

void CastDialogView::RestoreSinkListState() {
  if (selected_sink_index_ < sink_buttons_.size()) {
    sink_buttons_.at(selected_sink_index_)->SnapInkDropToActivated();
    SelectSinkAtIndex(selected_sink_index_);
  } else if (sink_buttons_.size() > 0u) {
    sink_buttons_.at(0)->SnapInkDropToActivated();
    SelectSinkAtIndex(0);
  }

  views::ScrollBar* scroll_bar =
      const_cast<views::ScrollBar*>(scroll_view_->vertical_scroll_bar());
  if (scroll_bar)
    scroll_view_->ScrollToPosition(scroll_bar, scroll_position_);
}

void CastDialogView::PopulateScrollView(const std::vector<UIMediaSink>& sinks) {
  sink_buttons_.clear();
  views::View* sink_list_view = new views::View();
  sink_list_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  for (size_t i = 0; i < sinks.size(); i++) {
    const UIMediaSink& sink = sinks.at(i);
    CastDialogSinkButton* sink_button = new CastDialogSinkButton(this, sink);
    sink_button->set_tag(i);
    sink_buttons_.push_back(sink_button);
    sink_list_view->AddChildView(sink_button);
  }
  scroll_view_->SetContents(sink_list_view);

  MaybeSizeToContents();
  Layout();
}

void CastDialogView::ShowSourcesMenu() {
  sources_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  const UIMediaSink& sink = GetSelectedSink();

  if (SupportsTabSource(sink)) {
    sources_menu_model_->AddCheckItemWithStringId(
        kTabSource, IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE);
  }
  if (SupportsDesktopSource(sink)) {
    sources_menu_model_->AddCheckItemWithStringId(
        DESKTOP_MIRROR, IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
  }

  sources_menu_runner_ = std::make_unique<views::MenuRunner>(
      sources_menu_model_.get(), views::MenuRunner::COMBOBOX);
  const gfx::Rect& screen_bounds = sources_button_->GetBoundsInScreen();
  sources_menu_runner_->RunMenuAt(sources_button_->GetWidget(), nullptr,
                                  screen_bounds, views::MENU_ANCHOR_TOPLEFT,
                                  ui::MENU_SOURCE_MOUSE);
}

void CastDialogView::UpdateSourcesMenu(const UIMediaSink& sink) {
  bool supports_desktop_source = SupportsDesktopSource(sink);
  // If desktop mirroring is supported, show the sources menu button so that
  // the user can switch sources.
  if (sources_button_)
    sources_button_->SetEnabled(supports_desktop_source);
  if (!supports_desktop_source && selected_source_ == DESKTOP_MIRROR)
    selected_source_ = kTabSource;
}

void CastDialogView::SelectSinkAtIndex(size_t index) {
  if (selected_sink_index_ != index &&
      selected_sink_index_ < sink_buttons_.size()) {
    sink_buttons_.at(selected_sink_index_)->SetSelected(false);
  }
  CastDialogSinkButton* selected_button = sink_buttons_.at(index);
  selected_button->SetSelected(true);
  selected_sink_index_ = index;

  UpdateSourcesMenu(selected_button->sink());
  // Update the text on the main action button.
  DialogModelChanged();
}

const UIMediaSink& CastDialogView::GetSelectedSink() const {
  DCHECK_GT(sink_buttons_.size(), selected_sink_index_);
  return sink_buttons_.at(selected_sink_index_)->sink();
}

void CastDialogView::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

void CastDialogView::RecordSinkCountWithDelay() {
  // Record the number of sinks after three seconds. This is consistent with the
  // WebUI dialog.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CastDialogView::RecordSinkCount,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(3));
}

void CastDialogView::RecordSinkCount() {
  metrics_.OnRecordSinkCount(sink_buttons_.size());
}

// static
CastDialogView* CastDialogView::instance_ = nullptr;

}  // namespace media_router
