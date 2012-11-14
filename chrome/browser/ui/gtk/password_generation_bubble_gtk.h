// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/common/password_generation_util.h"
#include "content/public/common/password_form.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/rect.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
class WebContents;
}

// PasswordGenerationBubbleGtk is a bubble use to show possible generated
// passwords to users. It is set in page content, anchored at |anchor_rect|.
// If the generated password is accepted by the user, the renderer associated
// with |render_view_host| and the |password_manager| are informed.
class PasswordGenerationBubbleGtk : public BubbleDelegateGtk {
 public:
  PasswordGenerationBubbleGtk(const gfx::Rect& anchor_rect,
                              const content::PasswordForm& form,
                              content::WebContents* web_contents,
                              autofill::PasswordGenerator* password_generator);
  virtual ~PasswordGenerationBubbleGtk();

  // Overridden from BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_0(PasswordGenerationBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(PasswordGenerationBubbleGtk, void, OnAcceptClicked);
  CHROMEGTK_CALLBACK_2(PasswordGenerationBubbleGtk, void, OnRegenerateClicked,
                       GtkEntryIconPosition, GdkEvent*);
  CHROMEGTK_CALLBACK_0(PasswordGenerationBubbleGtk, void, OnPasswordEdited);
  CHROMEG_CALLBACK_0(
      PasswordGenerationBubbleGtk, void, OnLearnMoreLinkClicked, GtkButton*);

  BubbleGtk* bubble_;
  GtkWidget* text_field_;

  // Form that contains the password field that we are generating a password
  // for. Used by the password_manager_.
  content::PasswordForm form_;

  // WebContents associated with the button that spawned this bubble.
  content::WebContents* web_contents_;

  // Object that deals with generating passwords. The class won't take the
  // ownership of it.
  autofill::PasswordGenerator* password_generator_;

  // Store various status of the current living bubble.
  password_generation::PasswordGenerationActions actions_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_PASSWORD_GENERATION_BUBBLE_GTK_H_
