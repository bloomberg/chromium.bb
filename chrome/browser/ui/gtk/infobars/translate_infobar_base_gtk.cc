// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/translate_infobar_base_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/after_translate_infobar_gtk.h"
#include "chrome/browser/ui/gtk/infobars/before_translate_infobar_gtk.h"
#include "chrome/browser/ui/gtk/infobars/translate_message_infobar_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "grit/generated_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"

namespace {

// To be able to map from language id <-> entry in the combo box, we
// store the language id in the combo box data model in addition to the
// displayed name.
enum {
  LANGUAGE_COMBO_COLUMN_ID,
  LANGUAGE_COMBO_COLUMN_NAME,
  LANGUAGE_COMBO_COLUMN_COUNT
};

}  // namespace

TranslateInfoBarBase::TranslateInfoBarBase(InfoBarService* owner,
                                           TranslateInfoBarDelegate* delegate)
    : InfoBarGtk(owner, delegate),
      background_error_percent_(0) {
  DCHECK(delegate);
  TranslateInfoBarDelegate::BackgroundAnimationType animation =
      delegate->background_animation_type();
  if (animation != TranslateInfoBarDelegate::NONE) {
    background_color_animation_.reset(new ui::SlideAnimation(this));
    background_color_animation_->SetTweenType(ui::Tween::LINEAR);
    background_color_animation_->SetSlideDuration(500);
    if (animation == TranslateInfoBarDelegate::NORMAL_TO_ERROR) {
      background_color_animation_->Show();
    } else {
      DCHECK_EQ(TranslateInfoBarDelegate::ERROR_TO_NORMAL, animation);
      // Hide() runs the animation in reverse.
      background_color_animation_->Reset(1.0);
      background_color_animation_->Hide();
    }
  } else {
    background_error_percent_ = delegate->IsError() ? 1 : 0;
  }
}

TranslateInfoBarBase::~TranslateInfoBarBase() {
}

void TranslateInfoBarBase::Init() {
  if (!ShowOptionsMenuButton())
    return;

  // The options button sits outside the translate_box so that it can be end
  // packed in hbox_.
  GtkWidget* options_menu_button = CreateMenuButton(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_OPTIONS));
  Signals()->Connect(options_menu_button, "clicked",
                     G_CALLBACK(&OnOptionsClickedThunk), this);
  gtk_widget_show_all(options_menu_button);
  gtk_util::CenterWidgetInHBox(hbox_, options_menu_button, true, 0);
}

void TranslateInfoBarBase::GetTopColor(InfoBarDelegate::Type type,
                                       double* r, double* g, double* b) {
  if (background_error_percent_ <= 0) {
    InfoBarGtk::GetTopColor(InfoBarDelegate::PAGE_ACTION_TYPE, r, g, b);
  } else if (background_error_percent_ >= 1) {
    InfoBarGtk::GetTopColor(InfoBarDelegate::WARNING_TYPE, r, g, b);
  } else {
    double normal_r, normal_g, normal_b;
    InfoBarGtk::GetTopColor(InfoBarDelegate::PAGE_ACTION_TYPE,
                            &normal_r, &normal_g, &normal_b);

    double error_r, error_g, error_b;
    InfoBarGtk::GetTopColor(InfoBarDelegate::WARNING_TYPE,
                            &error_r, &error_g, &error_b);

    double offset_r = error_r - normal_r;
    double offset_g = error_g - normal_g;
    double offset_b = error_b - normal_b;

    *r = normal_r + (background_error_percent_ * offset_r);
    *g = normal_g + (background_error_percent_ * offset_g);
    *b = normal_b + (background_error_percent_ * offset_b);
  }
}

void TranslateInfoBarBase::GetBottomColor(InfoBarDelegate::Type type,
                                          double* r, double* g, double* b) {
  if (background_error_percent_ <= 0) {
    InfoBarGtk::GetBottomColor(InfoBarDelegate::PAGE_ACTION_TYPE, r, g, b);
  } else if (background_error_percent_ >= 1) {
    InfoBarGtk::GetBottomColor(InfoBarDelegate::WARNING_TYPE, r, g, b);
  } else {
    double normal_r, normal_g, normal_b;
    InfoBarGtk::GetBottomColor(InfoBarDelegate::PAGE_ACTION_TYPE,
                               &normal_r, &normal_g, &normal_b);

    double error_r, error_g, error_b;
    InfoBarGtk::GetBottomColor(InfoBarDelegate::WARNING_TYPE,
                               &error_r, &error_g, &error_b);

    double offset_r = error_r - normal_r;
    double offset_g = error_g - normal_g;
    double offset_b = error_b - normal_b;

    *r = normal_r + (background_error_percent_ * offset_r);
    *g = normal_g + (background_error_percent_ * offset_g);
    *b = normal_b + (background_error_percent_ * offset_b);
  }
}

void TranslateInfoBarBase::AnimationProgressed(const ui::Animation* animation) {
  if (animation == background_color_animation_.get()) {
    background_error_percent_ = animation->GetCurrentValue();
    // Queue the info bar widget for redisplay so it repaints its background.
    gtk_widget_queue_draw(widget());
  } else {
    InfoBar::AnimationProgressed(animation);
  }
}

bool TranslateInfoBarBase::ShowOptionsMenuButton() const {
  return false;
}

GtkWidget* TranslateInfoBarBase::CreateLanguageCombobox(
    size_t selected_language,
    size_t exclude_language) {
  DCHECK(selected_language != exclude_language);

  GtkListStore* model = gtk_list_store_new(LANGUAGE_COMBO_COLUMN_COUNT,
                                           G_TYPE_INT, G_TYPE_STRING);
  bool set_selection = false;
  GtkTreeIter selected_iter;
  TranslateInfoBarDelegate* delegate = GetDelegate();
  for (size_t i = 0; i < delegate->num_languages(); ++i) {
    if (i == exclude_language)
      continue;
    GtkTreeIter tree_iter;
    const string16& name = delegate->language_name_at(i);

    gtk_list_store_append(model, &tree_iter);
    gtk_list_store_set(model, &tree_iter,
                       LANGUAGE_COMBO_COLUMN_ID, i,
                       LANGUAGE_COMBO_COLUMN_NAME, UTF16ToUTF8(name).c_str(),
                       -1);
    if (i == selected_language) {
      selected_iter = tree_iter;
      set_selection = true;
    }
  }

  GtkWidget* combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));
  if (set_selection)
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combobox), &selected_iter);
  g_object_unref(model);
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), renderer,
                                 "text", LANGUAGE_COMBO_COLUMN_NAME,
                                 NULL);
  return combobox;
}

// static
size_t TranslateInfoBarBase::GetLanguageComboboxActiveId(GtkComboBox* combo) {
  GtkTreeIter iter;
  if (!gtk_combo_box_get_active_iter(combo, &iter))
    return 0;

  gint id = 0;
  gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter,
                     LANGUAGE_COMBO_COLUMN_ID, &id,
                     -1);
  return static_cast<size_t>(id);
}

TranslateInfoBarDelegate* TranslateInfoBarBase::GetDelegate() {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

void TranslateInfoBarBase::OnOptionsClicked(GtkWidget* sender) {
  ShowMenuWithModel(sender, NULL, new OptionsMenuModel(GetDelegate()));
}

// TranslateInfoBarDelegate specific method:
InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  TranslateInfoBarBase* infobar = NULL;
  switch (infobar_type_) {
    case BEFORE_TRANSLATE:
      infobar = new BeforeTranslateInfoBar(owner, this);
      break;
    case AFTER_TRANSLATE:
      infobar = new AfterTranslateInfoBar(owner, this);
      break;
    case TRANSLATING:
    case TRANSLATION_ERROR:
      infobar = new TranslateMessageInfoBar(owner, this);
      break;
    default:
      NOTREACHED();
  }
  infobar->Init();
  return infobar;
}
