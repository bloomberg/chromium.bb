// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <stddef.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/string_compare.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

views::Link* CreateLink(views::LinkListener* listener,
                        int resource_id,
                        int id) {
  views::Link* link = new views::Link(l10n_util::GetStringUTF16(resource_id));
  link->set_listener(listener);
  link->set_id(id);
  return link;
}

// TODO(ftang) Restore icons in CreateViewAfterTranslate and CreateViewError
// without causing layout issues; see http://crbug.com/610351
void AddIconToLayout(views::GridLayout* layout) {
  views::ImageView* icon = new views::ImageView();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  icon->SetImage(bundle.GetImageNamed(IDR_TRANSLATE_BUBBLE_ICON).ToImageSkia());
  layout->AddView(icon);
}

bool Use2016Q2UI() {
  return base::FeatureList::IsEnabled(translate::kTranslateUI2016Q2);
}

}  // namespace

// static
TranslateBubbleView* TranslateBubbleView::translate_bubble_view_ = NULL;

TranslateBubbleView::~TranslateBubbleView() {
  // A child view could refer to a model which is owned by this class when
  // the child view is destructed. For example, |source_language_combobx_model_|
  // is referred by Combobox's destructor. Before destroying the models,
  // removing the child views is needed.
  RemoveAllChildViews(true);
}

// static
views::Widget* TranslateBubbleView::ShowBubble(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    DisplayReason reason) {
  if (translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (translate_bubble_view_->web_contents() == web_contents &&
        translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_ADVANCED) {
      return nullptr;
    }
    if (step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
      TranslateBubbleModel::ViewState state =
          TranslateBubbleModelImpl::TranslateStepToViewState(step);
      translate_bubble_view_->SwitchView(state);
    } else {
      translate_bubble_view_->SwitchToErrorView(error_type);
    }
    return nullptr;
  } else {
    if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE &&
        reason == AUTOMATIC) {
      return nullptr;
    }
  }

  std::string source_language;
  std::string target_language;
  ChromeTranslateClient::GetTranslateLanguages(web_contents, &source_language,
                                               &target_language);

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      new translate::TranslateUIDelegate(
          ChromeTranslateClient::GetManagerFromWebContents(web_contents)
              ->GetWeakPtr(),
          source_language, target_language));
  std::unique_ptr<TranslateBubbleModel> model(
      new TranslateBubbleModelImpl(step, std::move(ui_delegate)));
  TranslateBubbleView* view = new TranslateBubbleView(
      anchor_view, anchor_point, std::move(model), error_type, web_contents);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(view);
  view->ShowForReason(reason);
  translate::ReportUiAction(translate::BUBBLE_SHOWN);
  return bubble_widget;
}

// static
void TranslateBubbleView::CloseCurrentBubble() {
  if (translate_bubble_view_)
    translate_bubble_view_->CloseBubble();
}

// static
TranslateBubbleView* TranslateBubbleView::GetCurrentBubble() {
  return translate_bubble_view_;
}

void TranslateBubbleView::CloseBubble() {
  mouse_handler_.reset();
  LocationBarBubbleDelegateView::CloseBubble();
}

int TranslateBubbleView::GetDialogButtons() const {
  // TODO(estade): this should be using GetDialogButtons().
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 TranslateBubbleView::GetWindowTitle() const {
  // The 2016Q2 UI doesn't use a window title.
  if (Use2016Q2UI())
    return base::string16();
  int id = 0;
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      id = IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      id = IDS_TRANSLATE_BUBBLE_TRANSLATING;
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      id = IDS_TRANSLATE_BUBBLE_TRANSLATED_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      id = IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED:
      id = IDS_TRANSLATE_BUBBLE_ADVANCED_TITLE;
      break;
  }

  return l10n_util::GetStringUTF16(id);
}

void TranslateBubbleView::Init() {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  should_always_translate_ = model_->ShouldAlwaysTranslate();
  if (Use2016Q2UI() && !is_in_incognito_window_) {
    should_always_translate_ =
        model_->ShouldAlwaysTranslateBeCheckedByDefault();
  }

  before_translate_view_ = CreateViewBeforeTranslate();
  translating_view_ = CreateViewTranslating();
  after_translate_view_ = CreateViewAfterTranslate();
  error_view_ = CreateViewError();
  advanced_view_ = CreateViewAdvanced();

  AddChildView(before_translate_view_);
  AddChildView(translating_view_);
  AddChildView(after_translate_view_);
  AddChildView(error_view_);
  AddChildView(advanced_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();

  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ERROR)
    model_->ShowError(error_type_);
}

void TranslateBubbleView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  HandleButtonPressed(static_cast<ButtonID>(sender->id()));
}

views::View* TranslateBubbleView::GetInitiallyFocusedView() {
  return GetCurrentView()->GetNextFocusableView();
}

bool TranslateBubbleView::ShouldShowCloseButton() const {
  return true;
}

void TranslateBubbleView::WindowClosing() {
  // The operations for |model_| are valid only when a WebContents is alive.
  // TODO(hajimehoshi): TranslateBubbleViewModel(Impl) should not hold a
  // WebContents as a member variable because the WebContents might be destroyed
  // while the TranslateBubbleViewModel(Impl) is still alive. Instead,
  // TranslateBubbleViewModel should take a reference of a WebContents at each
  // method. (crbug/320497)
  if (web_contents())
    model_->OnBubbleClosing();

  // We have to reset |translate_bubble_view_| here, not in our destructor,
  // because we'll be destroyed asynchronously and the shown state will be
  // checked before then.
  DCHECK_EQ(translate_bubble_view_, this);
  translate_bubble_view_ = NULL;
}

bool TranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        HandleButtonPressed(BUTTON_ID_TRANSLATE);
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        HandleButtonPressed(BUTTON_ID_SHOW_ORIGINAL);
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      break;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        HandleButtonPressed(BUTTON_ID_DONE);
        return true;
      }
      break;
    }
  }
  return BubbleDialogDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size TranslateBubbleView::CalculatePreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    width = std::max(width, child->GetPreferredSize().width());
  }
  int height = GetCurrentView()->GetPreferredSize().height();
  return gfx::Size(width, height);
}

void TranslateBubbleView::OnPerformAction(views::Combobox* combobox) {
  HandleComboboxPerformAction(static_cast<ComboboxID>(combobox->id()));
}

void TranslateBubbleView::LinkClicked(views::Link* source, int event_flags) {
  HandleLinkClicked(static_cast<LinkID>(source->id()));
}

void TranslateBubbleView::OnMenuButtonClicked(views::MenuButton* source,
                                              const gfx::Point& point,
                                              const ui::Event* event) {
  if (!options_menu_runner_) {
    options_menu_model_.reset(new ui::SimpleMenuModel(this));

    options_menu_model_->AddCheckItem(
        DenialMenuItem::ALWAYS_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));

    options_menu_model_->AddItem(
        DenialMenuItem::NEVER_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));

    // TODO(crbug.com/793925): Blacklisting should probably not be possible in
    // incognito mode as it leaves a trace of the user.
    if (model_->CanBlacklistSite()) {
      if (Use2016Q2UI()) {
        options_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      }

      options_menu_model_->AddItemWithStringId(
          DenialMenuItem::NEVER_TRANSLATE_SITE,
          IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE);
    }

    if (!Use2016Q2UI()) {
      options_menu_model_->AddItemWithStringId(
          DenialMenuItem::MORE_OPTIONS,
          IDS_TRANSLATE_BUBBLE_ADVANCED_MENU_BUTTON);
    }

    options_menu_runner_.reset(
        new views::MenuRunner(options_menu_model_.get(), 0));
  }
  gfx::Rect screen_bounds = source->GetBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), source, screen_bounds,
                                  views::MENU_ANCHOR_TOPRIGHT,
                                  ui::MENU_SOURCE_MOUSE);
}

bool TranslateBubbleView::IsCommandIdChecked(int command_id) const {
  DCHECK_EQ(DenialMenuItem::ALWAYS_TRANSLATE_LANGUAGE, command_id);
  return should_always_translate_;
}

bool TranslateBubbleView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TranslateBubbleView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case DenialMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      should_always_translate_ = !should_always_translate_;
      model_->SetAlwaysTranslate(should_always_translate_);

      if (should_always_translate_) {
        model_->Translate();
        SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
      }
      break;

    case DenialMenuItem::NEVER_TRANSLATE_LANGUAGE:
      translate::ReportUiAction(
          translate::NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED);
      model_->SetNeverTranslateLanguage(true);
      model_->DeclineTranslation();
      GetWidget()->Close();
      break;
    case DenialMenuItem::NEVER_TRANSLATE_SITE:
      translate::ReportUiAction(translate::NEVER_TRANSLATE_SITE_MENU_CLICKED);
      model_->SetNeverTranslateSite(true);
      model_->DeclineTranslation();
      GetWidget()->Close();
      break;
    case DenialMenuItem::MORE_OPTIONS:
      translate::ReportUiAction(translate::ADVANCED_MENU_CLICKED);
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      break;
    default:
      NOTREACHED();
  }
}

void TranslateBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
  translate::ReportUiAction(translate::ADVANCED_LINK_CLICKED);
}

void TranslateBubbleView::WebContentsDestroyed() {
  GetWidget()->CloseNow();
}

void TranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (GetBubbleFrameView()->close_button_clicked()) {
    model_->DeclineTranslation();
    translate::ReportUiAction(translate::CLOSE_BUTTON_CLICKED);
  }
}

TranslateBubbleModel::ViewState TranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

TranslateBubbleView::TranslateBubbleView(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    std::unique_ptr<TranslateBubbleModel> model,
    translate::TranslateErrors::Type error_type,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      WebContentsObserver(web_contents),
      before_translate_view_(NULL),
      translating_view_(NULL),
      after_translate_view_(NULL),
      error_view_(NULL),
      advanced_view_(NULL),
      source_language_combobox_(NULL),
      target_language_combobox_(NULL),
      before_always_translate_checkbox_(NULL),
      advanced_always_translate_checkbox_(NULL),
      advanced_cancel_button_(NULL),
      advanced_done_button_(NULL),
      options_menu_button_(NULL),
      model_(std::move(model)),
      error_type_(error_type),
      is_in_incognito_window_(
          web_contents && web_contents->GetBrowserContext()->IsOffTheRecord()),
      should_always_translate_(false) {
  translate_bubble_view_ = this;
  if (web_contents)  // web_contents can be null in unit_tests.
    mouse_handler_.reset(new WebContentMouseHandler(this, web_contents));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TRANSLATE);
}

views::View* TranslateBubbleView::GetCurrentView() const {
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      return before_translate_view_;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      return translating_view_;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      return after_translate_view_;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      return error_view_;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED:
      return advanced_view_;
  }
  NOTREACHED();
  return NULL;
}

void TranslateBubbleView::HandleButtonPressed(
    TranslateBubbleView::ButtonID sender_id) {
  switch (sender_id) {
    case BUTTON_ID_TRANSLATE: {
      model_->SetAlwaysTranslate(should_always_translate_);
      model_->Translate();
      translate::ReportUiAction(translate::TRANSLATE_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_DONE: {
      model_->SetAlwaysTranslate(should_always_translate_);
      if (model_->IsPageTranslatedInCurrentLanguages()) {
        model_->GoBackFromAdvanced();
        UpdateChildVisibilities();
        SizeToContents();
      } else {
        model_->Translate();
        SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
      }
      translate::ReportUiAction(translate::DONE_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_CANCEL: {
      model_->GoBackFromAdvanced();
      UpdateChildVisibilities();
      SizeToContents();
      translate::ReportUiAction(translate::CANCEL_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_TRY_AGAIN: {
      model_->Translate();
      translate::ReportUiAction(translate::TRY_AGAIN_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_SHOW_ORIGINAL: {
      model_->RevertTranslation();
      GetWidget()->Close();
      translate::ReportUiAction(translate::SHOW_ORIGINAL_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_ALWAYS_TRANSLATE: {
      views::Checkbox* always_checkbox = GetAlwaysTranslateCheckbox();
      DCHECK(always_checkbox);
      should_always_translate_ = always_checkbox->checked();
      translate::ReportUiAction(should_always_translate_
                                    ? translate::ALWAYS_TRANSLATE_CHECKED
                                    : translate::ALWAYS_TRANSLATE_UNCHECKED);
      break;
    }
    case BUTTON_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      translate::ReportUiAction(translate::ADVANCED_BUTTON_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::HandleLinkClicked(
    TranslateBubbleView::LinkID sender_id) {
  switch (sender_id) {
    case LINK_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      translate::ReportUiAction(translate::ADVANCED_LINK_CLICKED);
      break;
    }
    case LINK_ID_LANGUAGE_SETTINGS: {
      GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
      web_contents()->OpenURL(content::OpenURLParams(
          url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      translate::ReportUiAction(translate::SETTINGS_LINK_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::HandleComboboxPerformAction(
    TranslateBubbleView::ComboboxID sender_id) {
  switch (sender_id) {
    case COMBOBOX_ID_SOURCE_LANGUAGE: {
      if (model_->GetOriginalLanguageIndex() ==
          source_language_combobox_->selected_index()) {
        break;
      }
      model_->UpdateOriginalLanguageIndex(
          source_language_combobox_->selected_index());
      UpdateAdvancedView();
      translate::ReportUiAction(translate::SOURCE_LANGUAGE_MENU_CLICKED);
      break;
    }
    case COMBOBOX_ID_TARGET_LANGUAGE: {
      if (model_->GetTargetLanguageIndex() ==
          target_language_combobox_->selected_index()) {
        break;
      }
      model_->UpdateTargetLanguageIndex(
          target_language_combobox_->selected_index());
      UpdateAdvancedView();
      translate::ReportUiAction(translate::TARGET_LANGUAGE_MENU_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::UpdateChildVisibilities() {
  // Update the state of the always translate checkbox
  if (advanced_always_translate_checkbox_)
    advanced_always_translate_checkbox_->SetChecked(should_always_translate_);
  if (before_always_translate_checkbox_)
    before_always_translate_checkbox_->SetChecked(should_always_translate_);
  for (int i = 0; i < child_count(); i++) {
    views::View* view = child_at(i);
    view->SetVisible(view == GetCurrentView());
  }
  if (!Use2016Q2UI() && GetWidget())
    GetWidget()->UpdateWindowTitle();
  // BoxLayout only considers visible children, so ensure any newly visible
  // child views are positioned correctly.
  Layout();
}

views::View* TranslateBubbleView::CreateViewBeforeTranslate() {
  const int kQuestionWidth = 200;

  base::string16 original_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  if (original_language_name.empty()) {
    original_language_name =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_UNKNOWN_LANGUAGE);
  }

  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int button_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  if (Use2016Q2UI()) {
    // Add padding between the icon and the text.
    cs->AddPaddingColumn(0, provider->GetDistanceMetric(
                                views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  } else {
    // Add padding between the text and the link.
    cs->AddPaddingColumn(0, button_spacing);
  }
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, button_spacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  if (Use2016Q2UI()) {
    AddIconToLayout(layout);

    base::string16 target_language_name =
        model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());
    std::vector<size_t> offsets;
    auto* styled_label = new views::StyledLabel(
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE_NEW,
                                   original_language_name, target_language_name,
                                   &offsets),
        this);
    auto style_info = views::StyledLabel::RangeStyleInfo::CreateForLink();
    styled_label->AddStyleRange(
        gfx::Range(static_cast<uint32_t>(offsets[0]),
                   static_cast<uint32_t>(offsets[0] +
                                         original_language_name.length())),
        style_info);
    styled_label->AddStyleRange(
        gfx::Range(
            static_cast<uint32_t>(offsets[1]),
            static_cast<uint32_t>(offsets[1] + target_language_name.length())),
        style_info);
    styled_label->SizeToFit(kQuestionWidth);
    layout->AddView(styled_label);
  }

  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // In an incognito window, the "Always translate" checkbox shouldn't be shown.
  if (Use2016Q2UI() && !is_in_incognito_window_) {
    layout->AddPaddingRow(0, vertical_spacing);
    layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
    layout->SkipColumns(1);
    before_always_translate_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS_DO_THIS));
    before_always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    before_always_translate_checkbox_->set_listener(this);
    layout->AddView(before_always_translate_checkbox_);
  }

  layout->AddPaddingRow(0, vertical_spacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* accept_button =
      Use2016Q2UI()
          ? views::MdTextButton::CreateSecondaryUiBlueButton(
                this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT))
          : views::MdTextButton::CreateSecondaryUiButton(
                this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT));
  accept_button->set_id(BUTTON_ID_TRANSLATE);
  layout->AddView(accept_button);
  accept_button->SetIsDefault(true);
  const bool show_dropdown_arrow = Use2016Q2UI();
  options_menu_button_ = new views::MenuButton(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON), this,
      show_dropdown_arrow);
  options_menu_button_->SetStyleDeprecated(views::Button::STYLE_BUTTON);
  layout->AddView(options_menu_button_);

  return view;
}

views::View* TranslateBubbleView::CreateViewTranslating() {
  base::string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());

  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  if (Use2016Q2UI()) {
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  views::GridLayout::USE_PREF, 0, 0);
    // Add padding between the icon and the text.
    cs->AddPaddingColumn(0, provider->GetDistanceMetric(
                                views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  }
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  if (Use2016Q2UI()) {
    layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
    AddIconToLayout(layout);
    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATING));
    layout->AddView(label);
  } else {
    cs->AddPaddingColumn(0, provider->GetDistanceMetric(
                                views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  GridLayout::USE_PREF, 0, 0);
  }

  layout->AddPaddingRow(0, provider->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* revert_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  revert_button->set_id(BUTTON_ID_SHOW_ORIGINAL);
  revert_button->SetEnabled(false);
  layout->AddView(revert_button);
  if (!Use2016Q2UI()) {
    views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
        this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_BUTTON));
    button->set_id(BUTTON_ID_ADVANCED);
    button->SetEnabled(false);
    layout->AddView(button);
  }

  return view;
}

views::View* TranslateBubbleView::CreateViewAfterTranslate() {
  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  const int button_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  // TODO(ftang) Restore icon without causing layout defects: crbug.com/610351

  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, button_spacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  if (Use2016Q2UI()) {
    layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATED));
    layout->AddView(label);
    layout->AddView(
        CreateLink(this, IDS_TRANSLATE_BUBBLE_ADVANCED_LINK, LINK_ID_ADVANCED));
  } else {
    cs->AddPaddingColumn(0, button_spacing);
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  GridLayout::USE_PREF, 0, 0);
  }

  layout->AddPaddingRow(0, provider->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  button->set_id(BUTTON_ID_SHOW_ORIGINAL);
  layout->AddView(button);
  if (!Use2016Q2UI()) {
    views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
        this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_BUTTON));
    button->set_id(BUTTON_ID_ADVANCED);
    layout->AddView(button);
  }

  return view;
}

views::View* TranslateBubbleView::CreateViewError() {
  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  const int button_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // TODO(ftang) Restore icon without causing layout defects: crbug.com/610351

  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, button_spacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  if (Use2016Q2UI()) {
    layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE));
    layout->AddView(label);
    layout->AddView(
        CreateLink(this, IDS_TRANSLATE_BUBBLE_ADVANCED_LINK, LINK_ID_ADVANCED));
  } else {
    cs->AddPaddingColumn(0, button_spacing);
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  GridLayout::USE_PREF, 0, 0);
  }

  layout->AddPaddingRow(0, provider->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  button->set_id(BUTTON_ID_TRY_AGAIN);
  layout->AddView(button);
  if (!Use2016Q2UI()) {
    views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
        this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_BUTTON));
    button->set_id(BUTTON_ID_ADVANCED);
    layout->AddView(button);
  }

  return view;
}

// TODO(hajimehoshi): Revice this later to show a specific message for each
// error. (crbug/307350)
views::View* TranslateBubbleView::CreateViewAdvanced() {
  views::Label* source_language_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_PAGE_LANGUAGE));

  views::Label* target_language_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATION_LANGUAGE));

  int source_default_index = model_->GetOriginalLanguageIndex();
  source_language_combobox_model_.reset(
      new LanguageComboboxModel(source_default_index, model_.get()));
  source_language_combobox_ =
      new views::Combobox(source_language_combobox_model_.get());

  source_language_combobox_->set_id(COMBOBOX_ID_SOURCE_LANGUAGE);
  source_language_combobox_->set_listener(this);

  int target_default_index = model_->GetTargetLanguageIndex();
  target_language_combobox_model_.reset(
      new LanguageComboboxModel(target_default_index, model_.get()));
  target_language_combobox_ =
      new views::Combobox(target_language_combobox_model_.get());

  target_language_combobox_->set_id(COMBOBOX_ID_TARGET_LANGUAGE);
  target_language_combobox_->set_listener(this);

  // In an incognito window, "Always translate" checkbox shouldn't be shown.
  if (!is_in_incognito_window_) {
    advanced_always_translate_checkbox_ = new views::Checkbox(base::string16());
    advanced_always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    advanced_always_translate_checkbox_->set_listener(this);
  }

  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_BUTTONS,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(
      1, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(source_language_label);
  layout->AddView(source_language_combobox_);

  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->AddPaddingRow(0, vertical_spacing);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(target_language_label);
  layout->AddView(target_language_combobox_);

  if (!is_in_incognito_window_) {
    layout->AddPaddingRow(0, vertical_spacing);
    layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
    layout->SkipColumns(1);
    layout->AddView(advanced_always_translate_checkbox_);
  }

  layout->AddPaddingRow(0, vertical_spacing);

  layout->StartRow(0, COLUMN_SET_ID_BUTTONS);
  // TODO(estade): this should use CreateExtraView().
  layout->AddView(CreateLink(this, IDS_TRANSLATE_BUBBLE_LANGUAGE_SETTINGS,
                             LINK_ID_LANGUAGE_SETTINGS));
  advanced_done_button_ =
      Use2016Q2UI() ? views::MdTextButton::CreateSecondaryUiBlueButton(
                          this, l10n_util::GetStringUTF16(IDS_DONE))
                    : views::MdTextButton::CreateSecondaryUiButton(
                          this, l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button_->set_id(BUTTON_ID_DONE);
  advanced_done_button_->SetIsDefault(true);
  advanced_cancel_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL));
  advanced_cancel_button_->set_id(BUTTON_ID_CANCEL);
  layout->AddView(advanced_done_button_);
  layout->AddView(advanced_cancel_button_);

  UpdateAdvancedView();

  return view;
}

views::Checkbox* TranslateBubbleView::GetAlwaysTranslateCheckbox() {
  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ADVANCED) {
    return advanced_always_translate_checkbox_;
  } else if (model_->GetViewState() ==
             TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
    return before_always_translate_checkbox_;
  } else {
    NOTREACHED();
    return nullptr;
  }
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  if (model_->GetViewState() == view_state)
    return;

  model_->SetViewState(view_state);
  if (view_state == TranslateBubbleModel::VIEW_STATE_ADVANCED)
    UpdateAdvancedView();
  UpdateChildVisibilities();
  SizeToContents();
}

void TranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors::Type error_type) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void TranslateBubbleView::UpdateAdvancedView() {
  // "Always translate" checkbox doesn't exist in an incognito window.
  if (advanced_always_translate_checkbox_) {
    advanced_always_translate_checkbox_->SetText(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS));
  }

  DCHECK(advanced_done_button_);
  advanced_done_button_->SetText(
      l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                    ? IDS_DONE
                                    : IDS_TRANSLATE_BUBBLE_ACCEPT));
}
