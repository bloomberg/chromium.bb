// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/echo_dialog_view.h"

#include <stddef.h>

#include "chrome/browser/chromeos/ui/echo_dialog_listener.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

const int kDialogLabelTopInset = 20;
const int kDialogLabelLeftInset = 20;
const int kDialogLabelBottomInset = 20;
const int kDialogLabelRightInset = 100;

const int kDialogLabelPreferredWidth =
    350 + kDialogLabelLeftInset + kDialogLabelRightInset;

}  // namespace

namespace chromeos {

EchoDialogView::EchoDialogView(EchoDialogListener* listener)
    : label_(NULL),
      listener_(listener),
      ok_button_label_id_(0),
      cancel_button_label_id_(0) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ECHO);
}

EchoDialogView::~EchoDialogView() {}

void EchoDialogView::InitForEnabledEcho(const base::string16& service_name,
                                        const base::string16& origin) {
  ok_button_label_id_ = IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON;
  cancel_button_label_id_ = IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON;

  base::string16 link =
      l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_LABEL_LEARN_MORE);

  std::vector<size_t> offsets;
  base::string16 text = l10n_util::GetStringFUTF16(IDS_ECHO_CONSENT_DIALOG_TEXT,
                                             service_name,
                                             link,
                                             &offsets);

  label_ = new views::StyledLabel(text, this);

  views::StyledLabel::RangeStyleInfo service_name_style;
  service_name_style.custom_font =
      label_->GetDefaultFontList().DeriveWithStyle(gfx::Font::UNDERLINE);
  service_name_style.tooltip = origin;
  label_->AddStyleRange(
      gfx::Range(offsets[0], offsets[0] + service_name.length()),
      service_name_style);

  label_->AddStyleRange(gfx::Range(offsets[1], offsets[1] + link.length()),
                        views::StyledLabel::RangeStyleInfo::CreateForLink());

  SetLabelBorderAndBounds();

  AddChildView(label_);
}

void EchoDialogView::InitForDisabledEcho() {
  ok_button_label_id_ = 0;
  cancel_button_label_id_ = IDS_ECHO_CONSENT_DISMISS_BUTTON;

  base::string16 link =
      l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_LABEL_LEARN_MORE);

  size_t offset;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_ECHO_DISABLED_CONSENT_DIALOG_TEXT, link, &offset);

  label_ = new views::StyledLabel(text, this);
  label_->AddStyleRange(gfx::Range(offset, offset + link.length()),
                        views::StyledLabel::RangeStyleInfo::CreateForLink());

  SetLabelBorderAndBounds();

  AddChildView(label_);
}

void EchoDialogView::Show(gfx::NativeWindow parent) {
  DCHECK(cancel_button_label_id_);

  views::DialogDelegate::CreateDialogWidget(this, parent, parent);
  GetWidget()->SetSize(GetWidget()->GetRootView()->GetPreferredSize());
  GetWidget()->Show();
}

int EchoDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_NONE;
}

int EchoDialogView::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_NONE;
  if (ok_button_label_id_)
    buttons |= ui::DIALOG_BUTTON_OK;
  if (cancel_button_label_id_)
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  return buttons;
}

bool EchoDialogView::Accept() {
  if (listener_) {
    listener_->OnAccept();
    listener_ = NULL;
  }
  return true;
}

bool EchoDialogView::Cancel() {
  if (listener_) {
    listener_->OnCancel();
    listener_ = NULL;
  }
  return true;
}

base::string16 EchoDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK && ok_button_label_id_)
    return l10n_util::GetStringUTF16(ok_button_label_id_);
  if (button == ui::DIALOG_BUTTON_CANCEL && cancel_button_label_id_)
    return l10n_util::GetStringUTF16(cancel_button_label_id_);
  return base::string16();
}

ui::ModalType EchoDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool EchoDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool EchoDialogView::ShouldShowWindowIcon() const {
  return false;
}

void EchoDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                            const gfx::Range& range,
                                            int event_flags) {
  if (!listener_)
    return;
  listener_->OnMoreInfoLinkClicked();
}

gfx::Size EchoDialogView::CalculatePreferredSize() const {
  gfx::Size size =
      gfx::Size(kDialogLabelPreferredWidth,
                label_->GetHeightForWidth(kDialogLabelPreferredWidth));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void EchoDialogView::SetLabelBorderAndBounds() {
  label_->SetBorder(views::CreateEmptyBorder(
      kDialogLabelTopInset, kDialogLabelLeftInset, kDialogLabelBottomInset,
      kDialogLabelRightInset));

  label_->SetBounds(label_->x(),
                    label_->y(),
                    kDialogLabelPreferredWidth,
                    label_->GetHeightForWidth(kDialogLabelPreferredWidth));
}

}  // namespace chromeos
