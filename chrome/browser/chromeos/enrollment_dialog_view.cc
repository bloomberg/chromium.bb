// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enrollment_dialog_view.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::Border;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::LayoutManager;
using views::View;

namespace chromeos {

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 100;

////////////////////////////////////////////////////////////////////////////////
// Dialog for certificate enrollment. This displays the content from the
// certificate enrollment URI.
class EnrollmentDialogView : public views::DialogDelegateView {
 public:
  virtual ~EnrollmentDialogView();

  static void ShowDialog(gfx::NativeWindow owning_window,
                         const std::string& network_name,
                         Profile* profile,
                         const GURL& target_uri,
                         const base::Closure& connect);

  // views::DialogDelegateView overrides
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;

  // views::WidgetDelegate overrides
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::Widget overrides
  virtual views::View* GetContentsView() OVERRIDE;

 private:
  EnrollmentDialogView(const std::string& network_name,
                       Profile* profile,
                       const GURL& target_uri,
                       const base::Closure& connect);
  void InitDialog();

  std::string network_name_;
  Profile* profile_;
  GURL target_uri_;
  base::Closure connect_;
  bool added_cert_;
};

////////////////////////////////////////////////////////////////////////////////
// EnrollmentDialogView implementation.

EnrollmentDialogView::EnrollmentDialogView(const std::string& network_name,
                                           Profile* profile,
                                           const GURL& target_uri,
                                           const base::Closure& connect)
    : network_name_(network_name),
      profile_(profile),
      target_uri_(target_uri),
      connect_(connect),
      added_cert_(false) {
}

EnrollmentDialogView::~EnrollmentDialogView() {
}

// static
void EnrollmentDialogView::ShowDialog(gfx::NativeWindow owning_window,
                                      const std::string& network_name,
                                      Profile* profile,
                                      const GURL& target_uri,
                                      const base::Closure& connect) {
  EnrollmentDialogView* dialog_view =
      new EnrollmentDialogView(network_name, profile, target_uri, connect);
  views::Widget::CreateWindowWithParent(dialog_view, owning_window);
  dialog_view->InitDialog();
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

int EnrollmentDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
}

bool EnrollmentDialogView::Accept() {
  // Navigate to the target URI in a browser tab.
  Browser* browser = browser::FindTabbedBrowser(profile_, false);
  if (!browser) {
    // Couldn't find a tabbed browser: create one.
    browser = Browser::Create(profile_);
  }
  DCHECK(browser);
  browser->AddSelectedTabWithURL(
      target_uri_,
      content::PAGE_TRANSITION_LINK);
  return true;
}

string16 EnrollmentDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_BUTTON);
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

ui::ModalType EnrollmentDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 EnrollmentDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_TITLE);
}

gfx::Size EnrollmentDialogView::GetPreferredSize() {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

views::View* EnrollmentDialogView::GetContentsView() {
  return this;
}

void EnrollmentDialogView::InitDialog() {
  added_cert_ = false;
  // Create the views and layout manager and set them up.
  Label* label = new Label(
      l10n_util::GetStringFUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_INSTRUCTIONS,
                                 UTF8ToUTF16(network_name_)));
  label->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
  label->SetHorizontalAlignment(Label::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);

  GridLayout* grid_layout = GridLayout::CreatePanel(this);
  SetLayoutManager(grid_layout);

  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL,  // Horizontal resize.
                     views::GridLayout::FILL,  // Vertical resize.
                     1,   // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,   // Ignored for USE_PREF.
                     0);  // Minimum size.
  columns = grid_layout->AddColumnSet(1);
  columns->AddPaddingColumn(
      0, views::kUnrelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING,  // Horizontal leading.
                     views::GridLayout::FILL,     // Vertical resize.
                     1,   // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,   // Ignored for USE_PREF.
                     0);  // Minimum size.

  grid_layout->StartRow(0, 0);
  grid_layout->AddView(label);
  grid_layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  grid_layout->Layout(this);
}

////////////////////////////////////////////////////////////////////////////////
// Handler for certificate enrollment.

class DialogEnrollmentDelegate : public EnrollmentDelegate {
 public:
  // |owning_window| is the window that will own the dialog.
  explicit DialogEnrollmentDelegate(gfx::NativeWindow owning_window,
                                    const std::string& network_name,
                                    Profile* profile);
  virtual ~DialogEnrollmentDelegate();

  // EnrollmentDelegate overrides
  virtual void Enroll(const std::vector<std::string>& uri_list,
                      const base::Closure& connect) OVERRIDE;

 private:
  gfx::NativeWindow owning_window_;
  std::string network_name_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DialogEnrollmentDelegate);
};

DialogEnrollmentDelegate::DialogEnrollmentDelegate(
    gfx::NativeWindow owning_window,
    const std::string& network_name,
    Profile* profile) : owning_window_(owning_window),
                        network_name_(network_name),
                        profile_(profile) {}

DialogEnrollmentDelegate::~DialogEnrollmentDelegate() {}

void DialogEnrollmentDelegate::Enroll(const std::vector<std::string>& uri_list,
                                      const base::Closure& connect) {
  // Keep the closure for later activation if we notice that
  // a certificate has been added.

  // TODO(gspencer): Do something smart with the closure.  At the moment it is
  // being ignored because we don't know when the enrollment tab is closed.
  // http://crosbug.com/30422
  for (std::vector<std::string>::const_iterator iter = uri_list.begin();
       iter != uri_list.end(); ++iter) {
    GURL uri(*iter);
    if (uri.IsStandard() || uri.scheme() == chrome::kExtensionScheme) {
      // If this is a "standard" scheme, like http, ftp, etc., then open that in
      // the enrollment dialog.
      EnrollmentDialogView::ShowDialog(owning_window_,
                                       network_name_,
                                       profile_,
                                       uri, connect);
      return;
    }
  }

  // If we didn't find a scheme we could handle, then don't continue connecting.
  // TODO(gspencer): provide a path to display this failure to the user. (but
  // for the most part they won't know what it means, since it's probably coming
  // from a policy-pushed ONC file).
  VLOG(1) << "Couldn't find usable scheme in enrollment URI(s)";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

EnrollmentDelegate* CreateEnrollmentDelegate(gfx::NativeWindow owning_window,
                                             const std::string& network_name,
                                             Profile* profile) {
  return new DialogEnrollmentDelegate(owning_window, network_name, profile);
}

}  // namespace chromeos
