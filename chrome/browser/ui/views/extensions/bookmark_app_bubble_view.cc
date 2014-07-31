// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/bookmark_app_bubble_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/app_icon_loader_impl.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::ColumnSet;
using views::GridLayout;

namespace {

// Minimum width of the the bubble.
const int kMinBubbleWidth = 300;
// Minimum width of the the textfield.
const int kMinTextfieldWidth = 200;
// Size of the icon.
const int kIconSize = extension_misc::EXTENSION_ICON_MEDIUM;

ExtensionService* GetExtensionService(Profile* profile) {
  return extensions::ExtensionSystem::Get(profile)->extension_service();
}

}  // namespace

BookmarkAppBubbleView* BookmarkAppBubbleView::bookmark_app_bubble_ = NULL;

BookmarkAppBubbleView::~BookmarkAppBubbleView() {
}

// static
void BookmarkAppBubbleView::ShowBubble(views::View* anchor_view,
                                       Profile* profile,
                                       const WebApplicationInfo& web_app_info,
                                       const std::string& extension_id) {
  if (bookmark_app_bubble_ != NULL)
    return;

  bookmark_app_bubble_ = new BookmarkAppBubbleView(
      anchor_view, profile, web_app_info, extension_id);
  views::BubbleDelegateView::CreateBubble(bookmark_app_bubble_)->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_app_bubble_->title_tf_->SelectAll(true);
  bookmark_app_bubble_->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
}

BookmarkAppBubbleView::BookmarkAppBubbleView(
    views::View* anchor_view,
    Profile* profile,
    const WebApplicationInfo& web_app_info,
    const std::string& extension_id)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      profile_(profile),
      web_app_info_(web_app_info),
      extension_id_(extension_id),
      add_button_(NULL),
      cancel_button_(NULL),
      open_as_tab_checkbox_(NULL),
      title_tf_(NULL),
      remove_app_(true),
      app_icon_loader_(new extensions::AppIconLoaderImpl(profile,
                                                         kIconSize,
                                                         this)) {
  const SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  set_arrow(views::BubbleBorder::TOP_CENTER);
  set_color(background_color);
  set_background(views::Background::CreateSolidBackground(background_color));
  set_margins(gfx::Insets(views::kPanelVertMargin, 0, 0, 0));
}

void BookmarkAppBubbleView::Init() {
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_TITLE));
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::MediumFont));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  add_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_ADD));
  add_button_->SetStyle(views::Button::STYLE_BUTTON);
  add_button_->SetIsDefault(true);

  cancel_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Column sets used in the layout of the bubble.
  enum ColumnSetID {
    TITLE_COLUMN_SET_ID,
    TITLE_TEXT_COLUMN_SET_ID,
    CONTENT_COLUMN_SET_ID
  };

  // The column layout used for the title and checkbox.
  ColumnSet* cs = layout->AddColumnSet(TITLE_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // The column layout used for the icon and text box.
  cs = layout->AddColumnSet(TITLE_TEXT_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::LEADING,
                GridLayout::CENTER,
                0,
                GridLayout::USE_PREF,
                0,
                0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::FILL,
                GridLayout::CENTER,
                1,
                GridLayout::USE_PREF,
                0,
                kMinTextfieldWidth);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // The column layout used for the row with buttons.
  cs = layout->AddColumnSet(CONTENT_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  layout->StartRow(0, TITLE_COLUMN_SET_ID);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id_, extensions::ExtensionRegistry::EVERYTHING);

  layout->StartRow(0, TITLE_TEXT_COLUMN_SET_ID);
  icon_image_view_ = new views::ImageView();
  icon_image_view_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  layout->AddView(icon_image_view_);
  app_icon_loader_->FetchImage(extension_id_);

  title_tf_ = new views::Textfield();
  title_tf_->SetText(extension ? base::UTF8ToUTF16(extension->name())
                               : web_app_info_.title);
  layout->AddView(title_tf_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, CONTENT_COLUMN_SET_ID);
  open_as_tab_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TAB));
  open_as_tab_checkbox_->SetChecked(
      profile_->GetPrefs()->GetInteger(
          extensions::pref_names::kBookmarkAppCreationLaunchType) ==
              extensions::LAUNCH_TYPE_REGULAR);
  layout->AddView(open_as_tab_checkbox_);
  layout->AddView(add_button_);
  layout->AddView(cancel_button_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

views::View* BookmarkAppBubbleView::GetInitiallyFocusedView() {
  return title_tf_;
}

void BookmarkAppBubbleView::WindowClosing() {
  // We have to reset |bookmark_app_bubble_| here, not in our destructor,
  // because we'll be destroyed asynchronously and the shown state will be
  // checked before then.
  DCHECK_EQ(bookmark_app_bubble_, this);
  bookmark_app_bubble_ = NULL;

  if (remove_app_) {
    GetExtensionService(profile_)
        ->UninstallExtension(extension_id_,
                             extensions::UNINSTALL_REASON_INSTALL_CANCELED,
                             base::Bind(&base::DoNothing),
                             NULL);
  } else {
    ApplyEdits();
  }
}

bool BookmarkAppBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN) {
    HandleButtonPressed(add_button_);
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size BookmarkAppBubbleView::GetMinimumSize() const {
  gfx::Size size(views::BubbleDelegateView::GetPreferredSize());
  size.SetToMax(gfx::Size(kMinBubbleWidth, 0));
  return size;
}

void BookmarkAppBubbleView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  HandleButtonPressed(sender);
}

void BookmarkAppBubbleView::SetAppImage(const std::string& id,
                                        const gfx::ImageSkia& image) {
  DCHECK_EQ(extension_id_, id);
  icon_image_view_->SetImage(image);
}

void BookmarkAppBubbleView::HandleButtonPressed(views::Button* sender) {
  // Unset |remove_app_| so we don't delete the bookmark after the window
  // closes.
  if (sender == add_button_)
    remove_app_ = false;

  GetWidget()->Close();
}

void BookmarkAppBubbleView::ApplyEdits() {
  // Set the launch type based on the checkbox.
  extensions::LaunchType launch_type = open_as_tab_checkbox_->checked()
      ? extensions::LAUNCH_TYPE_REGULAR
      : extensions::LAUNCH_TYPE_WINDOW;
  profile_->GetPrefs()->SetInteger(
          extensions::pref_names::kBookmarkAppCreationLaunchType, launch_type);
  extensions::SetLaunchType(GetExtensionService(profile_),
                            extension_id_,
                            launch_type);

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id_, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && base::UTF8ToUTF16(extension->name()) == title_tf_->text())
    return;

  // Reinstall the app with an updated name.
  WebApplicationInfo install_info(web_app_info_);
  install_info.title = title_tf_->text();

  extensions::CreateOrUpdateBookmarkApp(GetExtensionService(profile_),
                                        install_info);
}
