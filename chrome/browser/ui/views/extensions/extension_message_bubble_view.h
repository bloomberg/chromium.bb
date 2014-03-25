// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"

class Profile;
class BrowserActionsContainer;
class ToolbarView;

namespace views {
class View;
}

namespace extensions {
class DevModeBubbleController;

// Create and show ExtensionMessageBubbles for either extensions that look
// suspicious and have therefore been disabled, or for extensions that are
// running in developer mode that we want to warn the user about.
// Calling MaybeShow() will show one of the bubbles, if there is cause to (we
// don't show both in order to avoid spamminess). The suspicious extensions
// bubble takes priority over the developer mode extensions bubble.
class ExtensionMessageBubbleFactory : public BrowserActionsContainerObserver {
 public:
  ExtensionMessageBubbleFactory(Profile* profile, ToolbarView* toolbar_view);
  virtual ~ExtensionMessageBubbleFactory();

  void MaybeShow(views::View* anchor_view);

 private:
  // The stage of showing the developer mode extensions bubble. STAGE_START
  // corresponds to the beginning of the process, when nothing has been done.
  // STAGE_HIGHLIGHTED indicates that the toolbar should be highlighting
  // dangerous extensions. STAGE_COMPLETE means that the process should be
  // ended.
  enum Stage { STAGE_START, STAGE_HIGHLIGHTED, STAGE_COMPLETE };

  // Shows the suspicious extensions bubble, if there are suspicious extensions
  // and we have not done so already.
  // Returns true if we have show the view.
  bool MaybeShowSuspiciousExtensionsBubble(views::View* anchor_view);

  // Shows the developer mode extensions bubble, if there are extensions running
  // in developer mode and we have not done so already.
  // Returns true if we show the view (or start the process).
  bool MaybeShowDevModeExtensionsBubble(views::View* anchor_view);

  // Starts or stops observing the BrowserActionsContainer, if necessary.
  void MaybeObserve();
  void MaybeStopObserving();

  // BrowserActionsContainer::Observer implementation.
  virtual void OnBrowserActionsContainerAnimationEnded() OVERRIDE;
  virtual void OnBrowserActionsContainerDestroyed() OVERRIDE;

  // Inform the ExtensionToolbarModel to highlight the appropriate extensions.
  void HighlightDevModeExtensions();

  // Shows the developer mode bubble, after highlighting the extensions.
  void ShowDevModeBubble();

  // Finishes the process of showing the developer mode bubble.
  void Finish();

  // The associated profile.
  Profile* profile_;

  // The toolbar view that the ExtensionMessageBubbleViews will attach to.
  ToolbarView* toolbar_view_;

  // Whether or not we have shown the suspicious extensions bubble.
  bool shown_suspicious_extensions_bubble_;

  // Whether or not we have shown the developer mode extensions bubble.
  bool shown_dev_mode_extensions_bubble_;

  // Whether or not we are already observing the BrowserActionsContainer (so
  // we don't add ourselves twice).
  bool is_observing_;

  // The current stage of showing the bubble.
  Stage stage_;

  // The BrowserActionsContainer for the profile. This will be NULL if the
  // factory is not currently in the process of showing a bubble.
  BrowserActionsContainer* container_;

  // The default view to anchor the bubble to. This will be NULL if the factory
  // is not currently in the process of showing a bubble.
  views::View* anchor_view_;

  // The DevModeBubbleController to use. This will be NULL if the factory is not
  // currently in the process of showing a bubble.
  scoped_ptr<DevModeBubbleController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
