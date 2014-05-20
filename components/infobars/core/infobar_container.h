// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/infobars/core/infobar_manager.h"
#include "third_party/skia/include/core/SkColor.h"

namespace infobars {

class InfoBar;

// InfoBarContainer is a cross-platform base class to handle the visibility-
// related aspects of InfoBars.  While InfoBarManager owns the InfoBars, the
// InfoBarContainer is responsible for telling particular InfoBars that they
// should be hidden or visible.
//
// Platforms need to subclass this to implement a few platform-specific
// functions, which are pure virtual here.
class InfoBarContainer : public InfoBarManager::Observer {
 public:
  class Delegate {
   public:
    // The separator color may vary depending on where the container is hosted.
    virtual SkColor GetInfoBarSeparatorColor() const = 0;

    // The delegate is notified each time the infobar container changes height,
    // as well as when it stops animating.
    virtual void InfoBarContainerStateChanged(bool is_animating) = 0;

    // The delegate needs to tell us whether "unspoofable" arrows should be
    // drawn, and if so, at what |x| coordinate.  |x| may be NULL.
    virtual bool DrawInfoBarArrows(int* x) const = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InfoBarContainer(Delegate* delegate);
  virtual ~InfoBarContainer();

  // Changes the InfoBarManager for which this container is showing infobars.
  // This will hide all current infobars, remove them from the container, add
  // the infobars from |infobar_manager|, and show them all.  |infobar_manager|
  // may be NULL.
  void ChangeInfoBarManager(InfoBarManager* infobar_manager);

  // Returns the amount by which to overlap the toolbar above, and, when
  // |total_height| is non-NULL, set it to the height of the InfoBarContainer
  // (including overlap).
  int GetVerticalOverlap(int* total_height) const;

  // Called by the delegate when the distance between what the top infobar's
  // "unspoofable" arrow would point to and the top infobar itself changes.
  // This enables the top infobar to show a longer arrow (e.g. because of a
  // visible bookmark bar) or shorter (e.g. due to being in a popup window) if
  // desired.
  //
  // IMPORTANT: This MUST NOT result in a call back to
  // Delegate::InfoBarContainerStateChanged() unless it causes an actual
  // change, lest we infinitely recurse.
  void SetMaxTopArrowHeight(int height);

  // Called when a contained infobar has animated or by some other means changed
  // its height, or when it stops animating.  The container is expected to do
  // anything necessary to respond, e.g. re-layout.
  void OnInfoBarStateChanged(bool is_animating);

  // Called by |infobar| to request that it be removed from the container.  At
  // this point, |infobar| should already be hidden.
  void RemoveInfoBar(InfoBar* infobar);

  const Delegate* delegate() const { return delegate_; }

 protected:
  // Subclasses must call this during destruction, so that we can remove
  // infobars (which will call the pure virtual functions below) while the
  // subclass portion of |this| has not yet been destroyed.
  void RemoveAllInfoBarsForDestruction();

  // These must be implemented on each platform to e.g. adjust the visible
  // object hierarchy.  The first two functions should each be called exactly
  // once during an infobar's life (see comments on RemoveInfoBar() and
  // AddInfoBar()).
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) = 0;
  // TODO(miguelg): Remove this; it is only necessary for Android, and only
  // until the translate infobar is implemented as three different infobars like
  // GTK does.
  virtual void PlatformSpecificReplaceInfoBar(InfoBar* old_infobar,
                                              InfoBar* new_infobar) {}
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) = 0;
  virtual void PlatformSpecificInfoBarStateChanged(bool is_animating) {}

 private:
  typedef std::vector<InfoBar*> InfoBars;

  // InfoBarManager::Observer:
  virtual void OnInfoBarAdded(InfoBar* infobar) OVERRIDE;
  virtual void OnInfoBarRemoved(InfoBar* infobar, bool animate) OVERRIDE;
  virtual void OnInfoBarReplaced(InfoBar* old_infobar,
                                 InfoBar* new_infobar) OVERRIDE;
  virtual void OnManagerShuttingDown(InfoBarManager* manager) OVERRIDE;

  // Adds |infobar| to this container before the existing infobar at position
  // |position| and calls Show() on it.  |animate| is passed along to
  // infobar->Show().  Depending on the value of |callback_status|, this calls
  // infobar->set_container(this) either before or after the call to Show() so
  // that OnInfoBarStateChanged() either will or won't be called as a result.
  enum CallbackStatus { NO_CALLBACK, WANT_CALLBACK };
  void AddInfoBar(InfoBar* infobar,
                  size_t position,
                  bool animate,
                  CallbackStatus callback_status);

  void UpdateInfoBarArrowTargetHeights();
  int ArrowTargetHeightForInfoBar(size_t infobar_index) const;

  Delegate* delegate_;
  InfoBarManager* infobar_manager_;
  InfoBars infobars_;

  // Calculated in SetMaxTopArrowHeight().
  int top_arrow_target_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_
