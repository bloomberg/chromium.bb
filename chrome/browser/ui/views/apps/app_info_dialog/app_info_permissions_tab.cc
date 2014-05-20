// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_tab.h"

#include "apps/app_load_service.h"
#include "apps/app_restore_service.h"
#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// A view to display a title with an expandable permissions list section and an
// optional 'revoke' button below the list.
class ExpandableContainerView : public views::View,
                                public views::ButtonListener,
                                public gfx::AnimationDelegate {
 public:
  ExpandableContainerView(
      views::View* owner,
      const base::string16& title,
      const std::vector<base::string16>& permission_messages,
      views::Button* button);
  virtual ~ExpandableContainerView();

  // views::View:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  // Expand/Collapse the detail section for this ExpandableContainerView.
  void ToggleDetailLevel();

 private:
  // A view which displays the permission messages as a bulleted list, with an
  // optional button underneath (such as to revoke the permissions). |button|
  // may be NULL.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(const std::vector<base::string16>& messages,
                         views::Button* button);
    virtual ~DetailsView() {}

    // views::View:
    virtual gfx::Size GetPreferredSize() const OVERRIDE;

    // Animates this to be a height proportional to |ratio|.
    void AnimateToRatio(double ratio);

   private:
    // The current state of the animation, as a decimal from 0 to 1 (0 is fully
    // collapsed, 1 is fully expanded).
    double visible_ratio_;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // The dialog that owns |this|. It's also an ancestor in the View hierarchy.
  views::View* owner_;

  // A view for showing |permission_messages|.
  DetailsView* details_view_;

  gfx::SlideAnimation slide_animation_;

  // The up/down arrow next to the heading (points up/down depending on whether
  // the details section is expanded).
  views::ImageButton* arrow_toggle_;

  // Whether the details section is expanded.
  bool expanded_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableContainerView);
};

ExpandableContainerView::DetailsView::DetailsView(
    const std::vector<base::string16>& messages,
    views::Button* button)
    : visible_ratio_(0) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Create 2 columns: one for the bullet, one for the bullet text. Also inset
  // the whole bulleted list by kPanelHorizMargin on either side.
  static const int kColumnSet = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSet);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, 5);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);

  // Create a right-aligned column (just for the button at the bottom) that
  // aligns all the way to the right of the view.
  static const int kButtonColumnSet = 2;
  views::ColumnSet* button_column_set = layout->AddColumnSet(kButtonColumnSet);
  button_column_set->AddColumn(views::GridLayout::TRAILING,
                               views::GridLayout::LEADING,
                               1,
                               views::GridLayout::USE_PREF,
                               0,
                               0);

  // Add padding above the permissions.
  layout->AddPaddingRow(0, views::kPanelVertMargin);

  // Add the permissions.
  for (std::vector<base::string16>::const_iterator it = messages.begin();
       it != messages.end();
       ++it) {
    views::Label* permission_label = new views::Label(*it);

    permission_label->SetMultiLine(true);
    permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Add a row of padding before every item except the first.
    if (it != messages.begin()) {
      layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    }

    layout->StartRow(0, kColumnSet);
    // Extract only the bullet from the IDS_EXTENSION_PERMISSION_LINE text.
    layout->AddView(new views::Label(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PERMISSION_LINE, base::string16())));
    // Place the text second, so multi-lined permissions line up below the
    // bullet.
    layout->AddView(permission_label);
  }

  // Add the button, if one was provided.
  if (button) {
    layout->AddPaddingRow(0, views::kUnrelatedControlHorizontalSpacing);
    layout->StartRow(0, kButtonColumnSet);
    layout->AddView(button);
  }

  // Add the bottom padding.
  layout->AddPaddingRow(0, views::kPanelVertMargin);
}

gfx::Size ExpandableContainerView::DetailsView::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), size.height() * visible_ratio_);
}

void ExpandableContainerView::DetailsView::AnimateToRatio(double ratio) {
  visible_ratio_ = ratio;
  PreferredSizeChanged();
  SchedulePaint();
}

ExpandableContainerView::ExpandableContainerView(
    views::View* owner,
    const base::string16& title,
    const std::vector<base::string16>& permission_messages,
    views::Button* button)
    : owner_(owner),
      details_view_(NULL),
      slide_animation_(this),
      arrow_toggle_(NULL),
      expanded_(false) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int kMainColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kMainColumnSetId);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  // A column set that is split in half, to allow for the expand/collapse button
  // image to be aligned to the right of the view.
  const int kSplitColumnSetId = 1;
  views::ColumnSet* split_column_set = layout->AddColumnSet(kSplitColumnSetId);
  split_column_set->AddColumn(views::GridLayout::LEADING,
                              views::GridLayout::LEADING,
                              1,
                              views::GridLayout::USE_PREF,
                              0,
                              0);
  split_column_set->AddPaddingColumn(0,
                                     views::kRelatedControlHorizontalSpacing);
  split_column_set->AddColumn(views::GridLayout::TRAILING,
                              views::GridLayout::LEADING,
                              1,
                              views::GridLayout::USE_PREF,
                              0,
                              0);

  // To display the heading and count next to each other, create a sub-view
  // with a box layout that stacks them horizontally.
  views::View* title_view = new views::View();
  title_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0,
                           0,
                           views::kRelatedControlSmallHorizontalSpacing));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Format the title as 'Title - number'. Unfortunately, this needs to be in
  // separate views because labels only support a single font per view.
  title_view->AddChildView(
      new views::Label(title, rb.GetFontList(ui::ResourceBundle::BoldFont)));
  title_view->AddChildView(
      new views::Label(base::UTF8ToUTF16("\xe2\x80\x93")));  // En-dash.
  title_view->AddChildView(
      new views::Label(base::IntToString16(permission_messages.size())));

  arrow_toggle_ = new views::ImageButton(this);
  arrow_toggle_->SetImage(views::Button::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_DOWN_ARROW));

  layout->StartRow(0, kSplitColumnSetId);
  layout->AddView(title_view);
  layout->AddView(arrow_toggle_);

  details_view_ = new DetailsView(permission_messages, button);
  layout->StartRow(0, kMainColumnSetId);
  layout->AddView(details_view_);
}

ExpandableContainerView::~ExpandableContainerView() {
}

void ExpandableContainerView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  ToggleDetailLevel();
}

void ExpandableContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(&slide_animation_, animation);
  if (details_view_) {
    details_view_->AnimateToRatio(animation->GetCurrentValue());
  }
}

void ExpandableContainerView::AnimationEnded(const gfx::Animation* animation) {
  if (arrow_toggle_) {
    if (animation->GetCurrentValue() != 0.0) {
      arrow_toggle_->SetImage(
          views::Button::STATE_NORMAL,
          ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_UP_ARROW));
    } else {
      arrow_toggle_->SetImage(
          views::Button::STATE_NORMAL,
          ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_DOWN_ARROW));
    }
  }
}

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  owner_->Layout();
}

void ExpandableContainerView::ToggleDetailLevel() {
  expanded_ = !expanded_;

  if (slide_animation_.IsShowing())
    slide_animation_.Hide();
  else
    slide_animation_.Show();
}

}  // namespace

AppInfoPermissionsTab::AppInfoPermissionsTab(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback),
      revoke_file_permissions_button_(NULL) {
  this->SetLayoutManager(new views::FillLayout);

  // Create a scrollview and add it to the tab.
  views::View* scrollable_content = new views::View();
  scroll_view_ = new views::ScrollView();
  scroll_view_->SetContents(scrollable_content);
  AddChildView(scroll_view_);

  // Give the inner scrollview (the 'scrollable' part) a layout.
  views::GridLayout* layout =
      views::GridLayout::CreatePanel(scrollable_content);
  scrollable_content->SetLayoutManager(layout);

  // Main column that stretches to the width of the view.
  static const int kMainColumnSetId = 0;
  views::ColumnSet* main_column_set = layout->AddColumnSet(kMainColumnSetId);
  main_column_set->AddColumn(
      views::GridLayout::FILL,
      views::GridLayout::FILL,
      1,  // This column resizes to the width of the dialog.
      views::GridLayout::USE_PREF,
      0,
      0);

  const std::vector<base::string16> required_permission_messages =
      GetRequiredPermissionMessages();
  const std::vector<base::string16> optional_permission_messages =
      GetOptionalPermissionMessages();
  const std::vector<base::string16> retained_file_permission_messages =
      GetRetainedFilePermissionMessages();

  if (required_permission_messages.empty() &&
      optional_permission_messages.empty() &&
      retained_file_permission_messages.empty()) {
    // If there are no permissions at all, display an appropriate message.
    views::Label* no_permissions_text = new views::Label(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_NO_PERMISSIONS_TEXT));
    no_permissions_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    layout->StartRow(0, kMainColumnSetId);
    layout->AddView(no_permissions_text);
  } else {
    if (!required_permission_messages.empty()) {
      ExpandableContainerView* details_container = new ExpandableContainerView(
          this,
          l10n_util::GetStringUTF16(
              IDS_APPLICATION_INFO_REQUIRED_PERMISSIONS_TEXT),
          required_permission_messages,
          NULL);
      // Required permissions are visible by default.
      details_container->ToggleDetailLevel();

      layout->StartRow(0, kMainColumnSetId);
      layout->AddView(details_container);
      layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
    }

    if (!optional_permission_messages.empty()) {
      ExpandableContainerView* details_container = new ExpandableContainerView(
          this,
          l10n_util::GetStringUTF16(
              IDS_APPLICATION_INFO_OPTIONAL_PERMISSIONS_TEXT),
          optional_permission_messages,
          NULL);

      layout->StartRow(0, kMainColumnSetId);
      layout->AddView(details_container);
      layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
    }

    if (!retained_file_permission_messages.empty()) {
      revoke_file_permissions_button_ = new views::LabelButton(
          this,
          l10n_util::GetStringUTF16(
              IDS_APPLICATION_INFO_REVOKE_RETAINED_FILE_PERMISSIONS_BUTTON_TEXT));
      revoke_file_permissions_button_->SetStyle(views::Button::STYLE_BUTTON);

      ExpandableContainerView* details_container = new ExpandableContainerView(
          this,
          l10n_util::GetStringUTF16(
              IDS_APPLICATION_INFO_RETAINED_FILE_PERMISSIONS_TEXT),
          retained_file_permission_messages,
          revoke_file_permissions_button_);

      layout->StartRow(0, kMainColumnSetId);
      layout->AddView(details_container);
      layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
    }
  }
}

AppInfoPermissionsTab::~AppInfoPermissionsTab() {
}

void AppInfoPermissionsTab::Layout() {
  // To avoid 'jumping' issues when the scrollbar becomes visible, size the
  // scrollable area as though it always has a visible scrollbar.
  views::View* contents_view = scroll_view_->contents();
  int content_width = width() - scroll_view_->GetScrollBarWidth();
  int content_height = contents_view->GetHeightForWidth(content_width);
  contents_view->SetBounds(0, 0, content_width, content_height);
  scroll_view_->SetBounds(0, 0, width(), height());
}

void AppInfoPermissionsTab::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == revoke_file_permissions_button_)
    RevokeFilePermissions();
  else
    NOTREACHED();
}

void AppInfoPermissionsTab::RevokeFilePermissions() {
  apps::SavedFilesService::Get(profile_)->ClearQueue(app_);

  // TODO(benwells): Fix this to call something like
  // AppLoadService::RestartApplicationIfRunning.
  if (apps::AppRestoreService::Get(profile_)->IsAppRestorable(app_->id()))
    apps::AppLoadService::Get(profile_)->RestartApplication(app_->id());

  GetWidget()->Close();
}

const extensions::PermissionSet* AppInfoPermissionsTab::GetRequiredPermissions()
    const {
  return extensions::PermissionsData::GetRequiredPermissions(app_);
}

const std::vector<base::string16>
AppInfoPermissionsTab::GetRequiredPermissionMessages() const {
  return extensions::PermissionMessageProvider::Get()->GetWarningMessages(
      GetRequiredPermissions(), app_->GetType());
}

const extensions::PermissionSet* AppInfoPermissionsTab::GetOptionalPermissions()
    const {
  return extensions::PermissionsData::GetOptionalPermissions(app_);
}

const std::vector<base::string16>
AppInfoPermissionsTab::GetOptionalPermissionMessages() const {
  return extensions::PermissionMessageProvider::Get()->GetWarningMessages(
      GetOptionalPermissions(), app_->GetType());
}

const std::vector<base::FilePath>
AppInfoPermissionsTab::GetRetainedFilePermissions() const {
  std::vector<base::FilePath> retained_file_paths;
  if (app_->HasAPIPermission(extensions::APIPermission::kFileSystem)) {
    std::vector<apps::SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(profile_)->GetAllFileEntries(app_->id());
    for (std::vector<apps::SavedFileEntry>::const_iterator it =
             retained_file_entries.begin();
         it != retained_file_entries.end();
         ++it) {
      retained_file_paths.push_back(it->path);
    }
  }
  return retained_file_paths;
}

const std::vector<base::string16>
AppInfoPermissionsTab::GetRetainedFilePermissionMessages() const {
  const std::vector<base::FilePath> permissions = GetRetainedFilePermissions();
  std::vector<base::string16> file_permission_messages;
  for (std::vector<base::FilePath>::const_iterator it = permissions.begin();
       it != permissions.end();
       ++it) {
    file_permission_messages.push_back(it->LossyDisplayName());
  }
  return file_permission_messages;
}
