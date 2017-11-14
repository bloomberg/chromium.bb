// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/bookmark_app_confirmation_view.h"

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace {

// Minimum width of the the bubble.
const int kMinBubbleWidth = 300;

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

BookmarkAppConfirmationView::BookmarkAppConfirmationView(
    const WebApplicationInfo& web_app_info,
    chrome::ShowBookmarkAppDialogCallback callback)
    : web_app_info_(web_app_info),
      callback_(std::move(callback)),
      open_as_window_checkbox_(nullptr),
      title_tf_(nullptr) {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(views::CONTROL,
                                                             views::CONTROL));
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);
  const int column_set_id = 0;

  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0,
                               layout_provider->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  constexpr int textfield_width = 320;
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                        views::GridLayout::FIXED, textfield_width, 0);

  const int icon_size = layout_provider->IsHarmonyMode()
                            ? extension_misc::EXTENSION_ICON_SMALL
                            : extension_misc::EXTENSION_ICON_MEDIUM;
  views::ImageView* icon_image_view = new views::ImageView();
  gfx::Size image_size(icon_size, icon_size);
  gfx::ImageSkia image(
      base::MakeUnique<WebAppInfoImageSource>(icon_size, web_app_info_),
      image_size);
  icon_image_view->SetImageSize(image_size);
  icon_image_view->SetImage(image);
  layout->StartRow(0, column_set_id);
  layout->AddView(icon_image_view);

  title_tf_ = new views::Textfield();
  title_tf_->SetText(web_app_info_.title);
  title_tf_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL));
  title_tf_->set_controller(this);
  layout->AddView(title_tf_);

  layout->AddPaddingRow(
      0, layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));

  // When CanHostedAppsOpenInWindows() returns false, do not show the open as
  // window checkbox to avoid confusing users.
  // Desktop PWAs will choose the display mode automatically.
  if (extensions::util::CanHostedAppsOpenInWindows() &&
      !base::FeatureList::IsEnabled(features::kDesktopPWAWindowing)) {
    open_as_window_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW));
    open_as_window_checkbox_->SetChecked(web_app_info_.open_as_window);
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
    layout->AddView(open_as_window_checkbox_);
  }

  title_tf_->SelectAll(true);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::BOOKMARK_APP_CONFIRMATION);
}

views::View* BookmarkAppConfirmationView::GetInitiallyFocusedView() {
  return title_tf_;
}

ui::ModalType BookmarkAppConfirmationView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 BookmarkAppConfirmationView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ADD_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE);
}

bool BookmarkAppConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void BookmarkAppConfirmationView::WindowClosing() {
  if (!callback_.is_null())
    std::move(callback_).Run(false, web_app_info_);
}

bool BookmarkAppConfirmationView::Accept() {
  web_app_info_.title = GetTrimmedTitle();
  web_app_info_.open_as_window =
      open_as_window_checkbox_ && open_as_window_checkbox_->checked();
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
  gfx::Size size(views::DialogDelegateView::CalculatePreferredSize());
  size.SetToMax(gfx::Size(kMinBubbleWidth, 0));
  return size;
}

void BookmarkAppConfirmationView::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  DialogModelChanged();
}

base::string16 BookmarkAppConfirmationView::GetTrimmedTitle() const {
  base::string16 title(title_tf_->text());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}

namespace chrome {

void ShowBookmarkAppDialog(gfx::NativeWindow parent,
                           const WebApplicationInfo& web_app_info,
                           ShowBookmarkAppDialogCallback callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new BookmarkAppConfirmationView(web_app_info, std::move(callback)),
      parent)
      ->Show();
}

}  // namespace chrome
