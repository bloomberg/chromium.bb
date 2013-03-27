// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/common/search_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkColor.h"

class InfoBar;
class InfoBarDelegate;
class InfoBarService;
class SearchModel;

// InfoBarContainer is a cross-platform base class to handle the visibility-
// related aspects of InfoBars.  While InfoBars own themselves, the
// InfoBarContainer is responsible for telling particular InfoBars that they
// should be hidden or visible.
//
// Platforms need to subclass this to implement a few platform-specific
// functions, which are pure virtual here.
//
// This class also observes changes to the SearchModel modes.  It hides infobars
// temporarily if the user changes into |SEARCH_SUGGESTIONS| mode (refer to
// SearchMode in chrome/common/search_types.h for all search modes)
// when on a :
// - |DEFAULT| page: when Instant overlay is ready;
// - |NTP| or |SEARCH_RESULTS| page: immediately;
//   TODO(kuan): this scenario requires more complex synchronization with
//   renderer SearchBoxAPI and will be implemented as the next step;
//   for now, hiding is immediate.
// When the user changes back out of |SEARCH_SUGGESTIONS| mode, it reshows any
// infobars, and starts a 50 ms window during which any attempts to re-hide any
// infobars are handled without animation.  This prevents glitchy-looking
// behavior when the user navigates following a mode change, which otherwise
// would re-show the infobars only to instantly animate them closed.  The window
// to re-hide infobars without animation is canceled if a tab change occurs.
class InfoBarContainer : public content::NotificationObserver,
                         public SearchModelObserver {
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

  // |search_model| may be NULL if this class is used in a window that does not
  // support Instant Extended.
  InfoBarContainer(Delegate* delegate, SearchModel* search_model);
  virtual ~InfoBarContainer();

  // Changes the InfoBarService for which this container is showing
  // infobars.  This will remove all current infobars from the container, add
  // the infobars from |infobar_service|, and show them all.  |infobar_service|
  // may be NULL.
  void ChangeInfoBarService(InfoBarService* infobar_service);

  // Returns the amount by which to overlap the toolbar above, and, when
  // |total_height| is non-NULL, set it to the height of the InfoBarContainer
  // (including overlap).
  int GetVerticalOverlap(int* total_height);

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

  // Called by |infobar| to request that it be removed from the container, as it
  // is about to delete itself.  At this point, |infobar| should already be
  // hidden.
  void RemoveInfoBar(InfoBar* infobar);

  const Delegate* delegate() const { return delegate_; }

 protected:
  // Subclasses must call this during destruction, so that we can remove
  // infobars (which will call the pure virtual functions below) while the
  // subclass portion of |this| has not yet been destroyed.
  void RemoveAllInfoBarsForDestruction();

  // These must be implemented on each platform to e.g. adjust the visible
  // object hierarchy.
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) = 0;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) = 0;
  virtual void PlatformSpecificInfoBarStateChanged(bool is_animating) {}

 private:
  typedef std::vector<InfoBar*> InfoBars;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // Hides an InfoBar for the specified delegate, in response to a notification
  // from the selected InfoBarService.  The InfoBar's disappearance will be
  // animated if |use_animation| is true and it has been more than 50ms since
  // infobars were reshown due to an Instant Extended mode change. The InfoBar
  // will call back to RemoveInfoBar() to remove itself once it's hidden (which
  // may mean synchronously).  Returns the position within |infobars_| the
  // infobar was previously at.
  size_t HideInfoBar(InfoBarDelegate* delegate, bool use_animation);

  // Hides all infobars in this container without animation.
  void HideAllInfoBars();

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

  content::NotificationRegistrar registrar_;
  Delegate* delegate_;
  InfoBarService* infobar_service_;
  InfoBars infobars_;

  // Tracks whether infobars in the container are shown or hidden.
  bool infobars_shown_;

  // Tracks the most recent time infobars were re-shown after being hidden due
  // to Instant Extended's ModeChanged.
  base::TimeTicks infobars_shown_time_;

  // Tracks which search mode is active, as well as mode changes, for Instant
  // Extended.
  SearchModel* search_model_;

  // Calculated in SetMaxTopArrowHeight().
  int top_arrow_target_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_H_
