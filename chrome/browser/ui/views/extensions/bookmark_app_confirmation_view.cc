// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/bookmark_app_confirmation_view.h"

#include "base/callback_helpers.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "grit/components_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// Minimum width of the the bubble.
const int kMinBubbleWidth = 300;
// Size of the icon.
const int kIconSize = extension_misc::EXTENSION_ICON_MEDIUM;

class WebAppInfoImageSource : public gfx::ImageSkiaSource {
 public:
  WebAppInfoImageSource(int dip_size, const WebApplicationInfo& info)
      : dip_size_(dip_size), info_(info) {}
  ~WebAppInfoImageSource() override {}

 private:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    int size = base::saturated_cast<int>(dip_size_ * scale);
    for (const auto& icon_info : info_.icons) {
      if (icon_info.width == size)
        return gfx::ImageSkiaRep(icon_info.data, scale);
    }
    return gfx::ImageSkiaRep();
  }

  int dip_size_;
  WebApplicationInfo info_;
};

}  // namespace

BookmarkAppConfirmationView::~BookmarkAppConfirmationView() {}

// static
void BookmarkAppConfirmationView::CreateAndShow(
    gfx::NativeWindow parent,
    const WebApplicationInfo& web_app_info,
    const BrowserWindow::ShowBookmarkAppBubbleCallback& callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new BookmarkAppConfirmationView(web_app_info, callback), parent)
      ->Show();
}

BookmarkAppConfirmationView::BookmarkAppConfirmationView(
    const WebApplicationInfo& web_app_info,
    const BrowserWindow::ShowBookmarkAppBubbleCallback& callback)
    : web_app_info_(web_app_info),
      callback_(callback),
      open_as_window_checkbox_(nullptr),
      title_tf_(nullptr) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, views::kButtonHEdgeMarginNew,
      views::kButtonHEdgeMarginNew, views::kButtonHEdgeMarginNew);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout);

  views::ImageView* icon_image_view = new views::ImageView();
  gfx::Size image_size(kIconSize, kIconSize);
  gfx::ImageSkia image(new WebAppInfoImageSource(kIconSize, web_app_info_),
                       image_size);
  icon_image_view->SetImageSize(image_size);
  icon_image_view->SetImage(image);
  AddChildView(icon_image_view);

  title_tf_ = new views::Textfield();
  title_tf_->SetText(web_app_info_.title);
  title_tf_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL));
  title_tf_->set_controller(this);
  AddChildView(title_tf_);
  layout->SetFlexForView(title_tf_, 1);

  title_tf_->SelectAll(true);
}

views::View* BookmarkAppConfirmationView::GetInitiallyFocusedView() {
  return title_tf_;
}

ui::ModalType BookmarkAppConfirmationView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 BookmarkAppConfirmationView::GetWindowTitle() const {
#if defined(USE_ASH)
  int ids = IDS_ADD_TO_SHELF_BUBBLE_TITLE;
#else
  int ids = IDS_ADD_TO_DESKTOP_BUBBLE_TITLE;
#endif

  return l10n_util::GetStringUTF16(ids);
}

bool BookmarkAppConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void BookmarkAppConfirmationView::WindowClosing() {
  if (!callback_.is_null())
    callback_.Run(false, web_app_info_);
}

views::View* BookmarkAppConfirmationView::CreateExtraView() {
  open_as_window_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW));
  open_as_window_checkbox_->SetChecked(web_app_info_.open_as_window);
  return open_as_window_checkbox_;
}

bool BookmarkAppConfirmationView::Accept() {
  web_app_info_.title = GetTrimmedTitle();
  web_app_info_.open_as_window = open_as_window_checkbox_->checked();
  base::ResetAndReturn(&callback_).Run(true, web_app_info_);
  return true;
}

base::string16 BookmarkAppConfirmationView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK ? IDS_ADD
                                                                  : IDS_CANCEL);
}

bool BookmarkAppConfirmationView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? !GetTrimmedTitle().empty() : true;
}

gfx::Size BookmarkAppConfirmationView::GetMinimumSize() const {
  gfx::Size size(views::DialogDelegateView::GetPreferredSize());
  size.SetToMax(gfx::Size(kMinBubbleWidth, 0));
  return size;
}

void BookmarkAppConfirmationView::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  GetDialogClientView()->UpdateDialogButtons();
}

base::string16 BookmarkAppConfirmationView::GetTrimmedTitle() const {
  base::string16 title(title_tf_->text());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}
