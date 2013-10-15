// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/transform.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using content::OpenURLParams;
using content::Referrer;
using extensions::BundleInstaller;

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// The dialog width.
const int kDialogWidth = 385;

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

typedef std::vector<string16> PermissionDetails;

void AddResourceIcon(const gfx::ImageSkia* skia_image, void* data) {
  views::View* parent = static_cast<views::View*>(data);
  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(*skia_image);
  parent->AddChildView(image_view);
}

// Creates a string for displaying |message| to the user. If it has to look
// like a entry in a bullet point list, one is added.
string16 PrepareForDisplay(const string16& message, bool bullet_point) {
  return bullet_point ? l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PERMISSION_LINE,
      message) : message;
}

// A custom scrollable view implementation for the dialog.
class CustomScrollableView : public views::View {
 public:
  CustomScrollableView();
  virtual ~CustomScrollableView();

 private:
  virtual void Layout() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CustomScrollableView);
};

// Implements the extension installation dialog for TOOLKIT_VIEWS.
class ExtensionInstallDialogView : public views::DialogDelegateView,
                                   public views::LinkListener {
 public:
  ExtensionInstallDialogView(content::PageNavigator* navigator,
                             ExtensionInstallPrompt::Delegate* delegate,
                             const ExtensionInstallPrompt::Prompt& prompt);
  virtual ~ExtensionInstallDialogView();

  // Called when one of the child elements has expanded/collapsed.
  void ContentsChanged();

 private:
  // views::DialogDelegateView:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::WidgetDelegate
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual bool ShouldShowCloseButton() const OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  bool is_inline_install() const {
    return prompt_.type() == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT;
  }

  bool is_first_run() const {
    return prompt_.type() ==
        ExtensionInstallPrompt::DEFAULT_INSTALL_FIRST_RUN_PROMPT;
  }

  bool is_bundle_install() const {
    return prompt_.type() == ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT;
  }

  bool is_external_install() const {
    return prompt_.type() == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT;
  }

  content::PageNavigator* navigator_;
  ExtensionInstallPrompt::Delegate* delegate_;
  ExtensionInstallPrompt::Prompt prompt_;

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  views::ScrollView* scroll_view_;

  // The container view for the scroll view.
  CustomScrollableView* scrollable_;

  // The preferred size of the dialog.
  gfx::Size dialog_size_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

// A simple view that prepends a view with a bullet with the help of a grid
// layout.
class BulletedView : public views::View {
 public:
  explicit BulletedView(views::View* view);
 private:
  DISALLOW_COPY_AND_ASSIGN(BulletedView);
};

BulletedView::BulletedView(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0, // no fixed width
                        0);
   column_set->AddColumn(views::GridLayout::LEADING,
                         views::GridLayout::LEADING,
                         0,
                         views::GridLayout::USE_PREF,
                         0,  // no fixed width
                         0);
  layout->StartRow(0, 0);
  layout->AddView(new views::Label(PrepareForDisplay(string16(), true)));
  layout->AddView(view);
}

// A view to display text with an expandable details section.
class ExpandableContainerView : public views::View,
                                public views::ButtonListener,
                                public views::LinkListener,
                                public gfx::AnimationDelegate {
 public:
  ExpandableContainerView(ExtensionInstallDialogView* owner,
                          const string16& description,
                          const PermissionDetails& details,
                          int horizontal_space,
                          bool parent_bulleted);
  virtual ~ExpandableContainerView();

  // views::View:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

 private:
  // A view which displays all the details of an IssueAdviceInfoEntry.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(int horizontal_space, bool parent_bulleted);
    virtual ~DetailsView() {}

    // views::View:
    virtual gfx::Size GetPreferredSize() OVERRIDE;

    void AddDetail(const string16& detail);

    // Animates this to be a height proportional to |state|.
    void AnimateToState(double state);

   private:
    views::GridLayout* layout_;
    double state_;

    // Whether the parent item is showing bullets. This will determine how much
    // extra indentation is needed.
    bool parent_bulleted_;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // Expand/Collapse the detail section for this ExpandableContainerView.
  void ToggleDetailLevel();

  // The dialog that owns |this|. It's also an ancestor in the View hierarchy.
  ExtensionInstallDialogView* owner_;

  // A view for showing |issue_advice.details|.
  DetailsView* details_view_;

  // The '>' zippy control.
  views::ImageView* arrow_view_;

  gfx::SlideAnimation slide_animation_;

  // The 'more details' link shown under the heading (changes to 'hide details'
  // when the details section is expanded).
  views::Link* more_details_;

  // The up/down arrow next to the 'more detail' link (points up/down depending
  // on whether the details section is expanded).
  views::ImageButton* arrow_toggle_;

  // Whether the details section is expanded.
  bool expanded_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableContainerView);
};

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CreateBrowserModalDialogViews(
      new ExtensionInstallDialogView(show_params.navigator, delegate, prompt),
      show_params.parent_window)->Show();
}

}  // namespace

CustomScrollableView::CustomScrollableView() {}
CustomScrollableView::~CustomScrollableView() {}

void CustomScrollableView::Layout() {
  SetBounds(x(), y(), width(), GetHeightForWidth(width()));
  views::View::Layout();
}

ExtensionInstallDialogView::ExtensionInstallDialogView(
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt)
    : navigator_(navigator),
      delegate_(delegate),
      prompt_(prompt) {
  // Possible grid layouts:
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
  // w/ permissions XOR oauth issues    no permissions
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
  // w/ permissions AND oauth issues
  // +--------------------+------+
  // | heading            | icon |
  // +--------------------|      |
  // | permissions_header |      |
  // +--------------------|      |
  // | permission1        |      |
  // +--------------------|      |
  // | permission2        |      |
  // +--------------------+------+
  // | oauth header              |
  // +---------------------------+
  // | oauth issue 1             |
  // +---------------------------+
  // | oauth issue 2             |
  // +---------------------------+

  scroll_view_ = new views::ScrollView();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  AddChildView(scroll_view_);
  scrollable_ = new CustomScrollableView();
  scroll_view_->SetContents(scrollable_);

  views::GridLayout* layout = views::GridLayout::CreatePanel(scrollable_);
  scrollable_->SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  int left_column_width =
      (prompt.ShouldShowPermissions() + prompt.GetOAuthIssueCount() +
       prompt.GetRetainedFileCount()) > 0 ?
          kPermissionsLeftColumnWidth : kNoPermissionsLeftColumnWidth;
  if (is_bundle_install())
    left_column_width = kBundleLeftColumnWidth;
  if (is_external_install())
    left_column_width = kExternalInstallLeftColumnWidth;

  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::FILL,
                        0,  // no resizing
                        views::GridLayout::USE_PREF,
                        0,  // no fixed width
                        left_column_width);
  if (!is_bundle_install()) {
    column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    column_set->AddColumn(views::GridLayout::LEADING,
                          views::GridLayout::LEADING,
                          0,  // no resizing
                          views::GridLayout::USE_PREF,
                          0,  // no fixed width
                          kIconSize);
  }

  layout->StartRow(0, column_set_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::Label* heading = new views::Label(prompt.GetHeading());
  heading->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  heading->SetMultiLine(true);
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  heading->SizeToFit(left_column_width);
  layout->AddView(heading);

  if (!is_bundle_install()) {
    // Scale down to icon size, but allow smaller icons (don't scale up).
    const gfx::ImageSkia* image = prompt.icon().ToImageSkia();
    gfx::Size size(image->width(), image->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    views::ImageView* icon = new views::ImageView();
    icon->SetImageSize(size);
    icon->SetImage(*image);
    icon->SetHorizontalAlignment(views::ImageView::CENTER);
    icon->SetVerticalAlignment(views::ImageView::CENTER);
    int icon_row_span = 1;
    if (is_inline_install()) {
      // Also span the rating, user_count and store_link rows.
      icon_row_span = 4;
    } else if (prompt.ShouldShowPermissions()) {
      size_t permission_count = prompt.GetPermissionCount();
      if (permission_count > 0) {
        // Also span the permission header and each of the permission rows (all
        // have a padding row above it).
        icon_row_span = 3 + permission_count * 2;
      } else {
        // This is the 'no special permissions' case, so span the line we add
        // (without a header) saying the extension has no special privileges.
        icon_row_span = 4;
      }
    } else if (prompt.GetOAuthIssueCount()) {
      // Also span the permission header and each of the permission rows (all
      // have a padding row above it).
      icon_row_span = 3 + prompt.GetOAuthIssueCount() * 2;
    } else if (prompt.GetRetainedFileCount()) {
      // Also span the permission header and the retained files container.
      icon_row_span = 4;
    }
    layout->AddView(icon, 1, icon_row_span);
  }

  if (is_inline_install()) {
    layout->StartRow(0, column_set_id);
    views::View* rating = new views::View();
    rating->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    layout->AddView(rating);
    prompt.AppendRatingStars(AddResourceIcon, rating);

    views::Label* rating_count = new views::Label(prompt.GetRatingCount());
    rating_count->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
    // Add some space between the stars and the rating count.
    rating_count->set_border(views::Border::CreateEmptyBorder(0, 2, 0, 0));
    rating->AddChildView(rating_count);

    layout->StartRow(0, column_set_id);
    views::Label* user_count = new views::Label(prompt.GetUserCount());
    user_count->SetAutoColorReadabilityEnabled(false);
    user_count->SetEnabledColor(SK_ColorGRAY);
    user_count->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
    layout->AddView(user_count);

    layout->StartRow(0, column_set_id);
    views::Link* store_link = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK));
    store_link->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
    store_link->set_listener(this);
    layout->AddView(store_link);
  }

  if (is_bundle_install()) {
    BundleInstaller::ItemList items = prompt.bundle()->GetItemsWithState(
        BundleInstaller::Item::STATE_PENDING);
    for (size_t i = 0; i < items.size(); ++i) {
      string16 extension_name = UTF8ToUTF16(items[i].localized_name);
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

  if (prompt.ShouldShowPermissions()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    if (prompt.GetPermissionCount() > 0) {
      if (is_inline_install()) {
        layout->StartRow(0, column_set_id);
        layout->AddView(new views::Separator(views::Separator::HORIZONTAL),
                        3, 1, views::GridLayout::FILL, views::GridLayout::FILL);
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      }

      layout->StartRow(0, column_set_id);
      views::Label* permissions_header = NULL;
      if (is_bundle_install()) {
        // We need to pass the Font in the constructor, rather than calling
        // SetFont later, because otherwise SizeToFit mis-judges the width
        // of the line.
        permissions_header = new views::Label(
            prompt.GetPermissionsHeading(),
            rb.GetFont(ui::ResourceBundle::MediumFont));
      } else {
        permissions_header = new views::Label(prompt.GetPermissionsHeading());
      }
      permissions_header->SetMultiLine(true);
      permissions_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      permissions_header->SizeToFit(left_column_width);
      layout->AddView(permissions_header);

      for (size_t i = 0; i < prompt.GetPermissionCount(); ++i) {
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
        layout->StartRow(0, column_set_id);
        views::Label* permission_label =
            new views::Label(prompt.GetPermission(i));
        permission_label->SetMultiLine(true);
        permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        permission_label->SizeToFit(left_column_width);
        layout->AddView(new BulletedView(permission_label));

        // If we have more details to provide, show them in collapsed form.
        if (!prompt.GetPermissionsDetails(i).empty()) {
          layout->StartRow(0, column_set_id);
          PermissionDetails details;
          details.push_back(
              PrepareForDisplay(prompt.GetPermissionsDetails(i), false));
          ExpandableContainerView* details_container =
              new ExpandableContainerView(
                  this, string16(), details, left_column_width, true);
          layout->AddView(details_container);
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

  if (prompt.GetOAuthIssueCount()) {
    // Slide in under the permissions, if there are any. If there are
    // permissions, the OAuth prompt stretches all the way to the right of the
    // dialog. If there are no permissions, the OAuth prompt just takes up the
    // left column.
    int space_for_oauth = left_column_width;
    if (prompt.GetPermissionCount()) {
      space_for_oauth += kIconSize;
      column_set = layout->AddColumnSet(++column_set_id);
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            1,
                            views::GridLayout::USE_PREF,
                            0,  // no fixed width
                            space_for_oauth);
    }

    layout->StartRowWithPadding(0, column_set_id,
                                0, views::kRelatedControlVerticalSpacing);
    views::Label* oauth_header = new views::Label(prompt.GetOAuthHeading());
    oauth_header->SetMultiLine(true);
    oauth_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    oauth_header->SizeToFit(left_column_width);
    layout->AddView(oauth_header);

    for (size_t i = 0; i < prompt.GetOAuthIssueCount(); ++i) {
      layout->StartRowWithPadding(
          0, column_set_id,
          0, views::kRelatedControlVerticalSpacing);

      PermissionDetails details;
      const IssueAdviceInfoEntry& entry = prompt.GetOAuthIssue(i);
      for (size_t x = 0; x < entry.details.size(); ++x)
        details.push_back(entry.details[x]);
      ExpandableContainerView* issue_advice_view =
          new ExpandableContainerView(
              this, entry.description, details, space_for_oauth, true);
      layout->AddView(issue_advice_view);
    }
  }
  if (prompt.GetRetainedFileCount()) {
    // Slide in under the permissions or OAuth, if there are any. If there are
    // either, the retained files prompt stretches all the way to the right of
    // the dialog. If there are no permissions or OAuth, the retained files
    // prompt just takes up the left column.
    int space_for_files = left_column_width;
    if (prompt.GetPermissionCount() || prompt.GetOAuthIssueCount()) {
      space_for_files += kIconSize;
      column_set = layout->AddColumnSet(++column_set_id);
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
    retained_files_header =
        new views::Label(prompt.GetRetainedFilesHeading());
    retained_files_header->SetMultiLine(true);
    retained_files_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    retained_files_header->SizeToFit(space_for_files);
    layout->AddView(retained_files_header);

    layout->StartRow(0, column_set_id);
    PermissionDetails details;
    for (size_t i = 0; i < prompt.GetRetainedFileCount(); ++i)
      details.push_back(prompt.GetRetainedFile(i));
    ExpandableContainerView* issue_advice_view =
        new ExpandableContainerView(
            this, string16(), details, space_for_files, false);
    layout->AddView(issue_advice_view);
  }

  gfx::Size scrollable_size = scrollable_->GetPreferredSize();
  scrollable_->SetBoundsRect(gfx::Rect(scrollable_size));
  dialog_size_ = gfx::Size(
      kDialogWidth,
      std::min(scrollable_size.height(), kDialogMaxHeight));
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {}

void ExtensionInstallDialogView::ContentsChanged() {
  Layout();
}

int ExtensionInstallDialogView::GetDialogButtons() const {
  int buttons = prompt_.GetDialogButtons();
  // Simply having just an OK button is *not* supported. See comment on function
  // GetDialogButtons in dialog_delegate.h for reasons.
  DCHECK_GT(buttons & ui::DIALOG_BUTTON_CANCEL, 0);
  return buttons;
}

string16 ExtensionInstallDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return prompt_.GetAcceptButtonLabel();
    case ui::DIALOG_BUTTON_CANCEL:
      return prompt_.HasAbortButtonLabel() ?
          prompt_.GetAbortButtonLabel() :
          l10n_util::GetStringUTF16(IDS_CANCEL);
    default:
      NOTREACHED();
      return string16();
  }
}

int ExtensionInstallDialogView::GetDefaultDialogButton() const {
  if (is_first_run())
    return ui::DIALOG_BUTTON_OK;
  else
    return ui::DIALOG_BUTTON_CANCEL;
}

bool ExtensionInstallDialogView::Cancel() {
  delegate_->InstallUIAbort(true);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
  delegate_->InstallUIProceed();
  return true;
}

ui::ModalType ExtensionInstallDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 ExtensionInstallDialogView::GetWindowTitle() const {
  return prompt_.GetDialogTitle();
}

void ExtensionInstallDialogView::LinkClicked(views::Link* source,
                                             int event_flags) {
  GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                 prompt_.extension()->id());
  OpenURLParams params(
      store_url, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
      false);
  navigator_->OpenURL(params);
  GetWidget()->Close();
}

void ExtensionInstallDialogView::Layout() {
  scroll_view_->SetBounds(0, 0, width(), height());
  DialogDelegateView::Layout();
}

gfx::Size ExtensionInstallDialogView::GetPreferredSize() {
  return dialog_size_;
}

bool ExtensionInstallDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool ExtensionInstallDialogView::ShouldShowCloseButton() const {
  return false;
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}

// ExpandableContainerView::DetailsView ----------------------------------------

ExpandableContainerView::DetailsView::DetailsView(int horizontal_space,
                                                  bool parent_bulleted)
    : layout_(new views::GridLayout(this)),
      state_(0),
      parent_bulleted_(parent_bulleted) {
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

void ExpandableContainerView::DetailsView::AddDetail(const string16& detail) {
  layout_->StartRowWithPadding(0, 0,
                               0, views::kRelatedControlSmallVerticalSpacing);
  views::Label* detail_label =
      new views::Label(PrepareForDisplay(detail, false));
  detail_label->SetMultiLine(true);
  detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout_->AddView(detail_label);
}

gfx::Size ExpandableContainerView::DetailsView::GetPreferredSize() {
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
    const string16& description,
    const PermissionDetails& details,
    int horizontal_space,
    bool parent_bulleted)
    : owner_(owner),
      details_view_(NULL),
      arrow_view_(NULL),
      slide_animation_(this),
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

  details_view_ = new DetailsView(horizontal_space, parent_bulleted);

  layout->StartRow(0, column_set_id);
  layout->AddView(details_view_);

  for (size_t i = 0; i < details.size(); ++i)
    details_view_->AddDetail(details[i]);

  views::Link* link = new views::Link(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));

  // Make sure the link width column is as wide as needed for both Show and
  // Hide details, so that the arrow doesn't shift horizontally when we toggle.
  int link_col_width =
      views::kRelatedControlHorizontalSpacing +
      std::max(link->font_list().GetStringWidth(
                   l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS)),
               link->font_list().GetStringWidth(
                   l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS)));

  column_set = layout->AddColumnSet(++column_set_id);
  // Padding to the left of the More Details column. If the parent is using
  // bullets for its items, then a padding of one unit will make the child item
  // (which has no bullet) look like a sibling of its parent. Therefore increase
  // the indentation by one more unit to show that it is in fact a child item
  // (with no missing bullet) and not a sibling.
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

  more_details_->SetText(expanded_ ?
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS) :
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
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
