// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/transform.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_WIN)
#include "chrome/browser/extensions/app_host_installer_win.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#endif

using content::OpenURLParams;
using content::Referrer;
using extensions::BundleInstaller;

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

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

// Implements the extension installation dialog for TOOLKIT_VIEWS.
class ExtensionInstallDialogView : public views::DialogDelegateView,
                                   public views::LinkListener {
 public:
  ExtensionInstallDialogView(content::PageNavigator* navigator,
                             ExtensionInstallPrompt::Delegate* delegate,
                             const ExtensionInstallPrompt::Prompt& prompt,
                             bool show_launcher_opt_in);
  virtual ~ExtensionInstallDialogView();

  // Changes the size of the containing widget to match the preferred size
  // of this dialog.
  void SizeToContents();

 private:
  // views::DialogDelegateView:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  bool is_inline_install() const {
    return prompt_.type() == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT;
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
  bool show_launcher_opt_in_;

  // The lifetime of |app_launcher_opt_in_checkbox_| is managed by the views
  // system.
  views::Checkbox* app_launcher_opt_in_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

// A view to display a single IssueAdviceInfoEntry.
class IssueAdviceView : public views::View,
                        public ui::AnimationDelegate {
 public:
  IssueAdviceView(ExtensionInstallDialogView* owner,
                  const IssueAdviceInfoEntry& issue_advice,
                  int horizontal_space);
  virtual ~IssueAdviceView() {}

  // Implementation of views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // Implementation of ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  // A view which displays all the details of an IssueAdviceInfoEntry.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(int horizontal_space);
    virtual ~DetailsView() {}

    // Implementation of views::View:
    virtual gfx::Size GetPreferredSize() OVERRIDE;

    void AddDetail(const string16& detail);

    // Animates this to be a height proportional to |state|.
    void AnimateToState(double state);

   private:
    views::GridLayout* layout_;
    double state_;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // The dialog that owns |this|. It's also an ancestor in the View hierarchy.
  ExtensionInstallDialogView* owner_;

  // A view for showing |issue_advice.details|.
  DetailsView* details_view_;

  // The '>' zippy control.
  views::ImageView* arrow_view_;

  ui::SlideAnimation slide_animation_;

  DISALLOW_COPY_AND_ASSIGN(IssueAdviceView);
};

void DoShowDialog(const ExtensionInstallPrompt::ShowParams& show_params,
                  ExtensionInstallPrompt::Delegate* delegate,
                  const ExtensionInstallPrompt::Prompt& prompt,
                  bool show_launcher_opt_in) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  views::Widget::CreateWindowWithParent(
      new ExtensionInstallDialogView(show_params.navigator, delegate, prompt,
                                     show_launcher_opt_in),
      show_params.parent_window)->Show();
}

// Runs on the FILE thread. Check if the launcher is present and then show
// the install dialog with an appropriate |show_launcher_opt_in|.
void CheckLauncherAndShowDialog(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
#if defined(OS_WIN)
  bool present = chrome_launcher_support::IsAppLauncherPresent();
#else
  NOTREACHED();
  bool present = false;
#endif
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DoShowDialog, show_params, delegate, prompt, !present));
}

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_WIN)
  if (BrowserDistribution::GetDistribution()->AppHostIsSupported() &&
      prompt.extension()->is_platform_app()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CheckLauncherAndShowDialog, show_params, delegate, prompt));
    return;
  }
#endif
  DoShowDialog(show_params, delegate, prompt, false);
}

}  // namespace

ExtensionInstallDialogView::ExtensionInstallDialogView(
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt,
    bool show_launcher_opt_in)
    : navigator_(navigator),
      delegate_(delegate),
      prompt_(prompt),
      show_launcher_opt_in_(show_launcher_opt_in),
      app_launcher_opt_in_checkbox_(NULL) {
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
  //
  // w/ launcher opt in
  // +--------------------+------+
  // | heading            | icon |
  // +--------------------|      |
  // | permissions_header |      |
  // |                           |
  // | ......................... |
  // |                           |
  // +--------------------+------+
  // | launcher opt in           |
  // +---------------------------+

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  int left_column_width =
      prompt.GetPermissionCount() + prompt.GetOAuthIssueCount() > 0 ?
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
    } else if (prompt.GetPermissionCount()) {
      // Also span the permission header and each of the permission rows (all
      // have a padding row above it).
      icon_row_span = 3 + prompt.GetPermissionCount() * 2;
    } else if (prompt.GetOAuthIssueCount()) {
      // Also span the permission header and each of the permission rows (all
      // have a padding row above it).
      icon_row_span = 3 + prompt.GetOAuthIssueCount() * 2;
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
    BundleInstaller::ItemList items = prompt_.bundle()->GetItemsWithState(
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

  if (prompt.GetPermissionCount()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    if (is_inline_install()) {
      layout->StartRow(0, column_set_id);
      layout->AddView(new views::Separator(), 3, 1, views::GridLayout::FILL,
                      views::GridLayout::FILL);
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
      views::Label* permission_label = new views::Label(PrepareForDisplay(
          prompt.GetPermission(i), true));
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

      IssueAdviceView* issue_advice_view =
          new IssueAdviceView(this, prompt.GetOAuthIssue(i), space_for_oauth);
      layout->AddView(issue_advice_view);
    }
  }

  if (show_launcher_opt_in) {
    // Put the launcher opt-in prompt at the bottom. It should always stretch
    // to take up the whole width of the dialog.
    column_set = layout->AddColumnSet(++column_set_id);
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::FILL,
                          1,
                          views::GridLayout::USE_PREF,
                          0,  // no fixed width
                          left_column_width + kIconSize);
    layout->StartRowWithPadding(0, column_set_id,
                                0, views::kRelatedControlVerticalSpacing);
    app_launcher_opt_in_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_APP_LIST_OPT_IN_TEXT));
    app_launcher_opt_in_checkbox_->SetFont(
        rb.GetFont(ui::ResourceBundle::BoldFont));
    app_launcher_opt_in_checkbox_->SetChecked(true);
    layout->AddView(app_launcher_opt_in_checkbox_);
  }
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {}

void ExtensionInstallDialogView::SizeToContents() {
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
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
  return ui::DIALOG_BUTTON_CANCEL;
}

bool ExtensionInstallDialogView::Cancel() {
  delegate_->InstallUIAbort(true);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
#if defined(OS_WIN)
  extensions::AppHostInstaller::SetInstallWithLauncher(
      app_launcher_opt_in_checkbox_ &&
      app_launcher_opt_in_checkbox_->checked());
#endif
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

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}

// IssueAdviceView::DetailsView ------------------------------------------------

IssueAdviceView::DetailsView::DetailsView(int horizontal_space)
    : layout_(new views::GridLayout(this)),
      state_(0) {
  SetLayoutManager(layout_);
  views::ColumnSet* column_set = layout_->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        horizontal_space,
                        0);
}

void IssueAdviceView::DetailsView::AddDetail(const string16& detail) {
  layout_->StartRowWithPadding(0, 0,
                               0, views::kRelatedControlSmallVerticalSpacing);
  views::Label* detail_label =
      new views::Label(PrepareForDisplay(detail, true));
  detail_label->SetMultiLine(true);
  detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout_->AddView(detail_label);
}

gfx::Size IssueAdviceView::DetailsView::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), size.height() * state_);
}

void IssueAdviceView::DetailsView::AnimateToState(double state) {
  state_ = state;
  PreferredSizeChanged();
  SchedulePaint();
}

// IssueAdviceView -------------------------------------------------------------

IssueAdviceView::IssueAdviceView(ExtensionInstallDialogView* owner,
                                 const IssueAdviceInfoEntry& issue_advice,
                                 int horizontal_space)
    : owner_(owner),
      details_view_(NULL),
      arrow_view_(NULL),
      slide_animation_(this) {
  // TODO(estade): replace this with a more appropriate image.
  const gfx::ImageSkia& image = *ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_OMNIBOX_TTS);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  if (!issue_advice.details.empty()) {
    column_set->AddColumn(views::GridLayout::LEADING,
                          views::GridLayout::LEADING,
                          0,
                          views::GridLayout::FIXED,
                          image.width(),
                          0);
    horizontal_space -= image.width();
    details_view_ = new DetailsView(horizontal_space);
  }
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::FILL,
                        0,
                        views::GridLayout::FIXED,
                        horizontal_space,
                        0);
  layout->StartRow(0, column_set_id);

  if (details_view_) {
    arrow_view_ = new views::ImageView();
    arrow_view_->SetImage(image);
    arrow_view_->SetVerticalAlignment(views::ImageView::CENTER);
    layout->AddView(arrow_view_);
  }

  views::Label* description_label =
      new views::Label(PrepareForDisplay(issue_advice.description,
                                         !details_view_));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SizeToFit(horizontal_space);
  layout->AddView(description_label);

  if (!details_view_)
    return;

  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
  layout->AddView(details_view_);

  for (size_t i = 0; i < issue_advice.details.size(); ++i)
    details_view_->AddDetail(issue_advice.details[i]);
}

bool IssueAdviceView::OnMousePressed(const ui::MouseEvent& event) {
  return details_view_ && event.IsLeftMouseButton();
}

void IssueAdviceView::OnMouseReleased(const ui::MouseEvent& event) {
  if (slide_animation_.IsShowing())
    slide_animation_.Hide();
  else
    slide_animation_.Show();
}

void IssueAdviceView::AnimationProgressed(const ui::Animation* animation) {
  DCHECK_EQ(animation, &slide_animation_);

  if (details_view_)
    details_view_->AnimateToState(animation->GetCurrentValue());

  if (arrow_view_) {
    gfx::Transform rotate;
    if (animation->GetCurrentValue() != 0.0) {
      rotate.Translate(arrow_view_->width() / 2.0,
                       arrow_view_->height() / 2.0);
      // TODO(estade): for some reason there are rendering errors at 90 degrees.
      // Figure out why.
      rotate.Rotate(animation->GetCurrentValue() * 89);
      rotate.Translate(-arrow_view_->width() / 2.0,
                       -arrow_view_->height() / 2.0);
    }
    arrow_view_->SetTransform(rotate);
  }
}

void IssueAdviceView::ChildPreferredSizeChanged(views::View* child) {
  owner_->SizeToContents();
}
