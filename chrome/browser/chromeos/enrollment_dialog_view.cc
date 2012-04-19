// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enrollment_dialog_view.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
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
const int kDefaultWidth = 640;
const int kDefaultHeight = 480;

// Border around the WebView
const int kWebViewBorderSize = 1;

// TODO(gspencer): Move this into ui/views/layout, perhaps just adding insets
// to FillLayout.
class BorderLayout : public LayoutManager {
 public:
  explicit BorderLayout(const gfx::Insets& insets) : insets_(insets) {}
  virtual ~BorderLayout() {}

  // Overridden from LayoutManager:
  virtual void Layout(View* host) OVERRIDE {
    if (!host->has_children())
      return;

    View* frame_view = host->child_at(0);
    frame_view->SetBounds(insets_.left(),
                          insets_.top(),
                          host->width() - insets_.right() - insets_.left(),
                          host->height() - insets_.bottom() - insets_.top());
  }

  virtual gfx::Size GetPreferredSize(View* host) OVERRIDE {
    DCHECK_EQ(1, host->child_count());
    gfx::Size child_size = host->child_at(0)->GetPreferredSize();
    child_size.Enlarge(insets_.left() + insets_.right(),
                       insets_.top() + insets_.bottom());
    return child_size;
  }

 private:
  gfx::Insets insets_;
  DISALLOW_COPY_AND_ASSIGN(BorderLayout);
};

// Handler for certificate enrollment. This displays the content from the
// certificate enrollment URI and listens for a certificate to be added. If a
// certificate is added, then it invokes the closure to allow the network to
// continue to connect.
class DialogEnrollmentDelegate : public EnrollmentDelegate {
 public:
  // |owning_window| is the window that will own the dialog.
  explicit DialogEnrollmentDelegate(gfx::NativeWindow owning_window);
  virtual ~DialogEnrollmentDelegate();

  // EnrollmentDelegate overrides
  virtual void Enroll(const std::vector<std::string>& uri_list,
                      const base::Closure& connect) OVERRIDE;

 private:
  gfx::NativeWindow owning_window_;

  DISALLOW_COPY_AND_ASSIGN(DialogEnrollmentDelegate);
};

DialogEnrollmentDelegate::DialogEnrollmentDelegate(
    gfx::NativeWindow owning_window)
    : owning_window_(owning_window) {}

DialogEnrollmentDelegate::~DialogEnrollmentDelegate() {}

void DialogEnrollmentDelegate::Enroll(const std::vector<std::string>& uri_list,
                                      const base::Closure& connect) {
  // Keep the closure for later activation if we notice that
  // a certificate has been added.
  for (std::vector<std::string>::const_iterator iter = uri_list.begin();
       iter != uri_list.end(); ++iter) {
    GURL uri(*iter);
    if (uri.IsStandard() || uri.scheme() == "chrome-extension") {
      // If this is a "standard" scheme, like http, ftp, etc., then open that in
      // the enrollment dialog. If this is a chrome extension, then just open
      // that extension in a tab and let it provide any UI that it needs to
      // complete.
      EnrollmentDialogView::ShowDialog(owning_window_, uri, connect);
      return;
    }
  }
  // If we didn't find a scheme we could handle, then don't continue connecting.
  // TODO(gspencer): provide a path to display this failure to the user.
  VLOG(1) << "Couldn't find usable scheme in enrollment URI(s)";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// EnrollmentDialogView implementation.

EnrollmentDialogView::EnrollmentDialogView(const GURL& target_uri,
                                           const base::Closure& connect)
    : target_uri_(target_uri),
      connect_(connect),
      added_cert_(false) {
  net::CertDatabase::AddObserver(this);
}

EnrollmentDialogView::~EnrollmentDialogView() {
  net::CertDatabase::RemoveObserver(this);
}

// static
EnrollmentDelegate* EnrollmentDialogView::CreateEnrollmentDelegate(
    gfx::NativeWindow owning_window) {
  return new DialogEnrollmentDelegate(owning_window);
}

// static
void EnrollmentDialogView::ShowDialog(gfx::NativeWindow owning_window,
                                      const GURL& target_uri,
                                      const base::Closure& connect) {
  EnrollmentDialogView* dialog_view =
      new EnrollmentDialogView(target_uri, connect);
  views::Widget::CreateWindowWithParent(dialog_view, owning_window);
  dialog_view->InitDialog();
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

void EnrollmentDialogView::Close() {
  GetWidget()->Close();
}

int EnrollmentDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

void EnrollmentDialogView::OnClose() {
  if (added_cert_)
    connect_.Run();
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

void EnrollmentDialogView::OnUserCertAdded(
    const net::X509Certificate* cert) {
  added_cert_ = true;
  Close();
}

void EnrollmentDialogView::InitDialog() {
  added_cert_ = false;
  // Create the views and layout manager and set them up.
  Label* label = new Label(
      l10n_util::GetStringUTF16(
          IDS_NETWORK_ENROLLMENT_HANDLER_EMBEDDED_ENROLL));
  label->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
  label->SetHorizontalAlignment(Label::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);

  // In order to get a border that shows around the WebView, we need to embed it
  // into another view because it hosts a native window that fills the view.
  View* web_view_border_view = new View();
  web_view_border_view->SetLayoutManager(
      new BorderLayout(gfx::Insets(kWebViewBorderSize,
                                   kWebViewBorderSize,
                                   kWebViewBorderSize,
                                   kWebViewBorderSize)));
  SkColor frame_color = ThemeService::GetDefaultColor(
      ThemeService::COLOR_FRAME);
  Border* border = views::Border::CreateSolidBorder(kWebViewBorderSize,
                                                    frame_color);
  web_view_border_view->set_border(border);
  views::WebView* web_view =
      new views::WebView(ProfileManager::GetLastUsedProfile());
  web_view_border_view->AddChildView(web_view);
  web_view->SetVisible(true);

  GridLayout* grid_layout = GridLayout::CreatePanel(this);
  SetLayoutManager(grid_layout);

  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL,  // Horizontal resize.
                     views::GridLayout::FILL,  // Vertical resize.
                     1,   // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,   // Ignored for USE_PREF.
                     0);  // Minimum size.
  // Add a column set for aligning the text when it has no icons (such as the
  // help center link).
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
  grid_layout->StartRow(100.0f, 0);
  grid_layout->AddView(web_view_border_view);
  grid_layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  grid_layout->Layout(this);
  web_view->LoadInitialURL(target_uri_);
}

}  // namespace chromeos
