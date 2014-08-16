// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_prompt_experiment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

using content::OpenURLParams;
using content::Referrer;
using extensions::BundleInstaller;
using extensions::ExperienceSamplingEvent;

namespace {

// Width of the bullet column in BulletedView.
const int kBulletWidth = 20;

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

// We offset the icon a little bit from the right edge of the dialog, to make it
// align with the button below it.
const int kIconOffset = 16;

// The dialog will resize based on its content, but this sets a maximum height
// before overflowing a scrollbar.
const int kDialogMaxHeight = 300;

// Width of the left column of the dialog when the extension requests
// permissions.
const int kPermissionsLeftColumnWidth = 250;

// Width of the left column of the dialog when the extension requests no
// permissions.
const int kNoPermissionsLeftColumnWidth = 200;

// Width of the left column for bundle install prompts. There's only one column
// in this case, so make it wider than normal.
const int kBundleLeftColumnWidth = 300;

// Width of the left column for external install prompts. The text is long in
// this case, so make it wider than normal.
const int kExternalInstallLeftColumnWidth = 350;

// Lighter color for labels.
const SkColor kLighterLabelColor = SkColorSetRGB(0x99, 0x99, 0x99);

// Represents an action on a clickable link created by the install prompt
// experiment. This is used to group the actions in UMA histograms named
// Extensions.InstallPromptExperiment.ShowDetails and
// Extensions.InstallPromptExperiment.ShowPermissions.
enum ExperimentLinkAction {
  LINK_SHOWN = 0,
  LINK_NOT_SHOWN,
  LINK_CLICKED,
  NUM_LINK_ACTIONS
};

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

CheckboxedView::CheckboxedView(views::View* view,
                               views::ButtonListener* listener) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,  // No fixed width.
                        0);
   column_set->AddColumn(views::GridLayout::LEADING,
                         views::GridLayout::LEADING,
                         0,
                         views::GridLayout::USE_PREF,
                         0,  // No fixed width.
                         0);
  layout->StartRow(0, 0);
  views::Checkbox* checkbox = new views::Checkbox(base::string16());
  checkbox->set_listener(listener);
  // Alignment needs to be explicitly set again here, otherwise the views are
  // not vertically centered.
  layout->AddView(checkbox, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::CENTER);
  layout->AddView(view, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::CENTER);
}

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CreateBrowserModalDialogViews(
      new ExtensionInstallDialogView(show_params.navigator, delegate, prompt),
      show_params.parent_window)->Show();
}

CustomScrollableView::CustomScrollableView() {}
CustomScrollableView::~CustomScrollableView() {}

void CustomScrollableView::Layout() {
  SetBounds(x(), y(), width(), GetHeightForWidth(width()));
  views::View::Layout();
}

ExtensionInstallDialogView::ExtensionInstallDialogView(
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt)
    : navigator_(navigator),
      delegate_(delegate),
      prompt_(prompt),
      scroll_view_(NULL),
      scrollable_(NULL),
      scrollable_header_only_(NULL),
      show_details_link_(NULL),
      checkbox_info_label_(NULL),
      unchecked_boxes_(0) {
  // Possible grid layouts without ExtensionPermissionDialog experiment:
  // Inline install
  //      w/ permissions                 no permissions
  // +--------------------+------+  +--------------+------+
  // | heading            | icon |  | heading      | icon |
  // +--------------------|      |  +--------------|      |
  // | rating             |      |  | rating       |      |
  // +--------------------|      |  +--------------+      |
  // | user_count         |      |  | user_count   |      |
  // +--------------------|      |  +--------------|      |
  // | store_link         |      |  | store_link   |      |
  // +--------------------+------+  +--------------+------+
  // |      separator            |
  // +--------------------+------+
  // | permissions_header |      |
  // +--------------------+------+
  // | permission1        |      |
  // +--------------------+------+
  // | permission2        |      |
  // +--------------------+------+
  //
  // Regular install
  // w/ permissions                     no permissions
  // +--------------------+------+  +--------------+------+
  // | heading            | icon |  | heading      | icon |
  // +--------------------|      |  +--------------+------+
  // | permissions_header |      |
  // +--------------------|      |
  // | permission1        |      |
  // +--------------------|      |
  // | permission2        |      |
  // +--------------------+------+
  //
  // If the ExtensionPermissionDialog is on, the layout is modified depending
  // on the experiment group. For text only experiment, a footer is added at the
  // bottom of the layouts. For others, inline details are added below some of
  // the permissions.
  //
  // Regular install w/ permissions and footer (experiment):
  // +--------------------+------+
  // | heading            | icon |
  // +--------------------|      |
  // | permissions_header |      |
  // +--------------------|      |
  // | permission1        |      |
  // +--------------------|      |
  // | permission2        |      |
  // +--------------------+------+
  // | footer text        |      |
  // +--------------------+------+
  //
  // Regular install w/ permissions and inline explanations (experiment):
  // +--------------------+------+
  // | heading            | icon |
  // +--------------------|      |
  // | permissions_header |      |
  // +--------------------|      |
  // | permission1        |      |
  // +--------------------|      |
  // | explanation1       |      |
  // +--------------------|      |
  // | permission2        |      |
  // +--------------------|      |
  // | explanation2       |      |
  // +--------------------+------+
  //
  // Regular install w/ permissions and inline explanations (experiment):
  // +--------------------+------+
  // | heading            | icon |
  // +--------------------|      |
  // | permissions_header |      |
  // +--------------------|      |
  // |checkbox|permission1|      |
  // +--------------------|      |
  // |checkbox|permission2|      |
  // +--------------------+------+
  //
  // Additionally, links or informational text is added to non-client areas of
  // the dialog depending on the experiment group.

  int left_column_width =
      (prompt->ShouldShowPermissions() + prompt->GetRetainedFileCount()) > 0
          ? kPermissionsLeftColumnWidth
          : kNoPermissionsLeftColumnWidth;
  if (is_bundle_install())
    left_column_width = kBundleLeftColumnWidth;
  if (is_external_install())
    left_column_width = kExternalInstallLeftColumnWidth;

  scroll_view_ = new views::ScrollView();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  AddChildView(scroll_view_);

  int column_set_id = 0;
  // Create the full scrollable view which will contain all the information
  // including the permissions.
  scrollable_ = new CustomScrollableView();
  views::GridLayout* layout = CreateLayout(
      scrollable_, left_column_width, column_set_id, false);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  if (prompt->ShouldShowPermissions() &&
      prompt->experiment()->should_show_expandable_permission_list()) {
    // If the experiment should hide the permission list initially, create a
    // simple layout that contains only the header, extension name and icon.
    scrollable_header_only_ = new CustomScrollableView();
    CreateLayout(scrollable_header_only_, left_column_width,
                 column_set_id, true);
    scroll_view_->SetContents(scrollable_header_only_);
  } else {
    scroll_view_->SetContents(scrollable_);
  }

  int dialog_width = left_column_width + 2 * views::kPanelHorizMargin;
  if (!is_bundle_install())
    dialog_width += views::kPanelHorizMargin + kIconSize + kIconOffset;

  // Widen the dialog for experiment with checkboxes so that the information
  // label fits the area to the left of the buttons.
  if (prompt->experiment()->show_checkboxes())
    dialog_width += 4 * views::kPanelHorizMargin;

  if (prompt->has_webstore_data()) {
    layout->StartRow(0, column_set_id);
    views::View* rating = new views::View();
    rating->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    layout->AddView(rating);
    prompt->AppendRatingStars(AddResourceIcon, rating);

    const gfx::FontList& small_font_list =
        rb.GetFontList(ui::ResourceBundle::SmallFont);
    views::Label* rating_count =
        new views::Label(prompt->GetRatingCount(), small_font_list);
    // Add some space between the stars and the rating count.
    rating_count->SetBorder(views::Border::CreateEmptyBorder(0, 2, 0, 0));
    rating->AddChildView(rating_count);

    layout->StartRow(0, column_set_id);
    views::Label* user_count =
        new views::Label(prompt->GetUserCount(), small_font_list);
    user_count->SetAutoColorReadabilityEnabled(false);
    user_count->SetEnabledColor(SK_ColorGRAY);
    layout->AddView(user_count);

    layout->StartRow(0, column_set_id);
    views::Link* store_link = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK));
    store_link->SetFontList(small_font_list);
    store_link->set_listener(this);
    layout->AddView(store_link);
  }

  if (is_bundle_install()) {
    BundleInstaller::ItemList items = prompt->bundle()->GetItemsWithState(
        BundleInstaller::Item::STATE_PENDING);
    for (size_t i = 0; i < items.size(); ++i) {
      base::string16 extension_name =
          base::UTF8ToUTF16(items[i].localized_name);
      base::i18n::AdjustStringForLocaleDirection(&extension_name);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, column_set_id);
      views::Label* extension_label = new views::Label(
          PrepareForDisplay(extension_name, true));
      extension_label->SetMultiLine(true);
      extension_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      extension_label->SizeToFit(left_column_width);
      layout->AddView(extension_label);
    }
  }

  if (prompt->ShouldShowPermissions()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    if (prompt->GetPermissionCount() > 0) {
      if (is_inline_install()) {
        layout->StartRow(0, column_set_id);
        layout->AddView(new views::Separator(views::Separator::HORIZONTAL),
                        3, 1, views::GridLayout::FILL, views::GridLayout::FILL);
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      }

      layout->StartRow(0, column_set_id);
      views::Label* permissions_header = NULL;
      if (is_bundle_install()) {
        // We need to pass the FontList in the constructor, rather than calling
        // SetFontList later, because otherwise SizeToFit mis-judges the width
        // of the line.
        permissions_header =
            new views::Label(prompt->GetPermissionsHeading(),
                             rb.GetFontList(ui::ResourceBundle::MediumFont));
      } else {
        permissions_header = new views::Label(prompt->GetPermissionsHeading());
      }
      permissions_header->SetMultiLine(true);
      permissions_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      permissions_header->SizeToFit(left_column_width);
      layout->AddView(permissions_header);

      for (size_t i = 0; i < prompt->GetPermissionCount(); ++i) {
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
        layout->StartRow(0, column_set_id);
        views::Label* permission_label =
            new views::Label(prompt->GetPermission(i));

        const SkColor kTextHighlight = SK_ColorRED;
        const SkColor kBackgroundHighlight = SkColorSetRGB(0xFB, 0xF7, 0xA3);
        if (prompt->experiment()->ShouldHighlightText(
                prompt->GetPermission(i))) {
          permission_label->SetAutoColorReadabilityEnabled(false);
          permission_label->SetEnabledColor(kTextHighlight);
        } else if (prompt->experiment()->ShouldHighlightBackground(
                       prompt->GetPermission(i))) {
          permission_label->SetLineHeight(18);
          permission_label->set_background(
              views::Background::CreateSolidBackground(kBackgroundHighlight));
        }

        permission_label->SetMultiLine(true);
        permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

        if (prompt->experiment()->show_checkboxes()) {
          permission_label->SizeToFit(left_column_width);
          layout->AddView(new CheckboxedView(permission_label, this));
          ++unchecked_boxes_;
        } else {
          permission_label->SizeToFit(left_column_width - kBulletWidth);
          layout->AddView(new BulletedView(permission_label));
        }

        // If we have more details to provide, show them in collapsed form.
        if (!prompt->GetPermissionsDetails(i).empty()) {
          layout->StartRow(0, column_set_id);
          PermissionDetails details;
          details.push_back(
              PrepareForDisplay(prompt->GetPermissionsDetails(i), false));
          ExpandableContainerView* details_container =
              new ExpandableContainerView(
                  this, base::string16(), details, left_column_width,
                  true, true, false);
          layout->AddView(details_container);
        }

        if (prompt->experiment()->should_show_inline_explanations()) {
          base::string16 explanation =
              prompt->experiment()->GetInlineExplanation(
                  prompt->GetPermission(i));
          if (!explanation.empty()) {
            PermissionDetails details;
            details.push_back(explanation);
            ExpandableContainerView* container =
                new ExpandableContainerView(this, base::string16(), details,
                                            left_column_width,
                                            false, false, true);
            // Inline explanations are expanded by default if there is
            // no "Show details" link.
            if (!prompt->experiment()->show_details_link())
              container->ExpandWithoutAnimation();
            layout->StartRow(0, column_set_id);
            layout->AddView(container);
            inline_explanations_.push_back(container);
          }
        }
      }
    } else {
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, column_set_id);
      views::Label* permission_label = new views::Label(
          l10n_util::GetStringUTF16(IDS_EXTENSION_NO_SPECIAL_PERMISSIONS));
      permission_label->SetMultiLine(true);
      permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      permission_label->SizeToFit(left_column_width);
      layout->AddView(permission_label);
    }
  }

  if (prompt->GetRetainedFileCount()) {
    // Slide in under the permissions, if there are any. If there are
    // either, the retained files prompt stretches all the way to the
    // right of the dialog. If there are no permissions, the retained
    // files prompt just takes up the left column.
    int space_for_files = left_column_width;
    if (prompt->GetPermissionCount()) {
      space_for_files += kIconSize;
      views::ColumnSet* column_set = layout->AddColumnSet(++column_set_id);
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            1,
                            views::GridLayout::USE_PREF,
                            0,  // no fixed width
                            space_for_files);
    }

    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    layout->StartRow(0, column_set_id);
    views::Label* retained_files_header = NULL;
    retained_files_header = new views::Label(prompt->GetRetainedFilesHeading());
    retained_files_header->SetMultiLine(true);
    retained_files_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    retained_files_header->SizeToFit(space_for_files);
    layout->AddView(retained_files_header);

    layout->StartRow(0, column_set_id);
    PermissionDetails details;
    for (size_t i = 0; i < prompt->GetRetainedFileCount(); ++i)
      details.push_back(prompt->GetRetainedFile(i));
    ExpandableContainerView* issue_advice_view =
        new ExpandableContainerView(
            this, base::string16(), details, space_for_files,
            false, true, false);
    layout->AddView(issue_advice_view);
  }

  DCHECK(prompt->type() >= 0);
  UMA_HISTOGRAM_ENUMERATION("Extensions.InstallPrompt.Type",
                            prompt->type(),
                            ExtensionInstallPrompt::NUM_PROMPT_TYPES);

  if (prompt->ShouldShowPermissions()) {
    if (prompt->ShouldShowExplanationText()) {
      views::ColumnSet* column_set = layout->AddColumnSet(++column_set_id);
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::FILL,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      // Add two rows of space so that the text stands out.
      layout->AddPaddingRow(0, 2 * views::kRelatedControlVerticalSpacing);

      layout->StartRow(0, column_set_id);
      views::Label* explanation =
          new views::Label(prompt->experiment()->GetExplanationText());
      explanation->SetMultiLine(true);
      explanation->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      explanation->SizeToFit(left_column_width + kIconSize);
      layout->AddView(explanation);
    }

    if (prompt->experiment()->should_show_expandable_permission_list() ||
        (prompt->experiment()->show_details_link() &&
         prompt->experiment()->should_show_inline_explanations() &&
         !inline_explanations_.empty())) {
      // Don't show the "Show details" link if there are retained
      // files.  These have their own "Show details" links and having
      // multiple levels of links is confusing.
      if (prompt->GetRetainedFileCount() == 0) {
        int text_id =
            prompt->experiment()->should_show_expandable_permission_list()
                ? IDS_EXTENSION_PROMPT_EXPERIMENT_SHOW_PERMISSIONS
                : IDS_EXTENSION_PROMPT_EXPERIMENT_SHOW_DETAILS;
        show_details_link_ = new views::Link(
            l10n_util::GetStringUTF16(text_id));
        show_details_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        show_details_link_->set_listener(this);
        UpdateLinkActionHistogram(LINK_SHOWN);
      } else {
        UpdateLinkActionHistogram(LINK_NOT_SHOWN);
      }
    }

    if (prompt->experiment()->show_checkboxes()) {
      checkbox_info_label_ = new views::Label(
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_EXPERIMENT_CHECKBOX_INFO));
      checkbox_info_label_->SetMultiLine(true);
      checkbox_info_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      checkbox_info_label_->SetAutoColorReadabilityEnabled(false);
      checkbox_info_label_->SetEnabledColor(kLighterLabelColor);
    }
  }

  gfx::Size scrollable_size = scrollable_->GetPreferredSize();
  scrollable_->SetBoundsRect(gfx::Rect(scrollable_size));
  dialog_size_ = gfx::Size(
      dialog_width,
      std::min(scrollable_size.height(), kDialogMaxHeight));

  if (scrollable_header_only_) {
    gfx::Size header_only_size = scrollable_header_only_->GetPreferredSize();
    scrollable_header_only_->SetBoundsRect(gfx::Rect(header_only_size));
    dialog_size_ = gfx::Size(
        dialog_width, std::min(header_only_size.height(), kDialogMaxHeight));
  }

  std::string event_name = ExperienceSamplingEvent::kExtensionInstallDialog;
  event_name.append(
      ExtensionInstallPrompt::PromptTypeToString(prompt_->type()));
  sampling_event_ = ExperienceSamplingEvent::Create(event_name);
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {}

views::GridLayout* ExtensionInstallDialogView::CreateLayout(
    views::View* parent,
    int left_column_width,
    int column_set_id,
    bool single_detail_row) const {
  views::GridLayout* layout = views::GridLayout::CreatePanel(parent);
  parent->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::FILL,
                        0,  // no resizing
                        views::GridLayout::USE_PREF,
                        0,  // no fixed width
                        left_column_width);
  if (!is_bundle_install()) {
    column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::LEADING,
                          0,  // no resizing
                          views::GridLayout::USE_PREF,
                          0,  // no fixed width
                          kIconSize);
  }

  layout->StartRow(0, column_set_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::Label* heading = new views::Label(
      prompt_->GetHeading(), rb.GetFontList(ui::ResourceBundle::MediumFont));
  heading->SetMultiLine(true);
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  heading->SizeToFit(left_column_width);
  layout->AddView(heading);

  if (!is_bundle_install()) {
    // Scale down to icon size, but allow smaller icons (don't scale up).
    const gfx::ImageSkia* image = prompt_->icon().ToImageSkia();
    gfx::Size size(image->width(), image->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    views::ImageView* icon = new views::ImageView();
    icon->SetImageSize(size);
    icon->SetImage(*image);
    icon->SetHorizontalAlignment(views::ImageView::CENTER);
    icon->SetVerticalAlignment(views::ImageView::CENTER);
    if (single_detail_row) {
      layout->AddView(icon);
    } else {
      int icon_row_span = 1;
      if (is_inline_install()) {
        // Also span the rating, user_count and store_link rows.
        icon_row_span = 4;
      } else if (prompt_->ShouldShowPermissions()) {
        size_t permission_count = prompt_->GetPermissionCount();
        // Also span the permission header and each of the permission rows (all
        // have a padding row above it). This also works for the 'no special
        // permissions' case.
        icon_row_span = 3 + permission_count * 2;
      } else if (prompt_->GetRetainedFileCount()) {
        // Also span the permission header and the retained files container.
        icon_row_span = 4;
      }
      layout->AddView(icon, 1, icon_row_span);
    }
  }
  return layout;
}

void ExtensionInstallDialogView::ContentsChanged() {
  Layout();
}

void ExtensionInstallDialogView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Since we want the links to show up in the same visual row as the accept
  // and cancel buttons, which is provided by the framework, we must add the
  // buttons to the non-client view, which is the parent of this view.
  // Similarly, when we're removed from the view hierarchy, we must take care
  // to clean up those items as well.
  if (details.child == this) {
    if (details.is_add) {
      if (show_details_link_)
        details.parent->AddChildView(show_details_link_);
      if (checkbox_info_label_)
        details.parent->AddChildView(checkbox_info_label_);
    } else {
      if (show_details_link_)
        details.parent->RemoveChildView(show_details_link_);
      if (checkbox_info_label_)
        details.parent->RemoveChildView(checkbox_info_label_);
    }
  }
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
      return prompt_->HasAbortButtonLabel()
                 ? prompt_->GetAbortButtonLabel()
                 : l10n_util::GetStringUTF16(IDS_CANCEL);
    default:
      NOTREACHED();
      return base::string16();
  }
}

int ExtensionInstallDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

bool ExtensionInstallDialogView::Cancel() {
  UpdateInstallResultHistogram(false);
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  delegate_->InstallUIAbort(true);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
  UpdateInstallResultHistogram(true);
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
  delegate_->InstallUIProceed();
  return true;
}

ui::ModalType ExtensionInstallDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 ExtensionInstallDialogView::GetWindowTitle() const {
  return prompt_->GetDialogTitle();
}

void ExtensionInstallDialogView::LinkClicked(views::Link* source,
                                             int event_flags) {
  if (source == show_details_link_) {
    UpdateLinkActionHistogram(LINK_CLICKED);
    // Show details link is used to either reveal whole permission list or to
    // reveal inline explanations.
    if (prompt_->experiment()->should_show_expandable_permission_list()) {
      gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
      int spacing = bounds.height() -
          scrollable_header_only_->GetPreferredSize().height();
      int content_height = std::min(scrollable_->GetPreferredSize().height(),
                                    kDialogMaxHeight);
      bounds.set_height(spacing + content_height);
      scroll_view_->SetContents(scrollable_);
      GetWidget()->SetBoundsConstrained(bounds);
      ContentsChanged();
    } else {
      ToggleInlineExplanations();
    }
    show_details_link_->SetVisible(false);
  } else {
    GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                   prompt_->extension()->id());
    OpenURLParams params(
        store_url, Referrer(), NEW_FOREGROUND_TAB,
        content::PAGE_TRANSITION_LINK,
        false);
    navigator_->OpenURL(params);
    GetWidget()->Close();
  }
}

void ExtensionInstallDialogView::ToggleInlineExplanations() {
  for (InlineExplanations::iterator it = inline_explanations_.begin();
      it != inline_explanations_.end(); ++it)
    (*it)->ToggleDetailLevel();
}

void ExtensionInstallDialogView::Layout() {
  scroll_view_->SetBounds(0, 0, width(), height());

  if (show_details_link_ || checkbox_info_label_) {
    views::LabelButton* cancel_button = GetDialogClientView()->cancel_button();
    gfx::Rect parent_bounds = parent()->GetContentsBounds();
    // By default, layouts have an inset of kButtonHEdgeMarginNew. In order to
    // align the link horizontally with the left side of the contents of the
    // layout, put a horizontal margin with this amount.
    const int horizontal_margin = views::kButtonHEdgeMarginNew;
    const int vertical_margin = views::kButtonVEdgeMarginNew;
    int y_buttons = parent_bounds.bottom() -
        cancel_button->GetPreferredSize().height() - vertical_margin;
    int max_width = dialog_size_.width() - cancel_button->width() * 2 -
        horizontal_margin * 2 - views::kRelatedButtonHSpacing;
    if (show_details_link_) {
      gfx::Size link_size = show_details_link_->GetPreferredSize();
      show_details_link_->SetBounds(
          horizontal_margin,
          y_buttons + (cancel_button->height() - link_size.height()) / 2,
          link_size.width(), link_size.height());
    }
    if (checkbox_info_label_) {
      gfx::Size label_size = checkbox_info_label_->GetPreferredSize();
      checkbox_info_label_->SetBounds(
          horizontal_margin,
          y_buttons + (cancel_button->height() - label_size.height()) / 2,
          label_size.width(), label_size.height());
      checkbox_info_label_->SizeToFit(max_width);
    }
  }
  // Disable accept button if there are unchecked boxes and
  // the experiment is on.
  if (prompt_->experiment()->show_checkboxes())
    GetDialogClientView()->ok_button()->SetEnabled(unchecked_boxes_ == 0);

  DialogDelegateView::Layout();
}

gfx::Size ExtensionInstallDialogView::GetPreferredSize() const {
  return dialog_size_;
}

void ExtensionInstallDialogView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (std::string(views::Checkbox::kViewClassName) == sender->GetClassName()) {
    views::Checkbox* checkbox = static_cast<views::Checkbox*>(sender);
    if (checkbox->checked())
      --unchecked_boxes_;
    else
      ++unchecked_boxes_;

    GetDialogClientView()->ok_button()->SetEnabled(unchecked_boxes_ == 0);
    checkbox_info_label_->SetVisible(unchecked_boxes_ > 0);
  }
}

void ExtensionInstallDialogView::UpdateInstallResultHistogram(bool accepted)
    const {
  if (prompt_->type() == ExtensionInstallPrompt::INSTALL_PROMPT)
    UMA_HISTOGRAM_BOOLEAN("Extensions.InstallPrompt.Accepted", accepted);
}

void ExtensionInstallDialogView::UpdateLinkActionHistogram(int action_type)
    const {
  if (prompt_->experiment()->should_show_expandable_permission_list()) {
    // The clickable link in the UI is "Show Permissions".
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.InstallPromptExperiment.ShowPermissions",
        action_type,
        NUM_LINK_ACTIONS);
  } else {
    // The clickable link in the UI is "Show Details".
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.InstallPromptExperiment.ShowDetails",
        action_type,
        NUM_LINK_ACTIONS);
  }
}

// ExpandableContainerView::DetailsView ----------------------------------------

ExpandableContainerView::DetailsView::DetailsView(int horizontal_space,
                                                  bool parent_bulleted,
                                                  bool lighter_color)
    : layout_(new views::GridLayout(this)),
      state_(0),
      lighter_color_(lighter_color) {
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
  if (lighter_color_) {
    detail_label->SetEnabledColor(kLighterLabelColor);
    detail_label->SetAutoColorReadabilityEnabled(false);
  }
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
    ExtensionInstallDialogView* owner,
    const base::string16& description,
    const PermissionDetails& details,
    int horizontal_space,
    bool parent_bulleted,
    bool show_expand_link,
    bool lighter_color_details)
    : owner_(owner),
      details_view_(NULL),
      more_details_(NULL),
      slide_animation_(this),
      arrow_toggle_(NULL),
      expanded_(false) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  if (!description.empty()) {
    layout->StartRow(0, column_set_id);

    views::Label* description_label = new views::Label(description);
    description_label->SetMultiLine(true);
    description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    description_label->SizeToFit(horizontal_space);
    layout->AddView(new BulletedView(description_label));
  }

  if (details.empty())
    return;

  details_view_ = new DetailsView(horizontal_space, parent_bulleted,
                                  lighter_color_details);

  layout->StartRow(0, column_set_id);
  layout->AddView(details_view_);

  for (size_t i = 0; i < details.size(); ++i)
    details_view_->AddDetail(details[i]);

  // TODO(meacer): Remove show_expand_link when the experiment is completed.
  if (show_expand_link) {
    views::Link* link = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));

    // Make sure the link width column is as wide as needed for both Show and
    // Hide details, so that the arrow doesn't shift horizontally when we
    // toggle.
    int link_col_width =
        views::kRelatedControlHorizontalSpacing +
        std::max(gfx::GetStringWidth(
                     l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS),
                     link->font_list()),
                 gfx::GetStringWidth(
                     l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS),
                     link->font_list()));

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
                          views::GridLayout::LEADING,
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
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    arrow_toggle_ = new views::ImageButton(this);
    arrow_toggle_->SetImage(views::Button::STATE_NORMAL,
                            rb.GetImageSkiaNamed(IDR_DOWN_ARROW));
    layout->AddView(arrow_toggle_);
  }
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
  if (more_details_) {
    more_details_->SetText(expanded_ ?
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS) :
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  }
}

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  owner_->ContentsChanged();
}

void ExpandableContainerView::ToggleDetailLevel() {
  expanded_ = !expanded_;

  if (slide_animation_.IsShowing())
    slide_animation_.Hide();
  else
    slide_animation_.Show();
}

void ExpandableContainerView::ExpandWithoutAnimation() {
  expanded_ = true;
  details_view_->AnimateToState(1.0);
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
