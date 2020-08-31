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
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/md_text_button_with_down_arrow.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Container for |advanced_view_|. When the text on the "Translate"/"Done"
// button changes a layout is required.
class AdvancedViewContainer : public views::View {
 public:
  AdvancedViewContainer() {}

  void ChildPreferredSizeChanged(views::View* child) override { Layout(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(AdvancedViewContainer);
};

bool UseGoogleTranslateBranding() {
  // Only use Google Translate branding in Chrome branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<views::View> CreateWordmarkView(
    language::TranslateUIBubbleModel bubble_model) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (bubble_model != language::TranslateUIBubbleModel::TAB) {
    return nullptr;
  }

  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Translate icon
  const int translate_icon_id = IDR_TRANSLATE_TAB_WORDMARK;
  std::unique_ptr<views::ImageView> translate_icon =
      std::make_unique<views::ImageView>();
  gfx::ImageSkia* translate_icon_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          translate_icon_id);
  translate_icon->SetImage(*translate_icon_image);

  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(std::move(translate_icon));

  return view;
#else
  return nullptr;
#endif
}

}  // namespace

// static
TranslateBubbleView* TranslateBubbleView::translate_bubble_view_ = nullptr;

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
    views::Button* highlighted_button,
    content::WebContents* web_contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    LocationBarBubbleDelegateView::DisplayReason reason) {
  if (translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (translate_bubble_view_->web_contents() == web_contents &&
        (translate_bubble_view_->model()->GetViewState() ==
             TranslateBubbleModel::VIEW_STATE_ADVANCED ||
         translate_bubble_view_->model()->GetViewState() ==
             TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
         translate_bubble_view_->model()->GetViewState() ==
             TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE)) {
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
  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      new translate::TranslateUIDelegate(
          ChromeTranslateClient::GetManagerFromWebContents(web_contents)
              ->GetWeakPtr(),
          source_language, target_language));
  std::unique_ptr<TranslateBubbleModel> model(
      new TranslateBubbleModelImpl(step, std::move(ui_delegate)));
  TranslateBubbleView* view = new TranslateBubbleView(
      anchor_view, std::move(model), error_type, web_contents);

  if (highlighted_button)
    view->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(view);

  // TAB UI has the same view throughout. Select the right tab based on |step|
  // upon initialization.
  if (step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
    TranslateBubbleModel::ViewState state =
        TranslateBubbleModelImpl::TranslateStepToViewState(step);
    translate_bubble_view_->SwitchView(state);
  } else {
    translate_bubble_view_->SwitchToErrorView(error_type);
  }

  // |allow_refocus_alert| is set to false because translate bubble does not
  // have an additional screen reader alert instructing the user to use a
  // hotkey combination to focus the bubble.
  view->ShowForReason(reason, false);
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

base::string16 TranslateBubbleView::GetWindowTitle() const {
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
    // Widget title and close button does not show for TAB ui.
    // These two cases don't apply but need to be handled to avoid error.
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      id = IDS_TRANSLATE_BUBBLE_ADVANCED_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      id = IDS_TRANSLATE_BUBBLE_ADVANCED_TITLE;
      break;
  }

  return l10n_util::GetStringUTF16(id);
}

void TranslateBubbleView::TabSelectedAt(int index) {
  // Tabbed pane is indexed from left to right starting at 0.
  if (!model_->IsPageTranslatedInCurrentLanguages() && index == 1) {
    Translate();
  } else if (index == 0) {
    ShowOriginal();
  }
}

void TranslateBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  should_always_translate_ = model_->ShouldAlwaysTranslate();
  should_never_translate_language_ = model_->ShouldNeverTranslateLanguage();
  should_never_translate_site_ = model_->ShouldNeverTranslateSite();
  // Create different view based on user selection in
  // kUseButtonTranslateBubbleUI.
  if (bubble_ui_model_ == language::TranslateUIBubbleModel::TAB) {
    // |tab_translate_view| is the child view being used before/during/after
    // translate in TAB UI.
    tab_translate_view_ = AddChildView(CreateViewTab());
    before_translate_view_ = tab_translate_view_;
    translating_view_ = tab_translate_view_;
    after_translate_view_ = tab_translate_view_;
    advanced_view_source_ = AddChildView(TabUiCreateViewAdvancedSource());
    advanced_view_target_ = AddChildView(TabUiCreateViewAdvancedTarget());
    error_view_ = AddChildView(CreateViewErrorTab());
  } else {
    before_translate_view_ = AddChildView(CreateViewBeforeTranslate());
    translating_view_ = AddChildView(CreateViewTranslating());
    after_translate_view_ = AddChildView(CreateViewAfterTranslate());
    advanced_view_ = AddChildView(CreateViewAdvanced());
    error_view_ = AddChildView(CreateViewError());
  }

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();

  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ERROR)
    model_->ShowError(error_type_);
}

void TranslateBubbleView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  switch (static_cast<ButtonID>(sender->GetID())) {
    case BUTTON_ID_TRANSLATE: {
      Translate();
      break;
    }
    case BUTTON_ID_DONE: {
      ConfirmAdvancedOptions();
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
      ShowOriginal();
      break;
    }
    case BUTTON_ID_ALWAYS_TRANSLATE: {
      views::Checkbox* always_checkbox = GetAlwaysTranslateCheckbox();
      DCHECK(always_checkbox);
      should_always_translate_ = always_checkbox->GetChecked();
      // In the tab UI the always translate button should apply immediately
      // except for in an advanced view.
      if (bubble_ui_model_ == language::TranslateUIBubbleModel::TAB &&
          model_->GetViewState() !=
              TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
        model_->SetAlwaysTranslate(should_always_translate_);
      }
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
    case BUTTON_ID_OPTIONS_MENU: {
      ShowOptionsMenu(sender);
      break;
    }
    case BUTTON_ID_OPTIONS_MENU_TAB: {
      ShowOptionsMenuTab(sender);
      break;
    }
    case BUTTON_ID_CLOSE: {
      translate::ReportUiAction(translate::CLOSE_BUTTON_CLICKED);
      GetWidget()->Close();
      break;
    }
    case BUTTON_ID_RESET: {
      ResetLanguage();
      break;
    }
    case BUTTON_ID_RETURN: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
      tabbed_pane_->SelectTabAt(1);
      UpdateChildVisibilities();
      SizeToContents();
      break;
    }
  }
}

views::View* TranslateBubbleView::GetInitiallyFocusedView() {
  return GetCurrentView()->GetNextFocusableView();
}

bool TranslateBubbleView::ShouldShowCloseButton() const {
  return bubble_ui_model_ != language::TranslateUIBubbleModel::TAB;
}

bool TranslateBubbleView::ShouldShowWindowTitle() const {
  return bubble_ui_model_ != language::TranslateUIBubbleModel::TAB;
}

void TranslateBubbleView::ResetLanguage() {
  if (model_->GetViewState() ==
      TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    if (source_language_combobox_->GetSelectedIndex() ==
        previous_source_language_index_ + 1) {
      return;
    }
    source_language_combobox_->SetSelectedIndex(
        previous_source_language_index_ + 1);
    model_->UpdateOriginalLanguageIndex(
        source_language_combobox_->GetSelectedIndex() - 1);
  } else {
    if (target_language_combobox_->GetSelectedIndex() ==
        previous_target_language_index_) {
      return;
    }
    target_language_combobox_->SetSelectedIndex(
        previous_target_language_index_);
    model_->UpdateTargetLanguageIndex(
        target_language_combobox_->GetSelectedIndex());
  }
  UpdateAdvancedView();
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
  translate_bubble_view_ = nullptr;
}

bool TranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        Translate();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ShowOriginal();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      break;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ConfirmAdvancedOptions();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      break;
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      break;
  }
  return BubbleDialogDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size TranslateBubbleView::CalculatePreferredSize() const {
  int width = 0;
  for (const views::View* child : children())
    width = std::max(width, child->GetPreferredSize().width());
  return gfx::Size(width, GetCurrentView()->GetPreferredSize().height());
}

void TranslateBubbleView::OnPerformAction(views::Combobox* combobox) {
  HandleComboboxPerformAction(static_cast<ComboboxID>(combobox->GetID()));
}

void TranslateBubbleView::ShowOptionsMenu(views::Button* source) {
  // Recreate the menu model as translated languages can change while the menu
  // is not showing, which invalidates these text strings.
  options_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  // Don't show "Always translate <language>" in incognito mode, because it
  // doesn't do anything anyways.
  if (!is_in_incognito_window_) {
    options_menu_model_->AddCheckItem(
        OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));
  }

  options_menu_model_->AddItem(
      OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE,
      l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
          model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));

  if (model_->CanBlacklistSite()) {
    options_menu_model_->AddItemWithStringId(
        OptionsMenuItem::NEVER_TRANSLATE_SITE,
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE);
  }

  options_menu_model_->AddItemWithStringId(
      OptionsMenuItem::MORE_OPTIONS, IDS_TRANSLATE_BUBBLE_ADVANCED_MENU_BUTTON);

  options_menu_runner_ = std::make_unique<views::MenuRunner>(
      options_menu_model_.get(), views::MenuRunner::COMBOBOX);
  gfx::Rect screen_bounds = source->GetBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), nullptr, screen_bounds,
                                  views::MenuAnchorPosition::kTopRight,
                                  ui::MENU_SOURCE_MOUSE);
}

// Create the menu items for the dropdown options menu under TAB UI.
void TranslateBubbleView::ShowOptionsMenuTab(views::Button* source) {
  // Recreate the menu model as translated languages can change while the menu
  // is not showing, which invalidates these text strings.
  tab_options_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  tab_options_menu_model_->AddItemWithStringId(
      OptionsMenuItem::CHANGE_TARGET_LANGUAGE,
      IDS_TRANSLATE_BUBBLE_CHANGE_TARGET_LANGUAGE);

  auto original_language =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());

  // Don't show "Always translate <language>" in incognito mode, because it
  // doesn't do anything anyways. Don't show if the source language is an empty
  // string.
  if (!is_in_incognito_window_ && !original_language.empty()) {
    tab_options_menu_model_->AddCheckItem(
        OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
                                   original_language));
  }

  if (!original_language.empty()) {
    tab_options_menu_model_->AddCheckItem(
        OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
                                   original_language));
  }

  if (model_->CanBlacklistSite()) {
    tab_options_menu_model_->AddCheckItem(
        OptionsMenuItem::NEVER_TRANSLATE_SITE,
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE));
  }

  tab_options_menu_model_->AddItem(
      OptionsMenuItem::CHANGE_SOURCE_LANGUAGE,
      l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_BUBBLE_CHANGE_SOURCE_LANGUAGE,
          original_language.empty()
              ? l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_UNKNOWN_LANGUAGE)
              : original_language));

  options_menu_runner_ = std::make_unique<views::MenuRunner>(
      tab_options_menu_model_.get(), views::MenuRunner::COMBOBOX);
  gfx::Rect screen_bounds = source->GetAnchorBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), nullptr, screen_bounds,
                                  views::MenuAnchorPosition::kTopRight,
                                  ui::MENU_SOURCE_MOUSE);
}

bool TranslateBubbleView::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE:
      return should_never_translate_language_;
    case OptionsMenuItem::NEVER_TRANSLATE_SITE:
      return should_never_translate_site_;
    case OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      return should_always_translate_;
    default:
      NOTREACHED();
      return false;
  }
}

bool TranslateBubbleView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TranslateBubbleView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      should_always_translate_ = !should_always_translate_;
      model_->SetAlwaysTranslate(should_always_translate_);
      if (should_always_translate_) {
        should_never_translate_language_ = false;
        model_->SetNeverTranslateLanguage(should_never_translate_language_);
        if (((model_->GetViewState() ==
              TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) ||
             (bubble_ui_model_ == language::TranslateUIBubbleModel::TAB &&
              TabUiIsEquivalentState(model_->GetViewState())))) {
          model_->Translate();
          SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
        }
      }
      break;

    case OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE:
      should_never_translate_language_ = !should_never_translate_language_;
      if (should_never_translate_language_) {
        should_always_translate_ = false;
        model_->SetAlwaysTranslate(should_always_translate_);
        translate::ReportUiAction(
            translate::NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED);
        model_->SetNeverTranslateLanguage(true);
        RevertOrDeclineTranslation();
      } else {
        model_->SetNeverTranslateLanguage(false);
      }
      break;

    case OptionsMenuItem::NEVER_TRANSLATE_SITE:
      should_never_translate_site_ = !should_never_translate_site_;
      if (should_never_translate_site_) {
        translate::ReportUiAction(translate::NEVER_TRANSLATE_SITE_MENU_CLICKED);
        model_->SetNeverTranslateSite(true);
        RevertOrDeclineTranslation();
      } else {
        model_->SetNeverTranslateSite(false);
      }
      break;

    case OptionsMenuItem::MORE_OPTIONS:
      translate::ReportUiAction(translate::ADVANCED_MENU_CLICKED);
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      break;

    case OptionsMenuItem::CHANGE_TARGET_LANGUAGE:
      translate::ReportUiAction(translate::ADVANCED_MENU_CLICKED);
      SwitchView(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);
      break;

    case OptionsMenuItem::CHANGE_SOURCE_LANGUAGE:
      translate::ReportUiAction(translate::ADVANCED_MENU_CLICKED);
      SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);
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

void TranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (GetBubbleFrameView()->GetWidget()->closed_reason() ==
      views::Widget::ClosedReason::kCloseButtonClicked) {
    model_->DeclineTranslation();
    translate::ReportUiAction(translate::CLOSE_BUTTON_CLICKED);
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
      model_(std::move(model)),
      error_type_(error_type),
      is_in_incognito_window_(
          web_contents && web_contents->GetBrowserContext()->IsOffTheRecord()),
      bubble_ui_model_(language::GetTranslateUiBubbleModel()) {
  translate_bubble_view_ = this;

  UpdateInsets(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);

  if (web_contents)  // web_contents can be null in unit_tests.
    mouse_handler_ =
        std::make_unique<WebContentMouseHandler>(this, web_contents);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetFootnoteView(CreateWordmarkView(bubble_ui_model_));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TRANSLATE);
}

views::View* TranslateBubbleView::GetCurrentView() const {
  switch (model_->GetViewState()) {
    // TAB UI doesn't change view before/during/after translate. Only one chiled
    // view tab_translate_view is created and used throughout the translate
    // process.
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
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      return advanced_view_source_;
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      return advanced_view_target_;
  }
  NOTREACHED();
  return NULL;
}

void TranslateBubbleView::Translate() {
  // TODO(crbug/1074171): Shouldn't need the line below after cleaning up the
  // old UI.
  model_->SetAlwaysTranslate(should_always_translate_);
  model_->Translate();
  translate::ReportUiAction(translate::TRANSLATE_BUTTON_CLICKED);
}

void TranslateBubbleView::ShowOriginal() {
  model_->RevertTranslation();
  // In TAB UI, we keep the translate bubble shown to encourage user
  // experimentation and avoid click by mistake. The close button
  // at the end of the bubble provides the functionality to close.
  if (bubble_ui_model_ != language::TranslateUIBubbleModel::TAB) {
    GetWidget()->Close();
  }
  translate::ReportUiAction(translate::SHOW_ORIGINAL_BUTTON_CLICKED);
}

void TranslateBubbleView::ConfirmAdvancedOptions() {
  model_->SetAlwaysTranslate(should_always_translate_);
  if (bubble_ui_model_ == language::TranslateUIBubbleModel::TAB) {
    if (model_->IsPageTranslatedInCurrentLanguages()) {
      SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
      SizeToContents();
    } else {
      base::string16 original_language_name;
      base::string16 target_language_name;
      DCHECK(tabbed_pane_);
      UpdateLanguageNames(&original_language_name, &target_language_name);
      tabbed_pane_->GetTabAt(0)->SetTitleText(original_language_name);
      tabbed_pane_->GetTabAt(1)->SetTitleText(target_language_name);
      model_->Translate();
      tabbed_pane_->SelectTabAt(1);
      SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
    }
  } else {
    if (model_->IsPageTranslatedInCurrentLanguages()) {
      model_->GoBackFromAdvanced();
      UpdateChildVisibilities();
      SizeToContents();
    } else {
      model_->Translate();
      SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
    }
  }
  translate::ReportUiAction(translate::DONE_BUTTON_CLICKED);
}

void TranslateBubbleView::HandleComboboxPerformAction(
    TranslateBubbleView::ComboboxID sender_id) {
  switch (sender_id) {
    case COMBOBOX_ID_SOURCE_LANGUAGE: {
      model_->UpdateOriginalLanguageIndex(
          source_language_combobox_->GetSelectedIndex() - 1);
      UpdateAdvancedView();
      translate::ReportUiAction(translate::SOURCE_LANGUAGE_MENU_CLICKED);
      break;
    }
    case COMBOBOX_ID_TARGET_LANGUAGE: {
      model_->UpdateTargetLanguageIndex(
          target_language_combobox_->GetSelectedIndex());
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
  if (before_always_translate_checkbox_) {
    before_always_translate_checkbox_->SetText(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
        model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));
    before_always_translate_checkbox_->SetChecked(should_always_translate_);
  }
  for (views::View* view : children())
    view->SetVisible(view == GetCurrentView());

  // Not required for TAB UI because the title is not shown.
  if (bubble_ui_model_ != language::TranslateUIBubbleModel::TAB &&
      GetWidget()) {
    GetWidget()->UpdateWindowTitle();
  }
  // BoxLayout only considers visible children, so ensure any newly visible
  // child views are positioned correctly.
  Layout();
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewBeforeTranslate() {
  base::string16 original_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  if (original_language_name.empty()) {
    original_language_name =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_UNKNOWN_LANGUAGE);
  }

  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kCheckboxColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kCheckboxColumnSetId);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  constexpr int kButtonColumnSetId = 1;
  cs = layout->AddColumnSet(kButtonColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  if (model_->ShouldShowAlwaysTranslateShortcut()) {
    layout->StartRow(views::GridLayout::kFixedSize, kCheckboxColumnSetId);
    auto before_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())),
        this);
    before_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
    before_always_translate_checkbox_ =
        layout->AddView(std::move(before_always_translate_checkbox));
  }

  layout->StartRowWithPadding(
      views::GridLayout::kFixedSize, kButtonColumnSetId,
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  auto accept_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT));
  accept_button->SetID(BUTTON_ID_TRANSLATE);
  accept_button->SetIsDefault(true);
  auto before_translate_options_button =
      std::make_unique<views::MdTextButtonWithDownArrow>(
          this,
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  before_translate_options_button->SetID(BUTTON_ID_OPTIONS_MENU);
  before_translate_options_button->set_request_focus_on_press(true);

  if (views::PlatformStyle::kIsOkButtonLeading) {
    layout->AddView(std::move(accept_button));
    before_translate_options_button_ =
        layout->AddView(std::move(before_translate_options_button));
  } else {
    before_translate_options_button_ =
        layout->AddView(std::move(before_translate_options_button));
    layout->AddView(std::move(accept_button));
  }

  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateEmptyPane() {
  auto pane = std::make_unique<views::View>();
  pane->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  return pane;
}

// This view is shown if "Tab" is selected for feature
// kUseButtonTranslateBubbleUi before/on/after translate.
std::unique_ptr<views::View> TranslateBubbleView::CreateViewTab() {
  base::string16 original_language_name;
  base::string16 target_language_name;
  UpdateLanguageNames(&original_language_name, &target_language_name);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  auto inner_view = std::make_unique<views::View>();
  inner_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  auto* horizontal_view = view->AddChildView(std::move(inner_view));

  views::View* icon = nullptr;
  if (!UseGoogleTranslateBranding()) {
    icon = horizontal_view->AddChildView(CreateTranslateIcon());
  }

  // Tabbed pane for language selection. Can't use unique_ptr because
  // tabs have to be added after the tabbed_pane is added to the parent,
  // when we release ownership of the unique_ptr.
  auto tabbed_pane = std::make_unique<views::TabbedPane>();
  tabbed_pane_ = horizontal_view->AddChildView(std::move(tabbed_pane));

  // NOTE: Panes must be added after |tabbed_pane| has been added to its parent.
  tabbed_pane_->AddTab(original_language_name, CreateEmptyPane());
  tabbed_pane_->AddTab(target_language_name, CreateEmptyPane());
  tabbed_pane_->GetTabAt(0)->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(2, 20)));
  tabbed_pane_->GetTabAt(1)->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(2, 20)));
  tabbed_pane_->set_listener(this);

  auto* padding_view =
      horizontal_view->AddChildView(std::make_unique<views::View>());
  auto* options_menu = horizontal_view->AddChildView(CreateOptionsMenuButton());
  horizontal_view->AddChildView(CreateCloseButton());

  // Don't show the the always translate checkbox if the original language is
  // unknown.
  auto original_language =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  if (model_->ShouldShowAlwaysTranslateShortcut() &&
      !original_language.empty()) {
    auto before_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())),
        this);
    before_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
    before_always_translate_checkbox_ =
        view->AddChildView(std::move(before_always_translate_checkbox));
  }

  if (icon) {
    icon->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, 0, 0,
                    provider->GetDistanceMetric(
                        views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  }
  tabbed_pane_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  padding_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));
  options_menu->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, provider->GetDistanceMetric(
                         views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  if (before_always_translate_checkbox_) {
    horizontal_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, 0,
                    provider->GetDistanceMetric(
                        views::DISTANCE_RELATED_CONTROL_VERTICAL),
                    0));
    before_always_translate_checkbox_->SetProperty(views::kMarginsKey,
                                                   gfx::Insets(2, 0));
  }

  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewTranslating() {
  base::string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());

  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred,
                views::GridLayout::kFixedSize, 0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred,
                views::GridLayout::kFixedSize, 0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);

  auto revert_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  revert_button->SetID(BUTTON_ID_SHOW_ORIGINAL);
  revert_button->SetEnabled(false);
  layout->AddView(std::move(revert_button));

  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewAfterTranslate() {
  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred,
                views::GridLayout::kFixedSize, 0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  auto button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  button->SetID(BUTTON_ID_SHOW_ORIGINAL);
  layout->AddView(std::move(button));

  auto options_menu_button = std::make_unique<views::MdTextButtonWithDownArrow>(
      this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  options_menu_button->SetID(BUTTON_ID_OPTIONS_MENU);
  options_menu_button->set_request_focus_on_press(true);

  layout->AddView(std::move(options_menu_button));

  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewError() {
  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);

  auto try_again_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  try_again_button->SetID(BUTTON_ID_TRY_AGAIN);
  layout->AddView(std::move(try_again_button));

  auto advanced_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_BUTTON));
  advanced_button->SetID(BUTTON_ID_ADVANCED);
  layout->AddView(std::move(advanced_button));
  Layout();
  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewErrorTab() {
  auto translate_options_button =
      std::make_unique<views::MdTextButtonWithDownArrow>(
          this,
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  translate_options_button->SetID(BUTTON_ID_OPTIONS_MENU_TAB);
  translate_options_button->set_request_focus_on_press(true);
  return CreateViewErrorNoTitle(std::move(translate_options_button));
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewErrorNoTitle(
    std::unique_ptr<views::Button> advanced_button) {
  auto view = std::make_unique<views::View>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  enum { COLUMN_SET_ID_TITLE, COLUMN_SET_ID_BUTTONS };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(1, provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE);

  int error_message_id = IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE;
  auto error_message_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(error_message_id),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  error_message_label->SetLineHeight(vertical_spacing * 5);
  layout->AddView(std::move(error_message_label));
  layout->AddView(CreateCloseButton());

  layout->StartRowWithPadding(
      views::GridLayout::kFixedSize, COLUMN_SET_ID_BUTTONS,
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  auto try_again_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  try_again_button->SetID(BUTTON_ID_TRY_AGAIN);
  layout->AddView(std::move(try_again_button));

  layout->AddView(std::move(advanced_button));

  Layout();
  return view;
}

// TODO(hajimehoshi): Revice this later to show a specific message for each
// error. (crbug/307350)
std::unique_ptr<views::View> TranslateBubbleView::CreateViewAdvanced() {
  // Index + 1 because GetOriginalLanguageIndex() returns the actual index - 1
  // to accommodate added label "Unknown". (crbug/721600)
  int source_default_index = model_->GetOriginalLanguageIndex() + 1;
  source_language_combobox_model_.reset(
      new SourceLanguageComboboxModel(source_default_index, model_.get()));
  auto source_language_combobox =
      std::make_unique<views::Combobox>(source_language_combobox_model_.get());

  source_language_combobox->SetID(COMBOBOX_ID_SOURCE_LANGUAGE);
  source_language_combobox->set_listener(this);

  int target_default_index = model_->GetTargetLanguageIndex();
  target_language_combobox_model_.reset(
      new TargetLanguageComboboxModel(target_default_index, model_.get()));
  auto target_language_combobox =
      std::make_unique<views::Combobox>(target_language_combobox_model_.get());

  target_language_combobox->SetID(COMBOBOX_ID_TARGET_LANGUAGE);
  target_language_combobox->set_listener(this);

  // In an incognito window, "Always translate" checkbox shouldn't be shown.
  std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox;
  if (!is_in_incognito_window_) {
    advanced_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS), this);
    advanced_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
  }

  auto view = std::make_unique<AdvancedViewContainer>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  enum {
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_BUTTONS,
    COLUMN_SET_ID_ALWAYS_CHECKBOX,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  using ColumnSize = views::GridLayout::ColumnSize;
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(1.0, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      1.0, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_ALWAYS_CHECKBOX);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_PAGE_LANGUAGE)));
  source_language_combobox_ =
      layout->AddView(std::move(source_language_combobox));

  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATION_LANGUAGE)));
  target_language_combobox_ =
      layout->AddView(std::move(target_language_combobox));

  if (advanced_always_translate_checkbox) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
    layout->StartRow(views::GridLayout::kFixedSize,
                     COLUMN_SET_ID_ALWAYS_CHECKBOX);
    advanced_always_translate_checkbox_ =
        layout->AddView(std::move(advanced_always_translate_checkbox));
  }

  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_BUTTONS);
  layout->SkipColumns(1);

  auto advanced_done_button =
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button->SetID(BUTTON_ID_DONE);
  advanced_done_button->SetIsDefault(true);
  auto advanced_cancel_button =
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  advanced_cancel_button->SetID(BUTTON_ID_CANCEL);
  advanced_done_button_ = layout->AddView(std::move(advanced_done_button));
  layout->AddView(std::move(advanced_cancel_button));

  UpdateAdvancedView();

  return view;
}

std::unique_ptr<views::View>
TranslateBubbleView::TabUiCreateViewAdvancedSource() {
  // Bubble title
  std::unique_ptr<views::Label> source_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_SOURCE),
          views::style::CONTEXT_DIALOG_TITLE);

  // Index + 1 because GetOriginalLanguageIndex() returns the actual index - 1
  // to accommodate added label "Unknown". (crbug/721600)
  // Language icon
  int source_default_index = model_->GetOriginalLanguageIndex() + 1;
  source_language_combobox_model_.reset(
      new SourceLanguageComboboxModel(source_default_index, model_.get()));

  // Ideally all child view elements shall be created using unique_ptr.
  // Using normal pointer for compatibility with existing code.
  auto source_language_combobox =
      std::make_unique<views::Combobox>(source_language_combobox_model_.get());

  // In an incognito window or when the source language is unknown, "Always
  // translate" checkbox shouldn't be shown.
  std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox;
  auto original_language =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  if (!is_in_incognito_window_ && !original_language.empty()) {
    advanced_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS), this);
    advanced_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
  }

  source_language_combobox->SetID(COMBOBOX_ID_SOURCE_LANGUAGE);
  source_language_combobox->set_listener(this);
  source_language_combobox_ = source_language_combobox.get();

  auto advanced_done_button =
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button->SetID(BUTTON_ID_DONE);
  advanced_done_button->SetIsDefault(true);
  advanced_done_button_source_ = advanced_done_button.get();

  return CreateViewAdvancedTabUi(std::move(source_language_combobox),
                                 std::move(source_language_title_label),
                                 std::move(advanced_done_button),
                                 std::move(advanced_always_translate_checkbox));
}

std::unique_ptr<views::View>
TranslateBubbleView::TabUiCreateViewAdvancedTarget() {
  // Bubble title
  std::unique_ptr<views::Label> target_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_TARGET),
          views::style::CONTEXT_DIALOG_TITLE);

  int target_default_index = model_->GetTargetLanguageIndex();
  target_language_combobox_model_.reset(
      new TargetLanguageComboboxModel(target_default_index, model_.get()));

  // Ideally all view components shall be created using unique_ptr.
  // Using normal pointer for compatibility with existing code.
  auto target_language_combobox =
      std::make_unique<views::Combobox>(target_language_combobox_model_.get());

  target_language_combobox->SetID(COMBOBOX_ID_TARGET_LANGUAGE);
  target_language_combobox->set_listener(this);
  target_language_combobox_ = target_language_combobox.get();

  auto advanced_done_button =
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button->SetID(BUTTON_ID_DONE);
  advanced_done_button->SetIsDefault(true);
  advanced_done_button_target_ = advanced_done_button.get();

  return CreateViewAdvancedTabUi(std::move(target_language_combobox),
                                 std::move(target_language_title_label),
                                 std::move(advanced_done_button), nullptr);
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewAdvancedTabUi(
    std::unique_ptr<views::Combobox> combobox,
    std::unique_ptr<views::Label> language_title_label,
    std::unique_ptr<views::Button> advanced_done_button,
    std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox) {
  auto view = std::make_unique<AdvancedViewContainer>();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  std::unique_ptr<views::ImageView> language_icon = CreateTranslateIcon();

  enum {
    COLUMN_SET_ID_TITLE,
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_ALWAYS_CHECKBOX,
    COLUMN_SET_ID_BUTTONS
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  if (!UseGoogleTranslateBranding()) {
    cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                  views::GridLayout::kFixedSize,
                  views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                         provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  }
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(1, provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL) *
                              4);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);

  if (!UseGoogleTranslateBranding()) {
    cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                         language_icon->CalculatePreferredSize().width());
    cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                         provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
    cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                  views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  } else {
    cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                  views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  }
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  cs = layout->AddColumnSet(COLUMN_SET_ID_ALWAYS_CHECKBOX);
  if (!UseGoogleTranslateBranding()) {
    cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                         language_icon->CalculatePreferredSize().width());
    cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                         provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
    cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                  views::GridLayout::kFixedSize,
                  views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  } else {
    cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                  views::GridLayout::kFixedSize,
                  views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  }

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      1.0, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE);
  if (!UseGoogleTranslateBranding()) {
    // If the bottom branding isn't showing, display the leading translate
    // icon otherwise it's not obvious what the bubble is about. This should
    // only happen on non-Chrome-branded builds.
    layout->AddView(std::move(language_icon));
  }
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  language_title_label->SetLineHeight(vertical_spacing * 5);
  layout->AddView(std::move(language_title_label));
  layout->AddView(CreateCloseButton());

  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(std::move(combobox));

  if (advanced_always_translate_checkbox) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
    layout->StartRow(views::GridLayout::kFixedSize,
                     COLUMN_SET_ID_ALWAYS_CHECKBOX);
    advanced_always_translate_checkbox_ =
        layout->AddView(std::move(advanced_always_translate_checkbox));
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing * 2);
  } else {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing * 3);
  }

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_BUTTONS);
  layout->SkipColumns(1);

  auto advanced_reset_button = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button->SetID(BUTTON_ID_RESET);
  layout->AddView(std::move(advanced_reset_button));
  layout->AddView(std::move(advanced_done_button));

  UpdateAdvancedView();

  return view;
}

std::unique_ptr<views::ImageView> TranslateBubbleView::CreateTranslateIcon() {
  const int language_icon_id = IDR_TRANSLATE_BUBBLE_ICON;
  std::unique_ptr<views::ImageView> language_icon =
      std::make_unique<views::ImageView>();
  gfx::ImageSkia* language_icon_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          language_icon_id);
  language_icon->SetImage(*language_icon_image);
  return language_icon;
}

std::unique_ptr<views::Button> TranslateBubbleView::CreateOptionsMenuButton() {
  // Three dots options menu button
  auto tab_translate_options_button =
      views::CreateVectorImageButtonWithNativeTheme(this, kBrowserToolsIcon);
  InstallCircleHighlightPathGenerator(tab_translate_options_button.get());
  tab_translate_options_button->SetFocusForPlatform();
  tab_translate_options_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  tab_translate_options_button->set_request_focus_on_press(true);
  tab_translate_options_button->SetVisible(true);
  tab_translate_options_button->SetID(BUTTON_ID_OPTIONS_MENU_TAB);
  return tab_translate_options_button;
}

std::unique_ptr<views::Button> TranslateBubbleView::CreateCloseButton() {
  auto close_button = views::BubbleFrameView::CreateCloseButton(this);
  close_button->SetVisible(true);
  close_button->SetID(BUTTON_ID_CLOSE);
  return close_button;
}

bool TranslateBubbleView::TabUiIsEquivalentState(
    TranslateBubbleModel::ViewState view_state) {
  return view_state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
         view_state == TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
         view_state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE;
}

views::Checkbox* TranslateBubbleView::GetAlwaysTranslateCheckbox() {
  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ADVANCED ||
      model_->GetViewState() ==
          TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      model_->GetViewState() ==
          TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    return advanced_always_translate_checkbox_;
  } else if (model_->GetViewState() ==
                 TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
             model_->GetViewState() ==
                 TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
             model_->GetViewState() ==
                 TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    return before_always_translate_checkbox_;
  } else {
    NOTREACHED();
    return nullptr;
  }
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  UpdateInsets(view_state);

  if (view_state == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    GetBubbleFrameView()->SetFootnoteView(nullptr);
  } else {
    GetBubbleFrameView()->SetFootnoteView(CreateWordmarkView(bubble_ui_model_));
  }

  if (bubble_ui_model_ == language::TranslateUIBubbleModel::TAB) {
    SwitchTabForViewState(view_state);
  }

  model_->SetViewState(view_state);
  if (view_state == TranslateBubbleModel::VIEW_STATE_ADVANCED)
    UpdateAdvancedView();

  UpdateChildVisibilities();
  SizeToContents();
}

void TranslateBubbleView::SwitchTabForViewState(
    TranslateBubbleModel::ViewState view_state) {
  if ((view_state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE ||
       view_state == TranslateBubbleModel::VIEW_STATE_TRANSLATING) &&
      tabbed_pane_->GetSelectedTabIndex() != 1) {
    // When switching to "after" or "during" translate view from something
    // other than user interaction, |this| needs to unregister from listening
    // to the tabbed pane events otherwise it'll trigger an additional
    // translation as if the user had clicked the tabs.
    tabbed_pane_->set_listener(nullptr);
    tabbed_pane_->SelectTabAt(1, false);
    tabbed_pane_->set_listener(this);
  } else if (view_state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE &&
             tabbed_pane_->GetSelectedTabIndex() != 0) {
    tabbed_pane_->SelectTabAt(0);
  }
}
void TranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors::Type error_type) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void TranslateBubbleView::UpdateAdvancedView() {
  if (advanced_done_button_) {
    advanced_done_button_->SetText(
        l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                      ? IDS_DONE
                                      : IDS_TRANSLATE_BUBBLE_ACCEPT));
  }
  if (advanced_done_button_source_) {
    advanced_done_button_source_->SetText(
        l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                      ? IDS_DONE
                                      : IDS_TRANSLATE_BUBBLE_ACCEPT));
  }
  if (advanced_done_button_target_) {
    advanced_done_button_target_->SetText(
        l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                      ? IDS_DONE
                                      : IDS_TRANSLATE_BUBBLE_ACCEPT));
  }
  Layout();
}

void TranslateBubbleView::UpdateLanguageNames(
    base::string16* original_language_name,
    base::string16* target_language_name) {
  DCHECK(original_language_name && target_language_name);
  previous_source_language_index_ = model_->GetOriginalLanguageIndex();
  *original_language_name =
      model_->GetLanguageNameAt(previous_source_language_index_);
  previous_target_language_index_ = model_->GetTargetLanguageIndex();
  *target_language_name =
      model_->GetLanguageNameAt(previous_target_language_index_);
  if (original_language_name->empty()) {
    *original_language_name =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_UNKNOWN_LANGUAGE);
  }
}

void TranslateBubbleView::UpdateInsets(TranslateBubbleModel::ViewState state) {
  if (bubble_ui_model_ != language::TranslateUIBubbleModel::TAB) {
    return;
  }

  gfx::Insets kTabStateMargins = gfx::Insets(7, 16, 8, 12);
  gfx::Insets kDialogStateMargins = gfx::Insets(5, 16, 16, 4);

  if (state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
      state == TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
      state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    translate_bubble_view_->set_margins(kTabStateMargins);
  } else {
    translate_bubble_view_->set_margins(kDialogStateMargins);
  }
}

void TranslateBubbleView::RevertOrDeclineTranslation() {
  if (model_->IsPageTranslatedInCurrentLanguages()) {
    model_->RevertTranslation();
  } else {
    model_->DeclineTranslation();
  }
  GetWidget()->Close();
}
