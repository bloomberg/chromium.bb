// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <stddef.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using content::OpenURLParams;
using content::Referrer;
using extensions::ExperienceSamplingEvent;

namespace {

// Width of the bullet column in BulletedView.
const int kBulletWidth = 20;

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

// The maximum height of the scroll view before it will show a scrollbar.
const int kScrollViewMaxHeight = 250;

// Width of the left column of the dialog when the extension requests
// permissions.
const int kPermissionsLeftColumnWidth = 250;

// Width of the left column of the dialog when the extension requests no
// permissions.
const int kNoPermissionsLeftColumnWidth = 200;

// Width of the left column for external install prompts. The text is long in
// this case, so make it wider than normal.
const int kExternalInstallLeftColumnWidth = 350;

void AddResourceIcon(const gfx::ImageSkia* skia_image, void* data) {
  views::View* parent = static_cast<views::View*>(data);
  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(*skia_image);
  parent->AddChildView(image_view);
}

// Creates a string for displaying |message| to the user. If it has to look
// like a entry in a bullet point list, one is added.
base::string16 PrepareForDisplay(const base::string16& message,
                                 bool bullet_point) {
  return bullet_point ? l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PERMISSION_LINE,
      message) : message;
}

void ShowExtensionInstallDialogImpl(
    ExtensionInstallPromptShowParams* show_params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool use_tab_modal_dialog = prompt->ShouldUseTabModalDialog();
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      show_params->profile(), show_params->GetParentWebContents(),
      done_callback, std::move(prompt));
  if (use_tab_modal_dialog) {
    content::WebContents* parent_web_contents =
        show_params->GetParentWebContents();
    if (parent_web_contents)
      constrained_window::ShowWebModalDialogViews(dialog, parent_web_contents);
  } else {
    constrained_window::CreateBrowserModalDialogViews(
        dialog, show_params->GetParentWindow())
        ->Show();
  }
}

// A custom scrollable view implementation for the dialog.
class CustomScrollableView : public views::View {
 public:
  CustomScrollableView() {}
  ~CustomScrollableView() override {}

  // Called when one of the child elements has expanded/collapsed.
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  void Layout() override {
    SetBounds(x(), y(), width(), GetHeightForWidth(width()));
    views::View::Layout();
  }

  DISALLOW_COPY_AND_ASSIGN(CustomScrollableView);
};

}  // namespace

BulletedView::BulletedView(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        kBulletWidth,
                        0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,  // No fixed width.
                        0);
  layout->StartRow(0, 0);
  layout->AddView(new views::Label(PrepareForDisplay(base::string16(), true)));
  layout->AddView(view);
}

ExtensionInstallDialogView::ExtensionInstallDialogView(
    Profile* profile,
    content::PageNavigator* navigator,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt)
    : profile_(profile),
      navigator_(navigator),
      done_callback_(done_callback),
      prompt_(std::move(prompt)),
      container_(NULL),
      scroll_view_(NULL),
      handled_result_(false) {
  InitView();
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {
  if (!handled_result_ && !done_callback_.is_null()) {
    base::ResetAndReturn(&done_callback_)
        .Run(ExtensionInstallPrompt::Result::USER_CANCELED);
  }
}

void ExtensionInstallDialogView::InitView() {
  // Possible grid layouts:
  // With webstore data (inline install, external install, repair)
  //      w/ permissions           no permissions
  // +--------------+------+  +--------------+------+
  // | title        | icon |  | title        | icon |
  // +--------------|      |  +--------------|      |
  // | rating       |      |  | rating       |      |
  // +--------------|      |  +--------------|      |
  // | user_count   |      |  | user_count   |      |
  // +--------------|      |  +--------------|      |
  // | store_link   |      |  | store_link   |      |
  // +--------------+------+  +--------------+------+
  // |      separator      |  | scroll_view (empty) |
  // +---------------------+  +---------------------+
  // | scroll_view         |
  // +---------------------+
  //
  // No webstore data (all other types)
  //      w/ permissions           no permissions
  // +--------------+------+  +--------------+------+
  // | title        | icon |  | title        | icon |
  // +--------------+------+  +--------------+------+
  // |     separator       |  | scroll_view (empty) |
  // +---------------------+  +---------------------+
  // | scroll_view         |
  // +---------------------+

  // The scroll_view contains permissions (if there are any) and retained
  // files/devices (if there are any; post-install-permissions prompt only).
  int left_column_width =
      (prompt_->ShouldShowPermissions() || prompt_->GetRetainedFileCount() > 0)
          ? kPermissionsLeftColumnWidth
          : kNoPermissionsLeftColumnWidth;
  if (is_external_install())
    left_column_width = kExternalInstallLeftColumnWidth;

  int column_set_id = 0;
  views::GridLayout* layout = CreateLayout(left_column_width, column_set_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  if (prompt_->has_webstore_data()) {
    layout->StartRow(0, column_set_id);
    views::View* rating = new views::View();
    rating->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    layout->AddView(rating);
    prompt_->AppendRatingStars(AddResourceIcon, rating);

    const gfx::FontList& small_font_list =
        rb.GetFontList(ui::ResourceBundle::SmallFont);
    views::Label* rating_count =
        new views::Label(prompt_->GetRatingCount(), small_font_list);
    // Add some space between the stars and the rating count.
    rating_count->SetBorder(views::CreateEmptyBorder(0, 2, 0, 0));
    rating->AddChildView(rating_count);

    layout->StartRow(0, column_set_id);
    views::Label* user_count =
        new views::Label(prompt_->GetUserCount(), small_font_list);
    user_count->SetAutoColorReadabilityEnabled(false);
    user_count->SetEnabledColor(SK_ColorGRAY);
    layout->AddView(user_count);
  }

  if (prompt_->ShouldShowPermissions()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, column_set_id);
    layout->AddView(new views::Separator(), 3, 1, views::GridLayout::FILL,
                    views::GridLayout::FILL);
  }

  const int content_width = left_column_width +
                            LayoutDelegate::Get()->GetMetric(
                                LayoutDelegate::Metric::PANEL_CONTENT_MARGIN) +
                            kIconSize;

  // Create the scrollable view which will contain the permissions and retained
  // files/devices. It will span the full content width.
  CustomScrollableView* scrollable = new CustomScrollableView();
  views::GridLayout* scroll_layout = new views::GridLayout(scrollable);
  scrollable->SetLayoutManager(scroll_layout);

  views::ColumnSet* scrollable_column_set =
      scroll_layout->AddColumnSet(column_set_id);

  scrollable_column_set->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0,  // no resizing
      views::GridLayout::USE_PREF, content_width, content_width);

  // Pad to the very right of the dialog, so the scrollbar will be on the edge.
  scrollable_column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  layout->StartRow(0, column_set_id);
  scroll_view_ = new views::ScrollView();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  scroll_view_->SetContents(scrollable);
  layout->AddView(scroll_view_, 4, 1);

  if (prompt_->ShouldShowPermissions()) {
    bool has_permissions =
        prompt_->GetPermissionCount(
            ExtensionInstallPrompt::PermissionsType::ALL_PERMISSIONS) > 0;
    if (has_permissions) {
      AddPermissions(
          scroll_layout, rb, column_set_id, content_width,
          ExtensionInstallPrompt::PermissionsType::REGULAR_PERMISSIONS);
      AddPermissions(
          scroll_layout, rb, column_set_id, content_width,
          ExtensionInstallPrompt::PermissionsType::WITHHELD_PERMISSIONS);
    } else {
      scroll_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      scroll_layout->StartRow(0, column_set_id);
      views::Label* permission_label = new views::Label(
          l10n_util::GetStringUTF16(IDS_EXTENSION_NO_SPECIAL_PERMISSIONS));
      permission_label->SetMultiLine(true);
      permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      permission_label->SizeToFit(content_width);
      scroll_layout->AddView(permission_label);
    }
  }

  if (prompt_->GetRetainedFileCount()) {
    scroll_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    scroll_layout->StartRow(0, column_set_id);
    views::Label* retained_files_header =
        new views::Label(prompt_->GetRetainedFilesHeading());
    retained_files_header->SetMultiLine(true);
    retained_files_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    retained_files_header->SizeToFit(content_width);
    scroll_layout->AddView(retained_files_header);

    scroll_layout->StartRow(0, column_set_id);
    PermissionDetails details;
    for (size_t i = 0; i < prompt_->GetRetainedFileCount(); ++i) {
      details.push_back(prompt_->GetRetainedFile(i));
    }
    ExpandableContainerView* issue_advice_view =
        new ExpandableContainerView(details, content_width, false);
    scroll_layout->AddView(issue_advice_view);
  }

  if (prompt_->GetRetainedDeviceCount()) {
    scroll_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    scroll_layout->StartRow(0, column_set_id);
    views::Label* retained_devices_header =
        new views::Label(prompt_->GetRetainedDevicesHeading());
    retained_devices_header->SetMultiLine(true);
    retained_devices_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    retained_devices_header->SizeToFit(content_width);
    scroll_layout->AddView(retained_devices_header);

    scroll_layout->StartRow(0, column_set_id);
    PermissionDetails details;
    for (size_t i = 0; i < prompt_->GetRetainedDeviceCount(); ++i) {
      details.push_back(prompt_->GetRetainedDeviceMessageString(i));
    }
    ExpandableContainerView* issue_advice_view =
        new ExpandableContainerView(details, content_width, false);
    scroll_layout->AddView(issue_advice_view);
  }

  DCHECK_GE(prompt_->type(), 0);
  UMA_HISTOGRAM_ENUMERATION("Extensions.InstallPrompt.Type",
                            prompt_->type(),
                            ExtensionInstallPrompt::NUM_PROMPT_TYPES);

  scroll_view_->ClipHeightTo(
      0,
      std::min(kScrollViewMaxHeight, scrollable->GetPreferredSize().height()));

  dialog_size_ = gfx::Size(
      content_width + 2 * views::kButtonHEdgeMarginNew,
      container_->GetPreferredSize().height());

  std::string event_name = ExperienceSamplingEvent::kExtensionInstallDialog;
  event_name.append(
      ExtensionInstallPrompt::PromptTypeToString(prompt_->type()));
  sampling_event_ = ExperienceSamplingEvent::Create(event_name);
}

bool ExtensionInstallDialogView::AddPermissions(
    views::GridLayout* layout,
    ui::ResourceBundle& rb,
    int column_set_id,
    int left_column_width,
    ExtensionInstallPrompt::PermissionsType perm_type) {
  if (prompt_->GetPermissionCount(perm_type) == 0)
    return false;

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_set_id);
  views::Label* permissions_header =
      new views::Label(prompt_->GetPermissionsHeading(perm_type));
  permissions_header->SetMultiLine(true);
  permissions_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  permissions_header->SizeToFit(left_column_width);
  layout->AddView(permissions_header);

  for (size_t i = 0; i < prompt_->GetPermissionCount(perm_type); ++i) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, column_set_id);
    views::Label* permission_label =
        new views::Label(prompt_->GetPermission(i, perm_type));

    permission_label->SetMultiLine(true);
    permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    permission_label->SizeToFit(left_column_width - kBulletWidth);
    layout->AddView(new BulletedView(permission_label));

    // If we have more details to provide, show them in collapsed form.
    if (!prompt_->GetPermissionsDetails(i, perm_type).empty()) {
      layout->StartRow(0, column_set_id);
      PermissionDetails details;
      details.push_back(PrepareForDisplay(
          prompt_->GetPermissionsDetails(i, perm_type), false));
      ExpandableContainerView* details_container =
          new ExpandableContainerView(details, left_column_width, true);
      layout->AddView(details_container);
    }
  }
  return true;
}

views::GridLayout* ExtensionInstallDialogView::CreateLayout(
    int left_column_width,
    int column_set_id) {
  container_ = new views::View();
  // This is basically views::GridLayout::CreatePanel, but without a top or
  // right margin (we effectively get a top margin anyway from the empty dialog
  // title, and we add an explicit padding column as a right margin below).
  views::GridLayout* layout = new views::GridLayout(container_);
  layout->SetInsets(0, views::kButtonHEdgeMarginNew, views::kPanelVertMargin,
                    0);
  container_->SetLayoutManager(layout);
  AddChildView(container_);

  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        0,  // no resizing
                        views::GridLayout::USE_PREF,
                        0,  // no fixed width
                        left_column_width);
  column_set->AddPaddingColumn(
      0, LayoutDelegate::Get()->GetMetric(
             LayoutDelegate::Metric::PANEL_CONTENT_MARGIN));
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                        0,  // no resizing
                        views::GridLayout::USE_PREF,
                        0,  // no fixed width
                        kIconSize);
  column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  layout->StartRow(0, column_set_id);
  views::Label* title =
      new views::Label(prompt_->GetDialogTitle(),
                       ui::ResourceBundle::GetSharedInstance().GetFontList(
                           ui::ResourceBundle::MediumFont));
  title->SetMultiLine(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SizeToFit(left_column_width);

  // Center align the title along the vertical axis.
  layout->AddView(title, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  // Scale down to icon size, but allow smaller icons (don't scale up).
  const gfx::ImageSkia* image = prompt_->icon().ToImageSkia();
  gfx::Size size(image->width(), image->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  views::ImageView* icon = new views::ImageView();
  icon->SetImageSize(size);
  icon->SetImage(*image);

  // Span the title row. In case the webstore data is available, also span the
  // rating, user_count and store_link rows.
  const int icon_row_span = prompt_->has_webstore_data() ? 4 : 1;
  layout->AddView(icon, 1, icon_row_span);

  return layout;
}

void ExtensionInstallDialogView::OnNativeThemeChanged(
    const ui::NativeTheme* theme) {
  scroll_view_->SetBackgroundColor(
      theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground));
}

int ExtensionInstallDialogView::GetDialogButtons() const {
  int buttons = prompt_->GetDialogButtons();
  // Simply having just an OK button is *not* supported. See comment on function
  // GetDialogButtons in dialog_delegate.h for reasons.
  DCHECK_GT(buttons & ui::DIALOG_BUTTON_CANCEL, 0);
  return buttons;
}

base::string16 ExtensionInstallDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return prompt_->GetAcceptButtonLabel();
    case ui::DIALOG_BUTTON_CANCEL:
      return prompt_->GetAbortButtonLabel();
    default:
      NOTREACHED();
      return base::string16();
  }
}

int ExtensionInstallDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

bool ExtensionInstallDialogView::Cancel() {
  if (handled_result_)
    return true;

  handled_result_ = true;
  UpdateInstallResultHistogram(false);
  if (sampling_event_)
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  base::ResetAndReturn(&done_callback_)
      .Run(ExtensionInstallPrompt::Result::USER_CANCELED);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
  DCHECK(!handled_result_);

  handled_result_ = true;
  UpdateInstallResultHistogram(true);
  if (sampling_event_)
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
  base::ResetAndReturn(&done_callback_)
      .Run(ExtensionInstallPrompt::Result::ACCEPTED);
  return true;
}

ui::ModalType ExtensionInstallDialogView::GetModalType() const {
  return prompt_->ShouldUseTabModalDialog() ? ui::MODAL_TYPE_CHILD
                                            : ui::MODAL_TYPE_WINDOW;
}

void ExtensionInstallDialogView::LinkClicked(views::Link* source,
                                             int event_flags) {
  GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                 prompt_->extension()->id());
  OpenURLParams params(store_url, Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);

  if (navigator_) {
    navigator_->OpenURL(params);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    displayer.browser()->OpenURL(params);
  }
  GetWidget()->Close();
}

void ExtensionInstallDialogView::Layout() {
  container_->SetBounds(0, 0, width(), height());
  DialogDelegateView::Layout();
}

gfx::Size ExtensionInstallDialogView::GetPreferredSize() const {
  return dialog_size_;
}

views::View* ExtensionInstallDialogView::CreateExtraView() {
  if (!prompt_->has_webstore_data())
    return nullptr;

  views::Link* store_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK));
  store_link->set_listener(this);
  return store_link;
}

void ExtensionInstallDialogView::UpdateInstallResultHistogram(bool accepted)
    const {
  if (prompt_->type() == ExtensionInstallPrompt::INSTALL_PROMPT)
    UMA_HISTOGRAM_BOOLEAN("Extensions.InstallPrompt.Accepted", accepted);
}

// ExpandableContainerView::DetailsView ----------------------------------------

ExpandableContainerView::DetailsView::DetailsView(int horizontal_space,
                                                  bool parent_bulleted)
    : layout_(new views::GridLayout(this)),
      state_(0) {
  SetLayoutManager(layout_);
  views::ColumnSet* column_set = layout_->AddColumnSet(0);
  // If the parent is using bullets for its items, then a padding of one unit
  // will make the child item (which has no bullet) look like a sibling of its
  // parent. Therefore increase the indentation by one more unit to show that it
  // is in fact a child item (with no missing bullet) and not a sibling.
  int padding =
      views::kRelatedControlHorizontalSpacing * (parent_bulleted ? 2 : 1);
  column_set->AddPaddingColumn(0, padding);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        horizontal_space - padding,
                        0);
}

void ExpandableContainerView::DetailsView::AddDetail(
    const base::string16& detail) {
  layout_->StartRowWithPadding(0, 0,
                               0, views::kRelatedControlSmallVerticalSpacing);
  views::Label* detail_label =
      new views::Label(PrepareForDisplay(detail, false));
  detail_label->SetMultiLine(true);
  detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout_->AddView(detail_label);
}

gfx::Size ExpandableContainerView::DetailsView::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), size.height() * state_);
}

void ExpandableContainerView::DetailsView::AnimateToState(double state) {
  state_ = state;
  PreferredSizeChanged();
  SchedulePaint();
}

// ExpandableContainerView -----------------------------------------------------

ExpandableContainerView::ExpandableContainerView(
    const PermissionDetails& details,
    int horizontal_space,
    bool parent_bulleted)
    : details_view_(NULL),
      slide_animation_(this),
      more_details_(NULL),
      arrow_toggle_(NULL),
      expanded_(false) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);

  if (details.empty())
    return;

  details_view_ = new DetailsView(horizontal_space, parent_bulleted);

  layout->StartRow(0, column_set_id);
  layout->AddView(details_view_);

  for (size_t i = 0; i < details.size(); ++i)
    details_view_->AddDetail(details[i]);

  // Make sure the link width column is as wide as needed for both Show and
  // Hide details, so that the arrow doesn't shift horizontally when we
  // toggle.
  views::Link* link = new views::Link(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS));
  int link_col_width = link->GetPreferredSize().width();
  link->SetText(l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  link_col_width = std::max(link_col_width, link->GetPreferredSize().width());

  column_set = layout->AddColumnSet(++column_set_id);
  // Padding to the left of the More Details column. If the parent is using
  // bullets for its items, then a padding of one unit will make the child
  // item (which has no bullet) look like a sibling of its parent. Therefore
  // increase the indentation by one more unit to show that it is in fact a
  // child item (with no missing bullet) and not a sibling.
  column_set->AddPaddingColumn(
      0, views::kRelatedControlHorizontalSpacing * (parent_bulleted ? 2 : 1));
  // The More Details column.
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        link_col_width,
                        link_col_width);
  // The Up/Down arrow column.
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::TRAILING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  // Add the More Details link.
  layout->StartRow(0, column_set_id);
  more_details_ = link;
  more_details_->set_listener(this);
  more_details_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(more_details_);

  // Add the arrow after the More Details link.
  arrow_toggle_ = new views::ImageButton(this);
  UpdateArrowToggle(false);
  layout->AddView(arrow_toggle_);
}

ExpandableContainerView::~ExpandableContainerView() {
}

void ExpandableContainerView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  ToggleDetailLevel();
}

void ExpandableContainerView::LinkClicked(
    views::Link* source, int event_flags) {
  ToggleDetailLevel();
}

void ExpandableContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(&slide_animation_, animation);
  if (details_view_)
    details_view_->AnimateToState(animation->GetCurrentValue());
}

void ExpandableContainerView::AnimationEnded(const gfx::Animation* animation) {
  if (arrow_toggle_)
    UpdateArrowToggle(animation->GetCurrentValue() != 0.0);
  if (more_details_) {
    more_details_->SetText(expanded_ ?
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS) :
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  }
}

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ExpandableContainerView::ToggleDetailLevel() {
  expanded_ = !expanded_;

  if (slide_animation_.IsShowing())
    slide_animation_.Hide();
  else
    slide_animation_.Show();
}

void ExpandableContainerView::UpdateArrowToggle(bool expanded) {
  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      expanded ? kCaretUpIcon : kCaretDownIcon, gfx::kChromeIconGrey);
  arrow_toggle_->SetImage(views::Button::STATE_NORMAL, &icon);
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetViewsShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
