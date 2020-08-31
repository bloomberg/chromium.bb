// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/echo_dialog_view.h"

#include <stddef.h>

#include "chrome/browser/chromeos/ui/echo_dialog_listener.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<views::ImageButton> CreateLearnMoreButton(
    views::ButtonListener* listener) {
  auto learn_more_button = views::CreateVectorImageButtonWithNativeTheme(
      listener, vector_icons::kHelpOutlineIcon);
  learn_more_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CHROMEOS_ACC_LEARN_MORE));
  learn_more_button->SetFocusForPlatform();
  return learn_more_button;
}

}  // namespace

namespace chromeos {

EchoDialogView::EchoDialogView(EchoDialogListener* listener,
                               const EchoDialogView::Params& params)
    : listener_(listener) {
  DCHECK(listener_);
  learn_more_button_ =
      DialogDelegate::SetExtraView(CreateLearnMoreButton(this));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ECHO);

  if (params.echo_enabled) {
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK |
                                ui::DIALOG_BUTTON_CANCEL);
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON));
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_CANCEL,
        l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON));
    InitForEnabledEcho(params.service_name, params.origin);
  } else {
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL);
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_CANCEL,
        l10n_util::GetStringUTF16(IDS_ECHO_CONSENT_DISMISS_BUTTON));
    InitForDisabledEcho();
  }

  DialogDelegate::SetAcceptCallback(base::BindOnce(
      &EchoDialogListener::OnAccept, base::Unretained(listener_)));
  DialogDelegate::SetCancelCallback(base::BindOnce(
      &EchoDialogListener::OnCancel, base::Unretained(listener_)));
}

EchoDialogView::~EchoDialogView() = default;

void EchoDialogView::Show(gfx::NativeWindow parent) {
  views::DialogDelegate::CreateDialogWidget(this, parent, parent);
  GetWidget()->SetSize(GetWidget()->GetRootView()->GetPreferredSize());
  GetWidget()->Show();
}

void EchoDialogView::InitForEnabledEcho(const base::string16& service_name,
                                        const base::string16& origin) {

  size_t offset;
  base::string16 text = l10n_util::GetStringFUTF16(IDS_ECHO_CONSENT_DIALOG_TEXT,
                                                   service_name, &offset);

  auto label = std::make_unique<views::StyledLabel>(text, nullptr);

  views::StyledLabel::RangeStyleInfo service_name_style;
  gfx::FontList font_list = label->GetDefaultFontList();
  service_name_style.custom_font =
      font_list.DeriveWithStyle(gfx::Font::UNDERLINE);
  service_name_style.tooltip = origin;
  label->AddStyleRange(gfx::Range(offset, offset + service_name.length()),
                       service_name_style);

  SetBorderAndLabel(std::move(label), font_list);
}

void EchoDialogView::InitForDisabledEcho() {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ECHO_DISABLED_CONSENT_DIALOG_TEXT));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // grab the font list before std::move(label) or it'll be nullptr
  gfx::FontList font_list = label->font_list();
  SetBorderAndLabel(std::move(label), font_list);
}

ui::ModalType EchoDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool EchoDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool EchoDialogView::ShouldShowCloseButton() const {
  return false;
}

gfx::Size EchoDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(
      default_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, default_width));
}

void EchoDialogView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  DCHECK(sender == learn_more_button_);
  listener_->OnMoreInfoLinkClicked();
}

void EchoDialogView::SetBorderAndLabel(std::unique_ptr<views::View> label,
                                       const gfx::FontList& label_font_list) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Without a title, top padding isn't correctly calculated.  This adds the
  // text's internal leading to the top padding.  See
  // FontList::DeriveWithHeightUpperBound() for font padding details.
  int top_inset_padding =
      label_font_list.GetBaseline() - label_font_list.GetCapHeight();

  gfx::Insets insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(views::TEXT,
                                                                  views::TEXT);
  insets += gfx::Insets(top_inset_padding, 0, 0, 0);
  SetBorder(views::CreateEmptyBorder(insets));

  AddChildView(std::move(label));
}

}  // namespace chromeos
