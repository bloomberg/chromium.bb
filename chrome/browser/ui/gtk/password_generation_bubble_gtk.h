// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/password_form.h"

namespace autofill {
class PasswordGenerator;
}

class BubbleGtk;
class TabContents;

// PasswordGenerationBubbleGtk is a bubble use to show possible generated
// passwords to users. It is set in page content, anchored at |anchor_rect|.
// If the generated password is accepted by the user, the renderer associated
// with |render_view_host| and the |password_manager| are informed.
class PasswordGenerationBubbleGtk {
 public:
  PasswordGenerationBubbleGtk(const gfx::Rect& anchor_rect,
                              const webkit::forms::PasswordForm& form,
                              TabContents* tab,
                              autofill::PasswordGenerator* password_generator);
  virtual ~PasswordGenerationBubbleGtk();

 private:
  CHROMEGTK_CALLBACK_0(PasswordGenerationBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(PasswordGenerationBubbleGtk, void, OnAcceptClicked);
  CHROMEG_CALLBACK_0(
      PasswordGenerationBubbleGtk, void, OnLearnMoreLinkClicked, GtkButton*);

  BubbleGtk* bubble_;
  GtkWidget* text_field_;

  // Form that contains the password field that we are generating a password
  // for. Used by the password_manager_.
  webkit::forms::PasswordForm form_;

  // TabContents associated with the button that spawned this bubble.
  TabContents* tab_;

  // Object that deals with generating passwords. The class won't take the
  // ownership of it.
  autofill::PasswordGenerator* password_generator_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_
