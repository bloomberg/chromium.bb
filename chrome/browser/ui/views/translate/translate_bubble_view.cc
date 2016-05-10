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
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

const char kTranslateBubbleUIEvent[] = "Translate.BubbleUiEvent";

views::LabelButton* CreateBlueButton(views::ButtonListener* listener,
                                     const base::string16& label,
                                     int id) {
  views::LabelButton* button = new views::BlueButton(listener, label);
  button->set_id(id);
  return button;
}

views::LabelButton* CreateLabelButton(views::ButtonListener* listener,
                                      const base::string16& label,
                                      int id) {
  views::LabelButton* button = new views::LabelButton(listener, label);
  button->set_id(id);
  button->SetStyle(views::Button::STYLE_BUTTON);
  return button;
}

views::Link* CreateLink(views::LinkListener* listener,
                        const base::string16& text,
                        int id) {
  views::Link* link = new views::Link(text);
  link->set_listener(listener);
  link->set_id(id);
  return link;
}

views::Link* CreateLink(views::LinkListener* listener,
                        int resource_id,
                        int id) {
  return CreateLink(listener, l10n_util::GetStringUTF16(resource_id), id);
}

// TODO(ftang) Restore icons in CreateViewAfterTranslate and CreateViewError
// without causing layout issues; see http://crbug.com/610351
void AddIconToLayout(views::GridLayout* layout) {
  views::ImageView* icon = new views::ImageView();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  icon->SetImage(bundle.GetImageNamed(IDR_TRANSLATE_ICON_BUBBLE).ToImageSkia());
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
      anchor_view, std::move(model), error_type, web_contents);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(view);
  view->ShowForReason(reason);
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

void TranslateBubbleView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

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

bool TranslateBubbleView::ShouldShowCloseButton() const {
  return Use2016Q2UI();
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

gfx::Size TranslateBubbleView::GetPreferredSize() const {
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
  if (!denial_menu_runner_) {
    denial_menu_model_.reset(new ui::SimpleMenuModel(this));
    denial_menu_model_->AddItem(
        DenialMenuItem::NEVER_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));

    denial_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

    denial_menu_model_->AddItemWithStringId(
        DenialMenuItem::NEVER_TRANSLATE_SITE,
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE);

    denial_menu_runner_.reset(
        new views::MenuRunner(denial_menu_model_.get(), 0));
  }
  gfx::Rect screen_bounds = source->GetBoundsInScreen();
  screen_bounds.Inset(source->GetInsets());
  denial_menu_runner_->RunMenuAt(source->GetWidget(), source, screen_bounds,
                                 views::MENU_ANCHOR_TOPRIGHT,
                                 ui::MENU_SOURCE_MOUSE);
}

bool TranslateBubbleView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool TranslateBubbleView::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool TranslateBubbleView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void TranslateBubbleView::ExecuteCommand(int command_id, int event_flags) {
  model_->DeclineTranslation();
  switch (command_id) {
    case DenialMenuItem::NEVER_TRANSLATE_LANGUAGE:
      model_->SetNeverTranslateLanguage(true);
      ReportUiAction(translate::NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED);
      break;
    case DenialMenuItem::NEVER_TRANSLATE_SITE:
      model_->SetNeverTranslateSite(true);
      ReportUiAction(translate::NEVER_TRANSLATE_SITE_MENU_CLICKED);
      break;
    default:
      NOTREACHED();
  }
  GetWidget()->Close();
}

void TranslateBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
  ReportUiAction(translate::ADVANCED_LINK_CLICKED);
}

void TranslateBubbleView::WebContentsDestroyed() {
  GetWidget()->CloseNow();
}

void TranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (GetBubbleFrameView()->close_button_clicked()) {
    model_->DeclineTranslation();
    ReportUiAction(translate::CLOSE_BUTTON_CLICKED);
  }
}

TranslateBubbleModel::ViewState TranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

TranslateBubbleView::TranslateBubbleView(
    views::View* anchor_view,
    std::unique_ptr<TranslateBubbleModel> model,
    translate::TranslateErrors::Type error_type,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      WebContentsObserver(web_contents),
      before_translate_view_(NULL),
      translating_view_(NULL),
      after_translate_view_(NULL),
      error_view_(NULL),
      advanced_view_(NULL),
      denial_combobox_(NULL),
      source_language_combobox_(NULL),
      target_language_combobox_(NULL),
      always_translate_checkbox_(NULL),
      advanced_cancel_button_(NULL),
      advanced_done_button_(NULL),
      denial_menu_button_(NULL),
      model_(std::move(model)),
      error_type_(error_type),
      is_in_incognito_window_(
          web_contents && web_contents->GetBrowserContext()->IsOffTheRecord()) {
  translate_bubble_view_ = this;
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
      if (always_translate_checkbox_)
        model_->SetAlwaysTranslate(always_translate_checkbox_->checked());
      model_->Translate();
      ReportUiAction(translate::TRANSLATE_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_DONE: {
      if (always_translate_checkbox_)
        model_->SetAlwaysTranslate(always_translate_checkbox_->checked());
      if (model_->IsPageTranslatedInCurrentLanguages()) {
        model_->GoBackFromAdvanced();
        UpdateChildVisibilities();
        SizeToContents();
      } else {
        model_->Translate();
        SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
      }
      ReportUiAction(translate::DONE_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_CANCEL: {
      model_->GoBackFromAdvanced();
      UpdateChildVisibilities();
      SizeToContents();
      ReportUiAction(translate::CANCEL_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_TRY_AGAIN: {
      model_->Translate();
      ReportUiAction(translate::TRY_AGAIN_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_SHOW_ORIGINAL: {
      model_->RevertTranslation();
      GetWidget()->Close();
      ReportUiAction(translate::SHOW_ORIGINAL_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_ALWAYS_TRANSLATE: {
      // Do nothing. The state of the checkbox affects only when the 'Done'
      // button is pressed.
      ReportUiAction(always_translate_checkbox_->checked()
                         ? translate::ALWAYS_TRANSLATE_CHECKED
                         : translate::ALWAYS_TRANSLATE_UNCHECKED);
      break;
    }
  }
}

void TranslateBubbleView::HandleLinkClicked(
    TranslateBubbleView::LinkID sender_id) {
  switch (sender_id) {
    case LINK_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      ReportUiAction(translate::ADVANCED_LINK_CLICKED);
      break;
    }
    case LINK_ID_LANGUAGE_SETTINGS: {
      GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
      web_contents()->OpenURL(
          content::OpenURLParams(url, content::Referrer(), NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_LINK, false));
      ReportUiAction(translate::SETTINGS_LINK_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::HandleComboboxPerformAction(
    TranslateBubbleView::ComboboxID sender_id) {
  switch (sender_id) {
    case COMBOBOX_ID_DENIAL: {
      model_->DeclineTranslation();
      DenialComboboxIndex index =
          static_cast<DenialComboboxIndex>(denial_combobox_->selected_index());
      switch (index) {
        case DenialComboboxIndex::DONT_TRANSLATE:
          ReportUiAction(translate::NOPE_MENU_CLICKED);
          break;
        case DenialComboboxIndex::NEVER_TRANSLATE_LANGUAGE:
          model_->SetNeverTranslateLanguage(true);
          ReportUiAction(translate::NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED);
          break;
        case DenialComboboxIndex::NEVER_TRANSLATE_SITE:
          model_->SetNeverTranslateSite(true);
          ReportUiAction(translate::NEVER_TRANSLATE_SITE_MENU_CLICKED);
          break;
        default:
          NOTREACHED();
          break;
      }
      GetWidget()->Close();
      break;
    }
    case COMBOBOX_ID_SOURCE_LANGUAGE: {
      if (model_->GetOriginalLanguageIndex() ==
          source_language_combobox_->selected_index()) {
        break;
      }
      model_->UpdateOriginalLanguageIndex(
          source_language_combobox_->selected_index());
      UpdateAdvancedView();
      ReportUiAction(translate::SOURCE_LANGUAGE_MENU_CLICKED);
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
      ReportUiAction(translate::TARGET_LANGUAGE_MENU_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::UpdateChildVisibilities() {
  for (int i = 0; i < child_count(); i++) {
    views::View* view = child_at(i);
    view->SetVisible(view == GetCurrentView());
  }
}

views::View* TranslateBubbleView::CreateViewBeforeTranslate() {
  const int kQuestionWidth = 200;
  base::string16 original_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  if (Use2016Q2UI()) {
    AddIconToLayout(layout);

    base::string16 target_language_name =
        model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());
    std::vector<size_t> offsets;
    auto styled_label = new views::StyledLabel(
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
  } else {
    layout->AddView(new views::Label(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE)));
    layout->AddView(
        CreateLink(this, IDS_TRANSLATE_BUBBLE_ADVANCED, LINK_ID_ADVANCED));
  }

  // In an incognito window, the "Always translate" checkbox shouldn't be shown.
  if (Use2016Q2UI() && !is_in_incognito_window_) {
    layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
    layout->SkipColumns(1);
    always_translate_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS_DO_THIS));
    always_translate_checkbox_->SetChecked(
        model_->ShouldAlwaysTranslateBeCheckedByDefault());
    always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    always_translate_checkbox_->set_listener(this);
    layout->AddView(always_translate_checkbox_);
  }
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* accept_button =
      Use2016Q2UI()
          ? CreateBlueButton(
                this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT),
                BUTTON_ID_TRANSLATE)
          : CreateLabelButton(
                this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT),
                BUTTON_ID_TRANSLATE);
  layout->AddView(accept_button);
  accept_button->SetIsDefault(true);
  if (Use2016Q2UI()) {
    denial_menu_button_ = new views::MenuButton(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON),
        this, true);
    denial_menu_button_->SetStyle(views::Button::STYLE_BUTTON);
    layout->AddView(denial_menu_button_);
  } else {
    std::vector<base::string16> items(
        static_cast<size_t>(DenialComboboxIndex::MENU_SIZE));
    items[static_cast<size_t>(DenialComboboxIndex::DONT_TRANSLATE)] =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY);
    items[static_cast<size_t>(DenialComboboxIndex::NEVER_TRANSLATE_LANGUAGE)] =
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
                                   original_language_name);
    items[static_cast<size_t>(DenialComboboxIndex::NEVER_TRANSLATE_SITE)] =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE);
    denial_combobox_model_.reset(new ui::SimpleComboboxModel(items));
    denial_combobox_ = new views::Combobox(denial_combobox_model_.get());
    denial_combobox_->set_id(COMBOBOX_ID_DENIAL);
    denial_combobox_->set_listener(this);
    denial_combobox_->SetStyle(views::Combobox::STYLE_ACTION);
    layout->AddView(denial_combobox_);
  }

  return view;
}

views::View* TranslateBubbleView::CreateViewTranslating() {
  base::string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATING));

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  if (Use2016Q2UI()) {
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  views::GridLayout::USE_PREF, 0, 0);
    cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  }
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  if (Use2016Q2UI())
    AddIconToLayout(layout);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  views::LabelButton* revert_button = CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT),
      BUTTON_ID_SHOW_ORIGINAL);
  revert_button->SetEnabled(false);
  layout->AddView(revert_button);

  return view;
}

views::View* TranslateBubbleView::CreateViewAfterTranslate() {
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATED));

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);

  // TODO(ftang) Restore icon without causing layout defects: crbug.com/610351

  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(label);
  layout->AddView(
      CreateLink(this, IDS_TRANSLATE_BUBBLE_ADVANCED, LINK_ID_ADVANCED));

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT),
      BUTTON_ID_SHOW_ORIGINAL));

  return view;
}

views::View* TranslateBubbleView::CreateViewError() {
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE));

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);

  // TODO(ftang) Restore icon without causing layout defects: crbug.com/610351

  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(label);
  layout->AddView(
      CreateLink(this, IDS_TRANSLATE_BUBBLE_ADVANCED, LINK_ID_ADVANCED));

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN),
      BUTTON_ID_TRY_AGAIN));

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
    always_translate_checkbox_ = new views::Checkbox(base::string16());
    always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    always_translate_checkbox_->set_listener(this);
  }

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_BUTTONS,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(source_language_label);
  layout->AddView(source_language_combobox_);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(target_language_label);
  layout->AddView(target_language_combobox_);

  if (!is_in_incognito_window_) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
    layout->SkipColumns(1);
    layout->AddView(always_translate_checkbox_);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_BUTTONS);
  layout->AddView(CreateLink(this, IDS_TRANSLATE_BUBBLE_LANGUAGE_SETTINGS,
                             LINK_ID_LANGUAGE_SETTINGS));
  advanced_done_button_ =
      Use2016Q2UI()
          ? CreateBlueButton(this, l10n_util::GetStringUTF16(IDS_DONE),
                             BUTTON_ID_DONE)
          : CreateLabelButton(this, l10n_util::GetStringUTF16(IDS_DONE),
                              BUTTON_ID_DONE);
  advanced_done_button_->SetIsDefault(true);
  advanced_cancel_button_ = CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL), BUTTON_ID_CANCEL);
  layout->AddView(advanced_done_button_);
  layout->AddView(advanced_cancel_button_);

  UpdateAdvancedView();

  return view;
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  if (model_->GetViewState() == view_state)
    return;

  model_->SetViewState(view_state);
  UpdateChildVisibilities();
  if (view_state == TranslateBubbleModel::VIEW_STATE_ADVANCED)
    UpdateAdvancedView();
  SizeToContents();
}

void TranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors::Type error_type) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void TranslateBubbleView::UpdateAdvancedView() {
  DCHECK(source_language_combobox_);
  DCHECK(target_language_combobox_);
  DCHECK(advanced_done_button_);

  base::string16 source_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  base::string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());

  // "Always translate" checkbox doesn't exist in an incognito window.
  if (always_translate_checkbox_) {
    always_translate_checkbox_->SetText(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS));
    always_translate_checkbox_->SetChecked(
        Use2016Q2UI() ? model_->ShouldAlwaysTranslateBeCheckedByDefault()
                      : model_->ShouldAlwaysTranslate());
  }

  base::string16 label;
  if (model_->IsPageTranslatedInCurrentLanguages())
    label = l10n_util::GetStringUTF16(IDS_DONE);
  else
    label = l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT);
  advanced_done_button_->SetText(label);
  advanced_done_button_->SizeToPreferredSize();
  if (advanced_view_)
    advanced_view_->Layout();
}

void TranslateBubbleView::ReportUiAction(
    translate::TranslateBubbleUiEvent action) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateBubbleUIEvent, action,
                            translate::TRANSLATE_BUBBLE_UI_EVENT_MAX);
}
