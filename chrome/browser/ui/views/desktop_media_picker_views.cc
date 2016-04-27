// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_media_picker_views.h"

#include <stddef.h>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/combined_desktop_media_list.h"
#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/components_strings.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/wm/core/shadow_types.h"

using content::DesktopMediaID;

namespace {

const int kThumbnailWidth = 160;
const int kThumbnailHeight = 100;
const int kThumbnailMargin = 10;
const int kLabelHeight = 40;
const int kListItemWidth = kThumbnailMargin * 2 + kThumbnailWidth;
const int kListItemHeight =
    kThumbnailMargin * 2 + kThumbnailHeight + kLabelHeight;
const int kListColumns = 3;
const int kTotalListWidth = kListColumns * kListItemWidth;

const int kDesktopMediaSourceViewGroupId = 1;

const char kDesktopMediaSourceViewClassName[] =
    "DesktopMediaPicker_DesktopMediaSourceView";

DesktopMediaID::Id AcceleratedWidgetToDesktopMediaId(
    gfx::AcceleratedWidget accelerated_widget) {
#if defined(OS_WIN)
  return reinterpret_cast<DesktopMediaID::Id>(accelerated_widget);
#else
  return static_cast<DesktopMediaID::Id>(accelerated_widget);
#endif
}

int GetMediaListViewHeightForRows(size_t rows) {
  return kListItemHeight * rows;
}

}  // namespace

DesktopMediaSourceView::DesktopMediaSourceView(
    DesktopMediaListView* parent,
    DesktopMediaID source_id)
    : parent_(parent),
      source_id_(source_id),
      image_view_(new views::ImageView()),
      label_(new views::Label()),
      selected_(false)  {
  AddChildView(image_view_);
  AddChildView(label_);
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

DesktopMediaSourceView::~DesktopMediaSourceView() {}

void DesktopMediaSourceView::SetName(const base::string16& name) {
  label_->SetText(name);
}

void DesktopMediaSourceView::SetThumbnail(const gfx::ImageSkia& thumbnail) {
  image_view_->SetImage(thumbnail);
}

void DesktopMediaSourceView::SetSelected(bool selected) {
  if (selected == selected_)
    return;
  selected_ = selected;

  if (selected) {
    // Unselect all other sources.
    Views neighbours;
    parent()->GetViewsInGroup(GetGroup(), &neighbours);
    for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
      if (*i != this) {
        DCHECK_EQ((*i)->GetClassName(), kDesktopMediaSourceViewClassName);
        DesktopMediaSourceView* source_view =
            static_cast<DesktopMediaSourceView*>(*i);
        source_view->SetSelected(false);
      }
    }

    const SkColor bg_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
    set_background(views::Background::CreateSolidBackground(bg_color));

    parent_->OnSelectionChanged();
  } else {
    set_background(NULL);
  }

  SchedulePaint();
}

const char* DesktopMediaSourceView::GetClassName() const {
  return kDesktopMediaSourceViewClassName;
}

void DesktopMediaSourceView::Layout() {
  image_view_->SetBounds(kThumbnailMargin, kThumbnailMargin,
                         kThumbnailWidth, kThumbnailHeight);
  label_->SetBounds(kThumbnailMargin, kThumbnailHeight + kThumbnailMargin,
                    kThumbnailWidth, kLabelHeight);
}

views::View* DesktopMediaSourceView::GetSelectedViewForGroup(int group) {
  Views neighbours;
  parent()->GetViewsInGroup(group, &neighbours);
  if (neighbours.empty())
    return NULL;

  for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
    DCHECK_EQ((*i)->GetClassName(), kDesktopMediaSourceViewClassName);
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(*i);
    if (source_view->selected_)
      return source_view;
  }
  return NULL;
}

bool DesktopMediaSourceView::IsGroupFocusTraversable() const {
  return false;
}

void DesktopMediaSourceView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus()) {
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(kThumbnailMargin / 2, kThumbnailMargin / 2);
    canvas->DrawFocusRect(bounds);
  }
}

void DesktopMediaSourceView::OnFocus() {
  View::OnFocus();
  SetSelected(true);
  ScrollRectToVisible(gfx::Rect(size()));
  // We paint differently when focused.
  SchedulePaint();
}

void DesktopMediaSourceView::OnBlur() {
  View::OnBlur();
  // We paint differently when focused.
  SchedulePaint();
}

bool DesktopMediaSourceView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.GetClickCount() == 1) {
    RequestFocus();
  } else if (event.GetClickCount() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
  }
  return true;
}

void DesktopMediaSourceView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      event->details().tap_count() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
    event->SetHandled();
    return;
  }

  // Detect tap gesture using ET_GESTURE_TAP_DOWN so the view also gets focused
  // on the long tap (when the tap gesture starts).
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocus();
    event->SetHandled();
  }
}

DesktopMediaListView::DesktopMediaListView(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list)
    : parent_(parent), media_list_(std::move(media_list)), weak_factory_(this) {
  media_list_->SetThumbnailSize(gfx::Size(kThumbnailWidth, kThumbnailHeight));
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

DesktopMediaListView::~DesktopMediaListView() {}

void DesktopMediaListView::StartUpdating(DesktopMediaID dialog_window_id) {
  media_list_->SetViewDialogWindowId(dialog_window_id);
  media_list_->StartUpdating(this);
}

void DesktopMediaListView::OnSelectionChanged() {
  parent_->OnSelectionChanged();
}

void DesktopMediaListView::OnDoubleClick() {
  parent_->OnDoubleClick();
}

DesktopMediaSourceView* DesktopMediaListView::GetSelection() {
  for (int i = 0; i < child_count(); ++i) {
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(child_at(i));
    DCHECK_EQ(source_view->GetClassName(), kDesktopMediaSourceViewClassName);
    if (source_view->is_selected())
      return source_view;
  }
  return NULL;
}

gfx::Size DesktopMediaListView::GetPreferredSize() const {
  int total_rows = (child_count() + kListColumns - 1) / kListColumns;
  return gfx::Size(kTotalListWidth, GetMediaListViewHeightForRows(total_rows));
}

void DesktopMediaListView::Layout() {
  int x = 0;
  int y = 0;

  for (int i = 0; i < child_count(); ++i) {
    if (x + kListItemWidth > kTotalListWidth) {
      x = 0;
      y += kListItemHeight;
    }

    View* source_view = child_at(i);
    source_view->SetBounds(x, y, kListItemWidth, kListItemHeight);

    x += kListItemWidth;
  }

  y += kListItemHeight;
  SetSize(gfx::Size(kTotalListWidth, y));
}

bool DesktopMediaListView::OnKeyPressed(const ui::KeyEvent& event) {
  int position_increment = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      position_increment = -kListColumns;
      break;
    case ui::VKEY_DOWN:
      position_increment = kListColumns;
      break;
    case ui::VKEY_LEFT:
      position_increment = -1;
      break;
    case ui::VKEY_RIGHT:
      position_increment = 1;
      break;
    default:
      return false;
  }

  if (position_increment != 0) {
    DesktopMediaSourceView* selected = GetSelection();
    DesktopMediaSourceView* new_selected = NULL;

    if (selected) {
      int index = GetIndexOf(selected);
      int new_index = index + position_increment;
      if (new_index >= child_count())
        new_index = child_count() - 1;
      else if (new_index < 0)
        new_index = 0;
      if (index != new_index) {
        new_selected =
            static_cast<DesktopMediaSourceView*>(child_at(new_index));
      }
    } else if (has_children()) {
      new_selected = static_cast<DesktopMediaSourceView*>(child_at(0));
    }

    if (new_selected) {
      GetFocusManager()->SetFocusedView(new_selected);
    }

    return true;
  }

  return false;
}

void DesktopMediaListView::OnSourceAdded(DesktopMediaList* list, int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      new DesktopMediaSourceView(this, source.id);
  source_view->SetName(source.name);
  source_view->SetGroup(kDesktopMediaSourceViewGroupId);
  AddChildViewAt(source_view, index);

  PreferredSizeChanged();

  if (child_count() % kListColumns == 1)
    parent_->OnMediaListRowsChanged();

  std::string autoselect_source =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutoSelectDesktopCaptureSource);
  if (!autoselect_source.empty() &&
      base::ASCIIToUTF16(autoselect_source) == source.name) {
    // Select, then accept and close the dialog when we're done adding sources.
    source_view->OnFocus();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DesktopMediaListView::AcceptSelection,
                   weak_factory_.GetWeakPtr()));
  }
}

void DesktopMediaListView::OnSourceRemoved(DesktopMediaList* list, int index) {
  DesktopMediaSourceView* view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  DCHECK(view);
  DCHECK_EQ(view->GetClassName(), kDesktopMediaSourceViewClassName);
  bool was_selected = view->is_selected();
  RemoveChildView(view);
  delete view;

  if (was_selected)
    OnSelectionChanged();

  PreferredSizeChanged();

  if (child_count() % kListColumns == 0)
    parent_->OnMediaListRowsChanged();
}

void DesktopMediaListView::OnSourceMoved(DesktopMediaList* list,
                                         int old_index,
                                         int new_index) {
  DesktopMediaSourceView* view =
      static_cast<DesktopMediaSourceView*>(child_at(old_index));
  ReorderChildView(view, new_index);
  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceNameChanged(DesktopMediaList* list,
                                               int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  source_view->SetName(source.name);
}

void DesktopMediaListView::OnSourceThumbnailChanged(DesktopMediaList* list,
                                                    int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  source_view->SetThumbnail(source.thumbnail);
}

void DesktopMediaListView::AcceptSelection() {
  OnSelectionChanged();
  OnDoubleClick();
}

DesktopMediaPickerDialogView::DesktopMediaPickerDialogView(
    content::WebContents* parent_web_contents,
    gfx::NativeWindow context,
    DesktopMediaPickerViews* parent,
    const base::string16& app_name,
    const base::string16& target_name,
    std::unique_ptr<DesktopMediaList> screen_list,
    std::unique_ptr<DesktopMediaList> window_list,
    std::unique_ptr<DesktopMediaList> tab_list,
    bool request_audio)
    : parent_(parent),
      app_name_(app_name),
      description_label_(new views::Label()),
      audio_share_checkbox_(nullptr),
      audio_share_checked_(true),
      sources_scroll_view_(views::ScrollView::CreateScrollViewWithBorder()) {
  std::vector<std::unique_ptr<DesktopMediaList>> media_lists;
  if (screen_list)
    media_lists.push_back(std::move(screen_list));
  if (window_list)
    media_lists.push_back(std::move(window_list));
  if (tab_list)
    media_lists.push_back(std::move(tab_list));

  std::unique_ptr<DesktopMediaList> media_list;
  if (media_lists.size() > 1)
    media_list.reset(new CombinedDesktopMediaList(media_lists));
  else
    media_list = std::move(media_lists[0]);

  DCHECK(media_list != nullptr);
  sources_list_view_ = new DesktopMediaListView(this, std::move(media_list));

  // TODO(estade): we should be getting the inside-border spacing by default as
  // a DialogDelegateView subclass, via default BubbleFrameView content margins.
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, views::kButtonHEdgeMarginNew,
      views::kPanelVertMargin, views::kLabelToControlVerticalSpacing));

  if (app_name == target_name) {
    description_label_->SetText(
        l10n_util::GetStringFUTF16(IDS_DESKTOP_MEDIA_PICKER_TEXT, app_name));
  } else {
    description_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED, app_name, target_name));
  }
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(description_label_);

  sources_scroll_view_->SetContents(sources_list_view_);
  sources_scroll_view_->ClipHeightTo(GetMediaListViewHeightForRows(1),
                                     GetMediaListViewHeightForRows(2));
  AddChildView(sources_scroll_view_);

  if (request_audio) {
    audio_share_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE));
    AddChildView(audio_share_checkbox_);
    audio_share_checkbox_->SetEnabled(false);
    audio_share_checkbox_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_TOOLTIP_NONE));
  }

  // If |parent_web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  views::Widget* widget = NULL;
  bool modal_dialog =
      parent_web_contents &&
      !parent_web_contents->GetDelegate()->IsNeverVisible(parent_web_contents);
  if (modal_dialog) {
    widget = constrained_window::ShowWebModalDialogViews(this,
                                                         parent_web_contents);
  } else {
    widget = DialogDelegate::CreateDialogWidget(this, context, NULL);
    widget->Show();
  }

  // If the picker is not modal to the calling web contents then it is displayed
  // in its own top-level window, so in that case it needs to be filtered out of
  // the list of top-level windows available for capture, and to achieve that
  // the Id is passed to DesktopMediaList.
  DesktopMediaID dialog_window_id;
  if (!modal_dialog) {
    dialog_window_id = DesktopMediaID::RegisterAuraWindow(
        DesktopMediaID::TYPE_WINDOW, widget->GetNativeWindow());

    bool is_ash_window = false;
#if defined(USE_ASH)
    is_ash_window = chrome::IsNativeWindowInAsh(widget->GetNativeWindow());
#endif

    // Set native window ID if the windows is outside Ash.
    if (!is_ash_window) {
      dialog_window_id.id = AcceleratedWidgetToDesktopMediaId(
          widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
    }
  }

  sources_list_view_->StartUpdating(dialog_window_id);
}

DesktopMediaPickerDialogView::~DesktopMediaPickerDialogView() {}

void DesktopMediaPickerDialogView::DetachParent() {
  parent_ = NULL;
}

gfx::Size DesktopMediaPickerDialogView::GetPreferredSize() const {
  static const size_t kDialogViewWidth = 600;
  return gfx::Size(kDialogViewWidth, GetHeightForWidth(kDialogViewWidth));
}

ui::ModalType DesktopMediaPickerDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 DesktopMediaPickerDialogView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_DESKTOP_MEDIA_PICKER_TITLE, app_name_);
}

bool DesktopMediaPickerDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return sources_list_view_->GetSelection() != NULL;
  return true;
}

views::View* DesktopMediaPickerDialogView::GetInitiallyFocusedView() {
  return sources_list_view_;
}

base::string16 DesktopMediaPickerDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK ?
      IDS_DESKTOP_MEDIA_PICKER_SHARE : IDS_CANCEL);
}

bool DesktopMediaPickerDialogView::Accept() {
  DesktopMediaSourceView* selection = sources_list_view_->GetSelection();

  // Ok button should only be enabled when a source is selected.
  DCHECK(selection);
  DesktopMediaID source = selection->source_id();
  source.audio_share = audio_share_checkbox_ &&
                       audio_share_checkbox_->enabled() &&
                       audio_share_checkbox_->checked();

  // If the media source is an tab, activate it.
  if (source.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
    content::WebContents* tab = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(
            source.web_contents_id.render_process_id,
            source.web_contents_id.main_render_frame_id));
    if (tab)
      tab->GetDelegate()->ActivateContents(tab);
  }

  if (parent_)
    parent_->NotifyDialogResult(source);

  // Return true to close the window.
  return true;
}

void DesktopMediaPickerDialogView::DeleteDelegate() {
  // If the dialog is being closed then notify the parent about it.
  if (parent_)
    parent_->NotifyDialogResult(DesktopMediaID());
  delete this;
}

void DesktopMediaPickerDialogView::OnSelectionChanged() {
  GetDialogClientView()->UpdateDialogButtons();

  // Disable the checkbox if we cannot support audio for the selected source.
  if (audio_share_checkbox_) {
    DesktopMediaSourceView* selection = sources_list_view_->GetSelection();

    DesktopMediaID source;
    if (selection)
      source = selection->source_id();

    if (source.type == DesktopMediaID::TYPE_SCREEN ||
        source.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
      if (!audio_share_checkbox_->enabled()) {
        audio_share_checkbox_->SetEnabled(true);
        audio_share_checkbox_->SetChecked(audio_share_checked_);
      }
      audio_share_checkbox_->SetTooltipText(base::string16());
    } else if (source.type == DesktopMediaID::TYPE_WINDOW) {
      if (audio_share_checkbox_->enabled()) {
        audio_share_checkbox_->SetEnabled(false);
        audio_share_checked_ = audio_share_checkbox_->checked();
        audio_share_checkbox_->SetChecked(false);
      }
      audio_share_checkbox_->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_TOOLTIP_WINDOW));
    } else {
      NOTREACHED();
    }
  }
}

void DesktopMediaPickerDialogView::OnDoubleClick() {
  // This will call Accept() and close the dialog.
  GetDialogClientView()->AcceptWindow();
}

void DesktopMediaPickerDialogView::OnMediaListRowsChanged() {
  gfx::Rect widget_bound = GetWidget()->GetWindowBoundsInScreen();

  int new_height = widget_bound.height() - sources_scroll_view_->height() +
                   sources_scroll_view_->GetPreferredSize().height();

  GetWidget()->CenterWindow(gfx::Size(widget_bound.width(), new_height));
}

DesktopMediaListView* DesktopMediaPickerDialogView::GetMediaListViewForTesting()
    const {
  return sources_list_view_;
}

DesktopMediaSourceView*
DesktopMediaPickerDialogView::GetMediaSourceViewForTesting(int index) const {
  if (sources_list_view_->child_count() <= index)
    return NULL;

  return reinterpret_cast<DesktopMediaSourceView*>(
      sources_list_view_->child_at(index));
}

DesktopMediaPickerViews::DesktopMediaPickerViews() : dialog_(NULL) {
}

DesktopMediaPickerViews::~DesktopMediaPickerViews() {
  if (dialog_) {
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void DesktopMediaPickerViews::Show(
    content::WebContents* web_contents,
    gfx::NativeWindow context,
    gfx::NativeWindow parent,
    const base::string16& app_name,
    const base::string16& target_name,
    std::unique_ptr<DesktopMediaList> screen_list,
    std::unique_ptr<DesktopMediaList> window_list,
    std::unique_ptr<DesktopMediaList> tab_list,
    bool request_audio,
    const DoneCallback& done_callback) {
  callback_ = done_callback;
  dialog_ = new DesktopMediaPickerDialogView(
      web_contents, context, this, app_name, target_name,
      std::move(screen_list), std::move(window_list), std::move(tab_list),
      request_audio);
}

void DesktopMediaPickerViews::NotifyDialogResult(DesktopMediaID source) {
  // Once this method is called the |dialog_| will close and destroy itself.
  dialog_->DetachParent();
  dialog_ = NULL;

  DCHECK(!callback_.is_null());

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, source));
  callback_.Reset();
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create() {
  return std::unique_ptr<DesktopMediaPicker>(new DesktopMediaPickerViews());
}
