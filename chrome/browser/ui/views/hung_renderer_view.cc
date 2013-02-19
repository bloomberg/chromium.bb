// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include <windows.h>
#endif

#include "base/i18n/rtl.h"
#include "base/memory/scoped_vector.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using content::WebContents;

// These functions allow certain chrome platforms to override the default hung
// renderer dialog. For e.g. Chrome on Windows 8 metro
bool PlatformShowCustomHungRendererDialog(WebContents* contents);
bool PlatformHideCustomHungRendererDialog(WebContents* contents);

#if !defined(OS_WIN)
bool PlatformShowCustomHungRendererDialog(WebContents* contents) {
  return false;
}

bool PlatformHideCustomHungRendererDialog(WebContents* contents) {
  return false;
}
#endif  // OS_WIN

HungRendererDialogView* HungRendererDialogView::g_instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, public:

HungPagesTableModel::HungPagesTableModel(Delegate* delegate)
    : observer_(NULL),
      delegate_(delegate) {
}

HungPagesTableModel::~HungPagesTableModel() {
}

content::RenderProcessHost* HungPagesTableModel::GetRenderProcessHost() {
  return tab_observers_.empty() ? NULL :
      tab_observers_[0]->web_contents()->GetRenderProcessHost();
}

content::RenderViewHost* HungPagesTableModel::GetRenderViewHost() {
  return tab_observers_.empty() ? NULL :
      tab_observers_[0]->web_contents()->GetRenderViewHost();
}

void HungPagesTableModel::InitForWebContents(WebContents* hung_contents) {
  tab_observers_.clear();
  if (hung_contents) {
    // Force hung_contents to be first.
    if (hung_contents) {
      tab_observers_.push_back(new WebContentsObserverImpl(this,
                                                           hung_contents));
    }
    for (TabContentsIterator it; !it.done(); it.Next()) {
      if (*it != hung_contents &&
          it->GetRenderProcessHost() == hung_contents->GetRenderProcessHost())
        tab_observers_.push_back(new WebContentsObserverImpl(this, *it));
    }
  }
  // The world is different.
  if (observer_)
    observer_->OnModelChanged();
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, ui::TableModel implementation:

int HungPagesTableModel::RowCount() {
  return static_cast<int>(tab_observers_.size());
}

string16 HungPagesTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  string16 title = tab_observers_[row]->web_contents()->GetTitle();
  if (title.empty())
    title = CoreTabHelper::GetDefaultTitle();
  // TODO(xji): Consider adding a special case if the title text is a URL,
  // since those should always have LTR directionality. Please refer to
  // http://crbug.com/6726 for more information.
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

gfx::ImageSkia HungPagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return FaviconTabHelper::FromWebContents(
      tab_observers_[row]->web_contents())->GetFavicon().AsImageSkia();
}

void HungPagesTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void HungPagesTableModel::GetGroupRange(int model_index,
                                        views::GroupRange* range) {
  DCHECK(range);
  range->start = 0;
  range->length = RowCount();
}

void HungPagesTableModel::TabDestroyed(WebContentsObserverImpl* tab) {
  // Clean up tab_observers_ and notify our observer.
  TabObservers::iterator i = std::find(
      tab_observers_.begin(), tab_observers_.end(), tab);
  DCHECK(i != tab_observers_.end());
  int index = static_cast<int>(i - tab_observers_.begin());
  tab_observers_.erase(i);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);

  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

HungPagesTableModel::WebContentsObserverImpl::WebContentsObserverImpl(
    HungPagesTableModel* model, WebContents* tab)
    : content::WebContentsObserver(tab),
      model_(model) {
}

void HungPagesTableModel::WebContentsObserverImpl::RenderViewGone(
    base::TerminationStatus status) {
  model_->TabDestroyed(this);
}

void HungPagesTableModel::WebContentsObserverImpl::WebContentsDestroyed(
    WebContents* tab) {
  model_->TabDestroyed(this);
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView

// static
gfx::ImageSkia* HungRendererDialogView::frozen_icon_ = NULL;

// The distance in pixels from the top of the relevant contents to place the
// warning window.
static const int kOverlayContentsOffsetY = 50;

// The dimensions of the hung pages list table view, in pixels.
static const int kTableViewWidth = 300;
static const int kTableViewHeight = 100;

// Padding space in pixels between frozen icon to the info label, hung pages
// list table view and the Kill pages button.
static const int kCentralColumnPadding =
    views::kUnrelatedControlLargeHorizontalSpacing;

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, public:

// static
HungRendererDialogView* HungRendererDialogView::Create() {
  if (!g_instance_) {
    g_instance_ = new HungRendererDialogView;
    views::Widget::CreateWindow(g_instance_);
  }
  return g_instance_;
}

// static
HungRendererDialogView* HungRendererDialogView::GetInstance() {
  return g_instance_;
}

// static
bool HungRendererDialogView::IsFrameActive(WebContents* contents) {
  gfx::NativeView frame_view =
      platform_util::GetTopLevel(contents->GetNativeView());
  return platform_util::IsWindowActive(frame_view);
}

#if !defined(OS_WIN)
// static
void HungRendererDialogView::KillRendererProcess(
    base::ProcessHandle process_handle) {
  base::KillProcess(process_handle, content::RESULT_CODE_HUNG, false);
}
#endif  // OS_WIN


HungRendererDialogView::HungRendererDialogView()
    : hung_pages_table_(NULL),
      kill_button_(NULL),
      initialized_(false) {
  InitClass();
}

HungRendererDialogView::~HungRendererDialogView() {
  hung_pages_table_->SetModel(NULL);
}

void HungRendererDialogView::ShowForWebContents(WebContents* contents) {
  DCHECK(contents && GetWidget());

  // Don't show the warning unless the foreground window is the frame, or this
  // window (but still invisible). If the user has another window or
  // application selected, activating ourselves is rude.
  if (!IsFrameActive(contents) &&
      !platform_util::IsWindowActive(GetWidget()->GetNativeWindow()))
    return;

  if (!GetWidget()->IsActive()) {
    gfx::Rect bounds = GetDisplayBounds(contents);

    gfx::NativeView frame_view =
        platform_util::GetTopLevel(contents->GetNativeView());

    views::Widget* insert_after =
        views::Widget::GetWidgetForNativeView(frame_view);
    GetWidget()->SetBoundsConstrained(bounds);
    if (insert_after)
      GetWidget()->StackAboveWidget(insert_after);

    // We only do this if the window isn't active (i.e. hasn't been shown yet,
    // or is currently shown but deactivated for another WebContents). This is
    // because this window is a singleton, and it's possible another active
    // renderer may hang while this one is showing, and we don't want to reset
    // the list of hung pages for a potentially unrelated renderer while this
    // one is showing.
    hung_pages_table_model_->InitForWebContents(contents);
    GetWidget()->Show();
  }
}

void HungRendererDialogView::EndForWebContents(WebContents* contents) {
  DCHECK(contents);
  if (hung_pages_table_model_->RowCount() == 0 ||
      hung_pages_table_model_->GetRenderProcessHost() ==
      contents->GetRenderProcessHost()) {
    GetWidget()->Close();
    // Close is async, make sure we drop our references to the tab immediately
    // (it may be going away).
    hung_pages_table_model_->InitForWebContents(NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::DialogDelegate implementation:

string16 HungRendererDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_TITLE);
}

void HungRendererDialogView::WindowClosing() {
  // We are going to be deleted soon, so make sure our instance is destroyed.
  g_instance_ = NULL;
}

int HungRendererDialogView::GetDialogButtons() const {
  // We specifically don't want a CANCEL button here because that code path is
  // also called when the window is closed by the user clicking the X button in
  // the window's titlebar, and also if we call Window::Close. Rather, we want
  // the OK button to wait for responsiveness (and close the dialog) and our
  // additional button (which we create) to kill the process (which will result
  // in the dialog being destroyed).
  return ui::DIALOG_BUTTON_OK;
}

string16 HungRendererDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT);
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

views::View* HungRendererDialogView::CreateExtraView() {
  DCHECK(!kill_button_);
  kill_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_END));
  return kill_button_;
}

bool HungRendererDialogView::Accept(bool window_closing) {
  // Don't do anything if we're being called only because the dialog is being
  // destroyed and we don't supply a Cancel function...
  if (window_closing)
    return true;

  // Start waiting again for responsiveness.
  if (hung_pages_table_model_->GetRenderViewHost())
    hung_pages_table_model_->GetRenderViewHost()->RestartHangMonitorTimeout();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::ButtonListener implementation:

void HungRendererDialogView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender == kill_button_ &&
      hung_pages_table_model_->GetRenderProcessHost()) {

    base::ProcessHandle process_handle =
        hung_pages_table_model_->GetRenderProcessHost()->GetHandle();

    KillRendererProcess(process_handle);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, HungPagesTableModel::Delegate overrides:

void HungRendererDialogView::TabDestroyed() {
  GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::View overrides:

void HungRendererDialogView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (!initialized_ && is_add && child == this && GetWidget())
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, private:

void HungRendererDialogView::Init() {
  views::ImageView* frozen_icon_view = new views::ImageView;
  frozen_icon_view->SetImage(frozen_icon_);

  views::Label* info_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER));
  info_label->SetMultiLine(true);
  info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  hung_pages_table_model_.reset(new HungPagesTableModel(this));
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn());
  hung_pages_table_ = new views::TableView(
      hung_pages_table_model_.get(), columns, views::ICON_AND_TEXT, true,
      false, true);
  hung_pages_table_->SetGrouper(hung_pages_table_model_.get());

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int double_column_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(double_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::FIXED, frozen_icon_->width(), 0);
  column_set->AddPaddingColumn(0, kCentralColumnPadding);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_set_id);
  layout->AddView(frozen_icon_view, 1, 3);
  // Add the label with a preferred width of 1, this way it doesn't effect the
  // overall preferred size of the dialog.
  layout->AddView(
      info_label, 1, 1, GridLayout::FILL, GridLayout::LEADING, 1, 0);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(hung_pages_table_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL,
                  views::GridLayout::FILL, kTableViewWidth, kTableViewHeight);

  initialized_ = true;
}

gfx::Rect HungRendererDialogView::GetDisplayBounds(
    WebContents* contents) {
#if defined(USE_AURA)
  gfx::Rect contents_bounds(contents->GetNativeView()->GetBoundsInRootWindow());
#elif defined(OS_WIN)
  HWND contents_hwnd = contents->GetNativeView();
  RECT contents_bounds_rect;
  GetWindowRect(contents_hwnd, &contents_bounds_rect);
  gfx::Rect contents_bounds(contents_bounds_rect);
#endif
  gfx::Rect window_bounds = GetWidget()->GetWindowBoundsInScreen();

  int window_x = contents_bounds.x() +
      (contents_bounds.width() - window_bounds.width()) / 2;
  int window_y = contents_bounds.y() + kOverlayContentsOffsetY;
  return gfx::Rect(window_x, window_y, window_bounds.width(),
                   window_bounds.height());
}

// static
void HungRendererDialogView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    frozen_icon_ = rb.GetImageSkiaNamed(IDR_FROZEN_TAB_ICON);
    initialized = true;
  }
}

namespace chrome {

void ShowHungRendererDialog(WebContents* contents) {
  if (!logging::DialogsAreSuppressed() &&
      !PlatformShowCustomHungRendererDialog(contents)) {
    HungRendererDialogView* view = HungRendererDialogView::Create();
    view->ShowForWebContents(contents);
  }
}

void HideHungRendererDialog(WebContents* contents) {
  if (!logging::DialogsAreSuppressed() &&
      !PlatformHideCustomHungRendererDialog(contents) &&
      HungRendererDialogView::GetInstance())
    HungRendererDialogView::GetInstance()->EndForWebContents(contents);
}

}  // namespace chrome
