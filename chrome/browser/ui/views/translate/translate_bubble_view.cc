// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/string_compare.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/translate/translate_ui_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

views::LabelButton* CreateLabelButton(views::ButtonListener* listener,
                                      const string16& label, int id) {
  views::LabelButton* button = new views::LabelButton(listener, label);
  button->set_id(id);
  button->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  return button;
}

views::Link* CreateLink(views::LinkListener* listener,
                        int resource_id,
                        int id) {
  views::Link* link = new views::Link(
      l10n_util::GetStringUTF16(resource_id));
  link->set_listener(listener);
  link->set_id(id);
  return link;
}

void GetTranslateLanguages(content::WebContents* web_contents,
                           std::string* source,
                           std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  *source = translate_tab_helper->language_state().original_language();
  *target = TranslateManager::GetLanguageCode(
      g_browser_process->GetApplicationLocale());
}

// TODO(hajimehoshi): The interface to offer denial choices should be another
// control instead of Combobox. See crbug/305494.
class TranslateDenialComboboxModel : public ui::ComboboxModel {
 public:
  enum {
    INDEX_NOPE = 0,
    INDEX_NEVER_TRANSLATE_LANGUAGE = 2,
    INDEX_NEVER_TRANSLATE_SITE = 4,
  };

  explicit TranslateDenialComboboxModel(
      const string16& original_language_name) {
    items_.push_back(l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY));
    items_.push_back(string16());
    items_.push_back(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
        original_language_name));
    items_.push_back(string16());
    items_.push_back(l10n_util::GetStringUTF16(
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE));
  }
  virtual ~TranslateDenialComboboxModel() {}

 private:
  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE {
    return items_.size();
  }
  virtual string16 GetItemAt(int index) OVERRIDE {
    return items_[index];
  }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return items_[index].empty();
  }
  virtual int GetDefaultIndex() const OVERRIDE {
    return 0;
  }

  std::vector<string16> items_;

  DISALLOW_COPY_AND_ASSIGN(TranslateDenialComboboxModel);
};

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
void TranslateBubbleView::ShowBubble(views::View* anchor_view,
                                     content::WebContents* web_contents,
                                     TranslateBubbleModel::ViewState type,
                                     Browser* browser) {
  if (IsShowing()) {
    translate_bubble_view_->SwitchView(type);
    return;
  }

  std::string source_language;
  std::string target_language;
  GetTranslateLanguages(web_contents, &source_language, &target_language);

  scoped_ptr<TranslateUIDelegate> ui_delegate(
      new TranslateUIDelegate(web_contents, source_language, target_language));
  scoped_ptr<TranslateBubbleModel> model(
      new TranslateBubbleModelImpl(type, ui_delegate.Pass()));
  bool is_in_incognito_window =
      web_contents->GetBrowserContext()->IsOffTheRecord();
  TranslateBubbleView* view = new TranslateBubbleView(anchor_view,
                                                      model.Pass(),
                                                      is_in_incognito_window,
                                                      browser);
  views::BubbleDelegateView::CreateBubble(view)->Show();
}

// static
bool TranslateBubbleView::IsShowing() {
  return translate_bubble_view_ != NULL;
}

// static
TranslateBubbleView* TranslateBubbleView::GetCurrentBubble() {
  return translate_bubble_view_;
}

void TranslateBubbleView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        0, 0, 0));

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
}

void TranslateBubbleView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  HandleButtonPressed(static_cast<ButtonID>(sender->id()));
}

void TranslateBubbleView::WindowClosing() {
  if (!translate_executed_ &&
      (browser_ == NULL || !browser_->IsAttemptingToCloseBrowser())) {
    model_->TranslationDeclined();
  }

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
  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size TranslateBubbleView::GetPreferredSize() {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    views::View* child = child_at(i);
    width = std::max(width, child->GetPreferredSize().width());
  }
  int height = GetCurrentView()->GetPreferredSize().height();
  return gfx::Size(width, height);
}

void TranslateBubbleView::OnSelectedIndexChanged(views::Combobox* combobox) {
  HandleComboboxSelectedIndexChanged(static_cast<ComboboxID>(combobox->id()));
}

void TranslateBubbleView::LinkClicked(views::Link* source, int event_flags) {
  HandleLinkClicked(static_cast<LinkID>(source->id()));
}

TranslateBubbleModel::ViewState TranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

TranslateBubbleView::TranslateBubbleView(
    views::View* anchor_view,
    scoped_ptr<TranslateBubbleModel> model,
    bool is_in_incognito_window,
    Browser* browser)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      before_translate_view_(NULL),
      translating_view_(NULL),
      after_translate_view_(NULL),
      error_view_(NULL),
      advanced_view_(NULL),
      denial_combobox_(NULL),
      source_language_combobox_(NULL),
      target_language_combobox_(NULL),
      always_translate_checkbox_(NULL),
      model_(model.Pass()),
      is_in_incognito_window_(is_in_incognito_window),
      browser_(browser),
      translate_executed_(false) {
  if (model_->GetViewState() !=
      TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
    translate_executed_ = true;
  }

  set_margins(gfx::Insets(views::kPanelVertMargin, views::kPanelHorizMargin,
                          views::kPanelVertMargin, views::kPanelHorizMargin));

  translate_bubble_view_ = this;
}

views::View* TranslateBubbleView::GetCurrentView() {
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
      translate_executed_ = true;
      model_->Translate();
      break;
    }
    case BUTTON_ID_DONE: {
      if (always_translate_checkbox_)
        model_->SetAlwaysTranslate(always_translate_checkbox_->checked());
      translate_executed_ = true;
      model_->Translate();
      break;
    }
    case BUTTON_ID_CANCEL: {
      model_->GoBackFromAdvanced();
      UpdateChildVisibilities();
      SizeToContents();
      break;
    }
    case BUTTON_ID_TRY_AGAIN: {
      translate_executed_ = true;
      model_->Translate();
      break;
    }
    case BUTTON_ID_SHOW_ORIGINAL: {
      model_->RevertTranslation();
      StartFade(false);
      break;
    }
    case BUTTON_ID_ALWAYS_TRANSLATE: {
      // Do nothing. The state of the checkbox affects only when the 'Done'
      // button is pressed.
      break;
    }
  }
}

void TranslateBubbleView::HandleLinkClicked(
    TranslateBubbleView::LinkID sender_id) {
  switch (sender_id) {
    case LINK_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      break;
    }
    case LINK_ID_LEARN_MORE: {
      browser_->OpenURL(content::OpenURLParams(
          GURL(chrome::kAboutGoogleTranslateURL),
          content::Referrer(),
          NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK,
          false));
      break;
    }
  }
}

void TranslateBubbleView::HandleComboboxSelectedIndexChanged(
    TranslateBubbleView::ComboboxID sender_id) {
  switch (sender_id) {
    case COMBOBOX_ID_DENIAL: {
      int index = denial_combobox_->selected_index();
      switch (index) {
        case TranslateDenialComboboxModel::INDEX_NOPE:
          if (!translate_executed_)
            model_->TranslationDeclined();
          StartFade(false);
          break;
        case TranslateDenialComboboxModel::INDEX_NEVER_TRANSLATE_LANGUAGE:
          model_->SetNeverTranslateLanguage(true);
          StartFade(false);
          break;
        case TranslateDenialComboboxModel::INDEX_NEVER_TRANSLATE_SITE:
          model_->SetNeverTranslateSite(true);
          StartFade(false);
          break;
        default:
          NOTREACHED();
          break;
      }
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
  views::Label* message_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE));

  string16 original_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  denial_combobox_ = new views::Combobox(
      new TranslateDenialComboboxModel(original_language_name));
  denial_combobox_->set_id(COMBOBOX_ID_DENIAL);
  denial_combobox_->set_listener(this);

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_CONTENT,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(message_label);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_ADVANCED,
                             LINK_ID_ADVANCED));

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_LEARN_MORE,
                             LINK_ID_LEARN_MORE));
  layout->AddView(denial_combobox_);
  layout->AddView(CreateLabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT),
      BUTTON_ID_TRANSLATE));

  return view;
}

views::View* TranslateBubbleView::CreateViewTranslating() {
  string16 target_language_name =
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
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_LEARN_MORE,
                             LINK_ID_LEARN_MORE));
  views::LabelButton* revert_button = CreateLabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT),
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
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(label);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_ADVANCED,
                             LINK_ID_ADVANCED));

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_LEARN_MORE,
                             LINK_ID_LEARN_MORE));
  layout->AddView(CreateLabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT),
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
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_CONTENT);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_MESSAGE);
  layout->AddView(label);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_ADVANCED,
                             LINK_ID_ADVANCED));

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_CONTENT);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_LEARN_MORE,
                             LINK_ID_LEARN_MORE));
  layout->AddView(CreateLabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN),
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
    always_translate_checkbox_ = new views::Checkbox(string16());
    always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    always_translate_checkbox_->set_listener(this);
  }

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  using views::GridLayout;

  enum {
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_ALWAYS_TRANSLATE,
    COLUMN_SET_ID_BUTTONS,
  };

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  if (!is_in_incognito_window_) {
    cs = layout->AddColumnSet(COLUMN_SET_ID_ALWAYS_TRANSLATE);
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                  0, GridLayout::USE_PREF, 0, 0);
    cs->AddPaddingColumn(1, 0);
  }

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(source_language_label);
  layout->AddView(source_language_combobox_);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(target_language_label);
  layout->AddView(target_language_combobox_);

  if (!is_in_incognito_window_) {
    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

    layout->StartRow(0, COLUMN_SET_ID_ALWAYS_TRANSLATE);
    layout->AddView(always_translate_checkbox_);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, COLUMN_SET_ID_BUTTONS);
  layout->AddView(CreateLink(this,
                             IDS_TRANSLATE_BUBBLE_LEARN_MORE,
                             LINK_ID_LEARN_MORE));
  views::LabelButton* cancel_button = CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL), BUTTON_ID_CANCEL);
  layout->AddView(cancel_button);
  views::LabelButton* done_button = CreateLabelButton(
      this, l10n_util::GetStringUTF16(IDS_DONE), BUTTON_ID_DONE);
  done_button->SetIsDefault(true);
  layout->AddView(done_button);

  UpdateAdvancedView();

  return view;
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  if (model_->GetViewState() == view_state)
    return;

  model_->SetViewState(view_state);
  UpdateChildVisibilities();
  SizeToContents();
}

void TranslateBubbleView::UpdateAdvancedView() {
  DCHECK(source_language_combobox_);
  DCHECK(target_language_combobox_);

  string16 source_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());

  string16 message =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS,
                                 source_language_name,
                                 target_language_name);
  // "Always translate" checkbox doesn't exist in an incognito window.
  if (always_translate_checkbox_) {
    always_translate_checkbox_->SetText(message);
    always_translate_checkbox_->SetChecked(
        model_->ShouldAlwaysTranslate());
  }
}
