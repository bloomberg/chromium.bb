// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_VIEWS_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_VIEWS_H_

#include "app/animation.h"
#include "chrome/browser/blocked_popup_container.h"

class BlockedPopupContainerViewWidget;
class BlockedPopupContainerInternalView;
class SlideAnimation;

// Takes ownership of TabContents that are unrequested popup windows and
// presents an interface to the user for launching them. (Or never showing them
// again).
class BlockedPopupContainerViewViews : public BlockedPopupContainerView,
                                       public AnimationDelegate {
 public:
  virtual ~BlockedPopupContainerViewViews();

  // Returns the URL and title for popup |index|, used to construct a string for
  // display.
  void GetURLAndTitleForPopup(size_t index,
                              std::wstring* url,
                              std::wstring* title) const;

  // Returns the model that owns us.
  BlockedPopupContainer* model() const { return model_; }

  // Overridden from AnimationDelegate:
  virtual void AnimationStarted(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden from BlockedPopupContainerView:
  virtual void SetPosition();
  virtual void ShowView();
  virtual void UpdateLabel();
  virtual void HideView();
  virtual void Destroy();

 private:
  // For the static constructor BlockedPopupContainerView::Create().
  friend class BlockedPopupContainerView;
  friend class BlockedPopupContainerViewWidget;

  // Creates a container for a certain TabContents.
  explicit BlockedPopupContainerViewViews(BlockedPopupContainer* container);

  // Updates the shape of the widget.
  void UpdateWidgetShape(BlockedPopupContainerViewWidget* widget,
                         const gfx::Size& size);

  // Widget hosting container_view_.
  BlockedPopupContainerViewWidget* widget_;

  // Our model; calling the shots.
  BlockedPopupContainer* model_;

  // Our associated view object.
  BlockedPopupContainerInternalView* container_view_;

  // The animation that slides us up and down.
  scoped_ptr<SlideAnimation> slide_animation_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewViews);
};

#endif  // CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_VIEWS_H_
