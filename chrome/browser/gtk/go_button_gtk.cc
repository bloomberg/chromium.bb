// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/go_button_gtk.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// Limit the length of the tooltip text. This applies only to the text in the
// omnibox (e.g. X in "Go to X");
const size_t kMaxTooltipTextLength = 400;

GoButtonGtk::GoButtonGtk(LocationBarViewGtk* location_bar, Browser* browser)
    : location_bar_(location_bar),
      browser_(browser),
      button_delay_(0),
      stop_timer_(this),
      intended_mode_(MODE_GO),
      visible_mode_(MODE_GO),
      state_(BS_NORMAL),
      theme_provider_(browser ?
                      GtkThemeProvider::GetFrom(browser->profile()) : NULL),
      go_(theme_provider_, IDR_GO, IDR_GO_P, IDR_GO_H, 0, IDR_GO_MASK),
      stop_(theme_provider_, IDR_STOP, IDR_STOP_P, IDR_STOP_H, 0, IDR_GO_MASK),
      widget_(gtk_chrome_button_new()) {
  gtk_widget_set_size_request(widget_.get(), go_.Width(), go_.Height());

  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_.get(), FALSE);

  g_signal_connect(widget_.get(), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(widget_.get(), "enter",
                   G_CALLBACK(OnEnter), this);
  g_signal_connect(widget_.get(), "leave",
                   G_CALLBACK(OnLeave), this);
  g_signal_connect(widget_.get(), "clicked",
                   G_CALLBACK(OnClicked), this);
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);

  gtk_widget_set_has_tooltip(widget_.get(), TRUE);
  g_signal_connect(widget_.get(), "query-tooltip",
                   G_CALLBACK(OnQueryTooltipThunk), this);

  gtk_util::SetButtonTriggersNavigation(widget());

  if (theme_provider_) {
    theme_provider_->InitThemesFor(this);
    registrar_.Add(this,
                   NotificationType::BROWSER_THEME_CHANGED,
                   NotificationService::AllSources());
  }
}

GoButtonGtk::~GoButtonGtk() {
  widget_.Destroy();
}

void GoButtonGtk::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || (state() != BS_HOT) || ((mode == MODE_STOP) ?
      stop_timer_.empty() : (visible_mode_ != MODE_STOP))) {
    stop_timer_.RevokeAll();
    visible_mode_ = mode;
    gtk_widget_queue_draw(widget_.get());

    UpdateThemeButtons();
  }
}

void GoButtonGtk::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);

  GtkThemeProvider* provider = static_cast<GtkThemeProvider*>(
      Source<GtkThemeProvider>(source).ptr());
  DCHECK(provider == theme_provider_);
  UpdateThemeButtons();
}

Task* GoButtonGtk::CreateButtonTimerTask() {
  return stop_timer_.NewRunnableMethod(&GoButtonGtk::OnButtonTimer);
}

void GoButtonGtk::OnButtonTimer() {
  stop_timer_.RevokeAll();
  ChangeMode(intended_mode_, true);
}

// static
gboolean GoButtonGtk::OnExpose(GtkWidget* widget,
                               GdkEventExpose* e,
                               GoButtonGtk* button) {
  if (button->theme_provider_ && button->theme_provider_->UseGtkTheme()) {
    return FALSE;
  } else {
    if (button->visible_mode_ == MODE_GO) {
      return button->go_.OnExpose(widget, e);
    } else {
      return button->stop_.OnExpose(widget, e);
    }
  }
}

// static
gboolean GoButtonGtk::OnEnter(GtkButton* widget, GoButtonGtk* button) {
  DCHECK_EQ(BS_NORMAL, button->state());
  button->state_ = BS_HOT;
  return TRUE;
}

// static
gboolean GoButtonGtk::OnLeave(GtkButton* widget, GoButtonGtk* button) {
  // It's possible on shutdown for a "leave" event to be emitted twice in a row
  // for this button.  I'm not sure if this is a gtk quirk or something wrong
  // with our usage, but it's harmless.  I'm commenting out this DCHECK for now.
  // and adding a LOG(WARNING) instead.
  // See http://www.crbug.com/10851 for details.
  // DCHECK_EQ(BS_HOT, button->state());
  if (button->state() != BS_HOT)
    LOG(WARNING) << "Button state should be BS_HOT when leaving.";
  button->state_ = BS_NORMAL;
  button->ChangeMode(button->intended_mode_, true);
  return TRUE;
}

// static
gboolean GoButtonGtk::OnClicked(GtkButton* widget, GoButtonGtk* button) {
  if (button->visible_mode_ == MODE_STOP) {
    if (button->browser_)
      button->browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    button->ChangeMode(MODE_GO, true);
  } else if (button->visible_mode_ == MODE_GO && button->stop_timer_.empty()) {
    // If the go button is visible and not within the double click timer, go.
    GdkEventButton* event =
        reinterpret_cast<GdkEventButton*>(gtk_get_current_event());
    if (button->browser_) {
      button->browser_->ExecuteCommandWithDisposition(IDC_GO,
          event_utils::DispositionFromEventFlags(event->state));
    }

    // Figure out the system double-click time.
    if (button->button_delay_ == 0) {
      GtkSettings* settings = gtk_settings_get_default();
      g_object_get(G_OBJECT(settings),
                   "gtk-double-click-time",
                   &button->button_delay_,
                   NULL);
    }

    // Stop any existing timers.
    button->stop_timer_.RevokeAll();

    // Start a timer - while this timer is running, the go button
    // cannot be changed to a stop button. We do not set intended_mode_
    // to MODE_STOP here as we want to wait for the browser to tell
    // us that it has started loading (and this may occur only after
    // some delay).
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            button->CreateButtonTimerTask(),
                                            button->button_delay_);
  }

  return TRUE;
}

gboolean GoButtonGtk::OnQueryTooltip(GtkTooltip* tooltip) {
  // |location_bar_| can be NULL in tests.
  if (!location_bar_)
    return FALSE;

  std::string text;
  if (visible_mode_ == MODE_GO) {
    std::wstring current_text_wstr(location_bar_->location_entry()->GetText());
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      l10n_util::WrapStringWithLTRFormatting(&current_text_wstr);
    string16 current_text = WideToUTF16Hack(
        l10n_util::TruncateString(current_text_wstr, kMaxTooltipTextLength));

    AutocompleteEditModel* edit_model =
        location_bar_->location_entry()->model();
    if (edit_model->CurrentTextIsURL()) {
      text = l10n_util::GetStringFUTF8(IDS_TOOLTIP_GO_SITE, current_text);
    } else {
      std::wstring keyword(edit_model->keyword());
      TemplateURLModel* template_url_model =
          browser_->profile()->GetTemplateURLModel();
      const TemplateURL* provider =
          (keyword.empty() || edit_model->is_keyword_hint()) ?
          template_url_model->GetDefaultSearchProvider() :
          template_url_model->GetTemplateURLForKeyword(keyword);
      if (!provider)
        return FALSE;  // Don't show a tooltip.
      text = l10n_util::GetStringFUTF8(IDS_TOOLTIP_GO_SEARCH,
          WideToUTF16Hack(provider->AdjustedShortNameForLocaleDirection()),
          current_text);
    }
  } else {
    text = l10n_util::GetStringUTF8(IDS_TOOLTIP_STOP);
  }

  gtk_tooltip_set_text(tooltip, text.c_str());
  return TRUE;
}

void GoButtonGtk::UpdateThemeButtons() {
  bool use_gtk = theme_provider_ && theme_provider_->UseGtkTheme();

  if (use_gtk) {
    GdkPixbuf* pixbuf = NULL;
    if (intended_mode_ == MODE_GO) {
      pixbuf = theme_provider_->GetPixbufNamed(IDR_GO_NOBORDER_CENTER);
    } else {
      pixbuf = theme_provider_->GetPixbufNamed(IDR_STOP_NOBORDER_CENTER);
    }

    gtk_button_set_image(
        GTK_BUTTON(widget_.get()),
        gtk_image_new_from_pixbuf(pixbuf));

    gtk_widget_set_size_request(widget_.get(), -1, -1);
    gtk_widget_set_app_paintable(widget_.get(), FALSE);
    gtk_widget_set_double_buffered(widget_.get(), TRUE);
  } else {
    gtk_widget_set_size_request(widget_.get(), go_.Width(), go_.Height());

    gtk_widget_set_app_paintable(widget_.get(), TRUE);
    // We effectively double-buffer by virtue of having only one image...
    gtk_widget_set_double_buffered(widget_.get(), FALSE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget_.get()), use_gtk);
}
