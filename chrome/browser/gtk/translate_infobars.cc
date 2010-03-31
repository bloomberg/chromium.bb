// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/translate_infobars.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/slide_animation.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/common/notification_service.h"
#include "gfx/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// Reorders the widgets in the NULL terminated array |widgets| that are all
// children of |box|.
void ReorderWidgetsTo(GtkWidget* box, GtkWidget** widgets) {
  for (int count = 0; *widgets != NULL; widgets++) {
    gtk_box_reorder_child(GTK_BOX(box), *widgets, count++);
    gtk_widget_show_all(*widgets);
  }
}

// Creates a combobox set up to display text from a list of language codes
// (translating the codes into the display string).
GtkWidget* BuildLanguageComboboxFrom(
    TranslateInfoBarDelegate* delegate,
    const std::vector<std::string>& languages) {
  GtkListStore* model = gtk_list_store_new(1, G_TYPE_STRING);
  for (std::vector<std::string>::const_iterator iter = languages.begin();
       iter != languages.end(); ++iter) {
    GtkTreeIter tree_iter;
    std::string name = UTF16ToUTF8(delegate->GetDisplayNameForLocale(*iter));
    gtk_list_store_append(model, &tree_iter);
    gtk_list_store_set(model, &tree_iter, 0, name.c_str(), -1);
  }

  GtkWidget* combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));
  g_object_unref(model);
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes(
      GTK_CELL_LAYOUT(combobox), renderer, "text", 0, NULL);

  return combobox;
}

// Builds a button with an arrow in it to emulate the menu-button style from
// the windows version.
GtkWidget* BuildOptionsMenuButton() {
  GtkWidget* button = gtk_button_new();
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(button));
  if (former_child)
    gtk_container_remove(GTK_CONTAINER(button), former_child);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_OPTIONS).c_str());
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  GtkWidget* arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(button), hbox);

  return button;
}

}  // namespace

TranslateInfoBar::TranslateInfoBar(TranslateInfoBarDelegate* delegate)
    : InfoBar(delegate),
      background_error_percent_(0),
      state_(TranslateInfoBarDelegate::kTranslateNone),
      translation_pending_(false),
      swapped_language_placeholders_(false) {
  // Initialize slide animation for transitioning to and from error state.
  error_animation_.reset(new SlideAnimation(this));
  error_animation_->SetTweenType(SlideAnimation::NONE);
  error_animation_->SetSlideDuration(500);

  BuildWidgets();

  // Register for PAGE_TRANSLATED notification.
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
      Source<TabContents>(GetDelegate()->tab_contents()));
}

TranslateInfoBar::~TranslateInfoBar() {
}

void TranslateInfoBar::GetTopColor(InfoBarDelegate::Type type,
                                   double* r, double* g, double *b) {
  if (background_error_percent_ <= 0) {
    InfoBar::GetTopColor(InfoBarDelegate::PAGE_ACTION_TYPE, r, g, b);
  } else if (background_error_percent_ >= 1) {
    InfoBar::GetTopColor(InfoBarDelegate::ERROR_TYPE, r, g, b);
  } else {
    double normal_r, normal_g, normal_b;
    InfoBar::GetTopColor(InfoBarDelegate::PAGE_ACTION_TYPE,
                         &normal_r, &normal_g, &normal_b);

    double error_r, error_g, error_b;
    InfoBar::GetTopColor(InfoBarDelegate::ERROR_TYPE,
                         &error_r, &error_g, &error_b);

    double offset_r = error_r - normal_r;
    double offset_g = error_g - normal_g;
    double offset_b = error_b - normal_b;

    *r = normal_r + (background_error_percent_ * offset_r);
    *g = normal_g + (background_error_percent_ * offset_g);
    *b = normal_b + (background_error_percent_ * offset_b);
  }
}

void TranslateInfoBar::GetBottomColor(InfoBarDelegate::Type type,
                                      double* r, double* g, double *b) {
  if (background_error_percent_ <= 0) {
    InfoBar::GetBottomColor(InfoBarDelegate::PAGE_ACTION_TYPE, r, g, b);
  } else if (background_error_percent_ >= 1) {
    InfoBar::GetBottomColor(InfoBarDelegate::ERROR_TYPE, r, g, b);
  } else {
    double normal_r, normal_g, normal_b;
    InfoBar::GetBottomColor(InfoBarDelegate::PAGE_ACTION_TYPE,
                            &normal_r, &normal_g, &normal_b);

    double error_r, error_g, error_b;
    InfoBar::GetBottomColor(InfoBarDelegate::ERROR_TYPE,
                            &error_r, &error_g, &error_b);

    double offset_r = error_r - normal_r;
    double offset_g = error_g - normal_g;
    double offset_b = error_b - normal_b;

    *r = normal_r + (background_error_percent_ * offset_r);
    *g = normal_g + (background_error_percent_ * offset_g);
    *b = normal_b + (background_error_percent_ * offset_b);
  }
}

void TranslateInfoBar::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type.value == NotificationType::PAGE_TRANSLATED) {
    TabContents* tab = Source<TabContents>(source).ptr();
    if (tab != GetDelegate()->tab_contents())
      return;

    PageTranslatedDetails* page_translated_details =
        Details<PageTranslatedDetails>(details).ptr();
    UpdateState((page_translated_details->error_type == TranslateErrors::NONE ?
                 TranslateInfoBarDelegate::kAfterTranslate :
                 TranslateInfoBarDelegate::kTranslateError), false,
                page_translated_details->error_type);
  } else {
    InfoBar::Observe(type, source, details);
  }
}

void TranslateInfoBar::AnimationProgressed(const Animation* animation) {
  background_error_percent_ = animation->GetCurrentValue();
  // Queue the info bar widget for redisplay.
  gtk_widget_queue_draw(widget());
}

bool TranslateInfoBar::IsCommandIdChecked(int command_id) const {
  TranslateInfoBarDelegate* translate_delegate = GetDelegate();
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG :
      return translate_delegate->IsLanguageBlacklisted();

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE :
      return translate_delegate->IsSiteBlacklisted();

    case IDC_TRANSLATE_OPTIONS_ALWAYS :
      return translate_delegate->ShouldAlwaysTranslate();

    default:
      NOTREACHED() << "Invalid command_id from menu";
      break;
  }
  return false;
}

bool TranslateInfoBar::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool TranslateInfoBar::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void TranslateInfoBar::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
      GetDelegate()->ToggleLanguageBlacklist();
      break;

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
      GetDelegate()->ToggleSiteBlacklist();
      break;

    case IDC_TRANSLATE_OPTIONS_ALWAYS:
      GetDelegate()->ToggleAlwaysTranslate();
      break;

    case IDC_TRANSLATE_OPTIONS_ABOUT: {
      TabContents* tab_contents = GetDelegate()->tab_contents();
      if (tab_contents) {
        string16 url = l10n_util::GetStringUTF16(
            IDS_ABOUT_GOOGLE_TRANSLATE_URL);
        tab_contents->OpenURL(GURL(url), GURL(), NEW_FOREGROUND_TAB,
                              PageTransition::LINK);
      }
      break;
    }

    default:
      NOTREACHED() << "Invalid command id from menu.";
      break;
  }
}

void TranslateInfoBar::BuildWidgets() {
  translate_box_ = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_widget_set_no_show_all(translate_box_, TRUE);

  // Add all labels to the translate_box_. Unlike views or cocoa, there's no
  // concept of manual layout in GTK, so we'll manually reorder these widgets
  // inside |translate_box_| and selectively show and hide them.
  label_1_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(translate_box_), label_1_, FALSE, FALSE, 0);
  gtk_widget_modify_fg(label_1_, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  label_2_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(translate_box_), label_2_, FALSE, FALSE, 0);
  gtk_widget_modify_fg(label_2_, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  label_3_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(translate_box_), label_3_, FALSE, FALSE, 0);
  gtk_widget_modify_fg(label_3_, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  translating_label_ = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_TRANSLATING).c_str());
  gtk_box_pack_start(GTK_BOX(translate_box_), translating_label_,
                     FALSE, FALSE, 0);
  gtk_widget_modify_fg(translating_label_, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  error_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(translate_box_), error_label_,
                     FALSE, FALSE, 0);
  gtk_widget_modify_fg(error_label_, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  accept_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_ACCEPT).c_str());
  g_signal_connect(accept_button_, "clicked",
                   G_CALLBACK(&OnAcceptPressedThunk), this);
  accept_button_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, accept_button_, false, 0);

  deny_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_DENY).c_str());
  g_signal_connect(deny_button_, "clicked",
                   G_CALLBACK(&OnDenyPressedThunk), this);
  deny_button_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, deny_button_, false, 0);

  retry_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_RETRY).c_str());
  g_signal_connect(retry_button_, "clicked",
                   G_CALLBACK(&OnAcceptPressedThunk), this);
  retry_button_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, retry_button_, false, 0);

  revert_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_REVERT).c_str());
  g_signal_connect(revert_button_, "clicked",
                   G_CALLBACK(&OnAcceptPressedThunk), this);
  revert_button_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, revert_button_, false, 0);

  std::vector<std::string> orig_languages;
  GetDelegate()->GetAvailableOriginalLanguages(&orig_languages);
  original_language_combobox_ = BuildLanguageComboboxFrom(GetDelegate(),
                                                          orig_languages);
  g_signal_connect(original_language_combobox_, "changed",
                   G_CALLBACK(&OnOriginalModifiedThunk), this);
  original_language_combobox_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, original_language_combobox_, false, 0);

  std::vector<std::string> target_languages;
  GetDelegate()->GetAvailableTargetLanguages(&target_languages);
  target_language_combobox_ = BuildLanguageComboboxFrom(GetDelegate(),
                                                        target_languages);
  g_signal_connect(target_language_combobox_, "changed",
                   G_CALLBACK(&OnTargetModifiedThunk), this);
  target_language_combobox_vbox_ = gtk_util::CenterWidgetInHBox(
      translate_box_, target_language_combobox_, false, 0);

  gtk_box_pack_start(GTK_BOX(hbox_), translate_box_, FALSE, FALSE, 0);

  // The options button sits outside the translate_box so that it can be end
  // packed in hbox_.
  options_menu_button_ = BuildOptionsMenuButton();
  g_signal_connect(options_menu_button_, "clicked",
                   G_CALLBACK(&OnOptionsClickedThunk), this);
  gtk_widget_show_all(options_menu_button_);
  gtk_util::CenterWidgetInHBox(
      hbox_, options_menu_button_, true, 0);

  UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
              GetDelegate()->error_type());

  gtk_widget_show_all(border_bin_.get());
}

void TranslateInfoBar::UpdateState(
    TranslateInfoBarDelegate::TranslateState new_state,
    bool new_translation_pending, TranslateErrors::Type error_type) {
  if (state_ == new_state && translation_pending_ == new_translation_pending)
    return;

  TranslateInfoBarDelegate::TranslateState old_state = state_;
  state_ = new_state;
  translation_pending_ = new_translation_pending;

  // Show the box...
  gtk_widget_show(translate_box_);
  // ...but hide all children of the translate box. They will be selectively
  // unhidden and reordered based on the current translate state.
  gtk_container_foreach(
      GTK_CONTAINER(translate_box_),
      reinterpret_cast<void (*)(GtkWidget*, void*)>(gtk_widget_hide_all), NULL);

  switch (state_) {
    case TranslateInfoBarDelegate::kBeforeTranslate: {
      SetLabels();

      GtkWidget* before_state[] = {
        label_1_,
        original_language_combobox_vbox_,
        label_2_,
        translation_pending_ ? translating_label_ : accept_button_vbox_,
        translation_pending_ ? NULL : deny_button_vbox_,
        NULL
      };
      ReorderWidgetsTo(translate_box_, before_state);

      gtk_combo_box_set_active(GTK_COMBO_BOX(original_language_combobox_),
                               GetDelegate()->original_lang_index());
      break;
    }
    case TranslateInfoBarDelegate::kAfterTranslate: {
      SetLabels();
      GtkWidget* first_button_box =
          swapped_language_placeholders_ ?
          target_language_combobox_vbox_ : original_language_combobox_vbox_;
      GtkWidget* second_button_box =
          swapped_language_placeholders_ ?
          original_language_combobox_vbox_ : target_language_combobox_vbox_;

      GtkWidget* after_state[] = {
        label_1_,
        first_button_box,
        label_2_,
        second_button_box,
        label_3_,
        translation_pending_ ? translating_label_ : revert_button_vbox_,
        NULL
      };
      ReorderWidgetsTo(translate_box_, after_state);

      gtk_combo_box_set_active(GTK_COMBO_BOX(original_language_combobox_),
                               GetDelegate()->original_lang_index());
      gtk_combo_box_set_active(GTK_COMBO_BOX(target_language_combobox_),
                               GetDelegate()->target_lang_index());
      break;
    }
    case TranslateInfoBarDelegate::kTranslateError: {
      string16 error_message_utf16 = GetDelegate()->GetErrorMessage(error_type);
      gtk_label_set_text(GTK_LABEL(error_label_),
                         UTF16ToUTF8(error_message_utf16).c_str());

      GtkWidget* error_state[] = {
        error_label_,
        retry_button_vbox_,
        NULL
      };
      ReorderWidgetsTo(translate_box_, error_state);
      break;
    }
    default: {
      NOTIMPLEMENTED() << "Received state " << new_state;
    }
  }

  // The options button is visible any time that we aren't in an error state.
  if (state_ == TranslateInfoBarDelegate::kTranslateError)
    gtk_widget_hide(options_menu_button_);
  else
    gtk_widget_show(options_menu_button_);

  // If background should change per state, trigger animation of transition
  // accordingly.
  if (old_state != TranslateInfoBarDelegate::kTranslateError &&
      state_ == TranslateInfoBarDelegate::kTranslateError) {
    error_animation_->Show();  // Transition to error state.
  } else if (old_state == TranslateInfoBarDelegate::kTranslateError &&
             state_ != TranslateInfoBarDelegate::kTranslateError) {
    error_animation_->Hide();  // Transition from error state.
  } else {
    error_animation_->Stop();  // No transition.
  }
}

void TranslateInfoBar::SetLabels() {
  std::vector<size_t> offsets;
  string16 message_text_utf16;
  GetDelegate()->GetMessageText(GetDelegate()->state(), &message_text_utf16,
      &offsets, &swapped_language_placeholders_);

  string16 text = message_text_utf16.substr(0, offsets[0]);
  gtk_label_set_text(GTK_LABEL(label_1_), UTF16ToUTF8(text).c_str());

  text = message_text_utf16.substr(offsets[0], offsets[1] - offsets[0]);
  gtk_label_set_text(GTK_LABEL(label_2_), UTF16ToUTF8(text).c_str());

  if (offsets.size() == 3) {
    text = message_text_utf16.substr(offsets[1], offsets[2] - offsets[1]);
    gtk_label_set_text(GTK_LABEL(label_3_), UTF16ToUTF8(text).c_str());
  }
}

TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() const {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

void TranslateInfoBar::LanguageModified() {
  options_menu_model_.reset();

  // Selecting an item from the "from language" menu in the before translate
  // phase shouldn't trigger translation - http://crbug.com/36666
  if (GetDelegate()->state() == TranslateInfoBarDelegate::kAfterTranslate) {
    GetDelegate()->Translate();
    UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
                GetDelegate()->error_type());
  }
}

void TranslateInfoBar::OnOriginalModified(GtkWidget* sender) {
  int index = gtk_combo_box_get_active(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->original_lang_index())
    return;

  GetDelegate()->ModifyOriginalLanguage(index);

  LanguageModified();
}

void TranslateInfoBar::OnTargetModified(GtkWidget* sender) {
  int index = gtk_combo_box_get_active(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->target_lang_index())
    return;

  GetDelegate()->ModifyTargetLanguage(index);

  LanguageModified();
}

void TranslateInfoBar::OnAcceptPressed(GtkWidget* sender) {
  GetDelegate()->Translate();
  UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
              GetDelegate()->error_type());
}

void TranslateInfoBar::OnDenyPressed(GtkWidget* sender) {
  GetDelegate()->TranslationDeclined();
  RemoveInfoBar();
}

void TranslateInfoBar::OnRevertPressed(GtkWidget* sender) {
  GetDelegate()->RevertTranslation();
}

void TranslateInfoBar::OnOptionsClicked(GtkWidget* sender) {
  if (!options_menu_model_.get()) {
    options_menu_model_.reset(new OptionsMenuModel(this, GetDelegate()));
    options_menu_menu_.reset(new MenuGtk(this, options_menu_model_.get()));
  }
  options_menu_menu_->Popup(sender, 1, gtk_get_current_event_time());
}

// TranslateInfoBarDelegate, InfoBarDelegate overrides: ------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  return new TranslateInfoBar(this);
}
