// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Profile;
class BrowserActionsContainer;
class ToolbarView;

namespace views {
class Label;
class LabelButton;
class View;
}

namespace extensions {

class DevModeBubbleController;
class ExtensionMessageBubbleController;

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

  // Shows the settings API extensions bubble, if there are extensions
  // overriding the startup pages and we have not done so already.
  // Returns true if we show the view (or start the process).
  bool MaybeShowStartupOverrideExtensionsBubble(views::View* anchor_view);

  // Shows the bubble for when there are extensions overriding the proxy (if we
  // have not done so already). Returns true if we show the view (or start the
  // process of doing so).
  bool MaybeShowProxyOverrideExtensionsBubble(views::View* anchor_view);

  // Shows the developer mode extensions bubble, if there are extensions running
  // in developer mode and we have not done so already.
  // Returns true if we show the view (or start the process).
  bool MaybeShowDevModeExtensionsBubble(views::View* anchor_view);

  // Starts or stops observing the BrowserActionsContainer, if necessary.
  void MaybeObserve();
  void MaybeStopObserving();

  // Adds |profile| to the list of profiles that have been evaluated for showing
  // a bubble. Handy for things that only want to check once per profile.
  void RecordProfileCheck(Profile* profile);
  // Returns false if this profile has been evaluated before.
  bool IsInitialProfileCheck(Profile* profile);

  // BrowserActionsContainer::Observer implementation.
  virtual void OnBrowserActionsContainerAnimationEnded() OVERRIDE;
  virtual void OnBrowserActionsContainerDestroyed() OVERRIDE;

  // Sets the stage for highlighting extensions and then showing the bubble
  // controlled by |controller|, anchored to |anchor_view|.
  void PrepareToHighlightExtensions(
      scoped_ptr<ExtensionMessageBubbleController> controller,
      views::View* anchor_view);

  // Inform the ExtensionToolbarModel to highlight the appropriate extensions.
  void HighlightExtensions();

  // Shows the waiting bubbble, after highlighting the extensions.
  void ShowHighlightingBubble();

  // Finishes the process of showing the developer mode bubble.
  void Finish();

  // The associated profile.
  Profile* profile_;

  // The toolbar view that the ExtensionMessageBubbleViews will attach to.
  ToolbarView* toolbar_view_;

  // Whether or not we have shown the suspicious extensions bubble.
  bool shown_suspicious_extensions_bubble_;

  // Whether or not we have shown the Settings API extensions bubble notifying
  // the user about the startup pages being overridden.
  bool shown_startup_override_extensions_bubble_;

  // Whether or not we have shown the bubble notifying the user about the proxy
  // being overridden.
  bool shown_proxy_override_extensions_bubble_;

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

  // The controller to show a bubble for. This will be NULL if the factory is
  // not currently in the process of showing a bubble.
  scoped_ptr<ExtensionMessageBubbleController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleFactory);
};

// This is a class that implements the UI for the bubble showing which
// extensions look suspicious and have therefore been automatically disabled.
class ExtensionMessageBubbleView : public ExtensionMessageBubble,
                                   public views::BubbleDelegateView,
                                   public views::ButtonListener,
                                   public views::LinkListener {
 public:
  ExtensionMessageBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow_location,
      scoped_ptr<ExtensionMessageBubbleController> controller);

  // ExtensionMessageBubble methods.
  virtual void OnActionButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnDismissButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnLinkClicked(const base::Closure& callback) OVERRIDE;
  virtual void Show() OVERRIDE;

  // WidgetObserver methods.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  virtual ~ExtensionMessageBubbleView();

  void ShowBubble();

  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View implementation.
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;

  // The controller for this bubble.
  scoped_ptr<ExtensionMessageBubbleController> controller_;

  // The view this bubble is anchored against.
  views::View* anchor_view_;

  // The headline, labels and buttons on the bubble.
  views::Label* headline_;
  views::Link* learn_more_;
  views::LabelButton* action_button_;
  views::LabelButton* dismiss_button_;

  // All actions (link, button, esc) close the bubble, but we need to
  // make sure we don't send dismiss if the link was clicked.
  bool link_clicked_;
  bool action_taken_;

  // Callbacks into the controller.
  base::Closure action_callback_;
  base::Closure dismiss_callback_;
  base::Closure link_callback_;

  base::WeakPtrFactory<ExtensionMessageBubbleView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleView);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
