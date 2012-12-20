// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/gfx/size.h"

class WebIntentPickerDelegate;
class WebIntentPickerModel;

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}

// Base class for the web intent picker dialog.
class WebIntentPicker {
 public:
  // The minimum width of the window.
  static const int kWindowMinWidth = 400;

  // The maximum width the window.
  static const int kWindowMaxWidth = 900;

  // The minimum height of the window.
  static const int kWindowMinHeight = 145;

  // The maximum width in view units of a suggested extension's title link.
  static const int kTitleLinkMaxWidth = 130;

  // The space in pixels between the top-level groups and the dialog border.
  static const int kContentAreaBorder = 20;

  // Vertical space above the separator.
  static const int kHeaderSeparatorPaddingTop = 16;

  // Vertical space below the separator.
  static const int kHeaderSeparatorPaddingBottom = 7;

  // Width of the service icon.
  static const int kServiceIconWidth = 16;

  // Height of the service icon.
  static const int kServiceIconHeight = 16;

  // Space between icon and text.
  static const int kIconTextPadding = 10;

  // Space between star rating and select button.
  static const int kStarButtonPadding = 20;

  // The height of the suggested and installed service row.
  static const int kServiceRowHeight = 32;

  // The maximum number of installed services + suggested servcies to show. Note
  // that all installed services are always shown so the actual number of
  // services shown maybe greater than this.
  static const int kMaxServicesToShow = 4;

  // Platform specific factory function. This function will automatically show
  // the picker.
  static WebIntentPicker* Create(content::WebContents* web_contents,
                                 WebIntentPickerDelegate* delegate,
                                 WebIntentPickerModel* model);

  // Hides the UI for this picker, and destroys its UI.
  virtual void Close() = 0;

  // Sets the action string of the picker, e.g.,
  // "Which service should be used for sharing?".
  virtual void SetActionString(const string16& action) = 0;

  // Called when an extension is successfully installed via the picker.
  virtual void OnExtensionInstallSuccess(const std::string& id) = 0;

  // Called when an extension installation started via the picker has failed.
  virtual void OnExtensionInstallFailure(const std::string& id) = 0;

  // Shows the default extension install dialog. Override this to show a custom
  // dialog. We *MUST* eventually call either Proceed() or Abort() on
  // |delegate|.
  virtual void OnShowExtensionInstallDialog(
      const ExtensionInstallPrompt::ShowParams& show_params,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt);

  // Called when the inline disposition experiences an auto-resize.
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) = 0;

  virtual void OnInlineDispositionHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) {}

  // Called when the controller has finished all pending asynchronous
  // activities.
  virtual void OnPendingAsyncCompleted() = 0;

  // Called once the delegate gets destroyed/invalid. This should only be
  // called during a shut down sequence that will tear down the picker, too.
  virtual void InvalidateDelegate() = 0;

  // Called when the inline disposition's web contents have been loaded.
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) {}

  // Get the minimum size of the inline disposition content container.
  virtual gfx::Size GetMinInlineDispositionSize();

  // Get the maximum size of the inline disposition content container.
  virtual gfx::Size GetMaxInlineDispositionSize();

  // Get the star image IDs to use for the nth star (out of 5), given a
  // |rating| in the range [0, 5].
  static int GetNthStarImageIdFromCWSRating(double rating, int index);

  // Returns the action-specific string to display for |action|.
  static string16 GetDisplayStringForIntentAction(const string16& action16);

 protected:
  virtual ~WebIntentPicker() {}
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
