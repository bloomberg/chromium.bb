// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_

#include <deque>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/point.h"
#include "gfx/rect.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chromeos {

class BalloonViewImpl;

// A balloon collection represents a set of notification balloons being
// shown in the chromeos notification panel. Unlike other platforms,
// chromeos shows the all notifications in the notification panel, and
// this class does not manage the location of balloons.
class BalloonCollectionImpl : public BalloonCollection,
                              public NotificationObserver {
 public:
  // An interface to display balloons on the screen.
  // This is used for unit tests to inject a mock ui implementation.
  class NotificationUI {
   public:
    NotificationUI() {}
    virtual ~NotificationUI() {}

    // Add, remove, resize and show the balloon.
    virtual void Add(Balloon* balloon) = 0;
    virtual bool Update(Balloon* balloon) = 0;
    virtual void Remove(Balloon* balloon) = 0;
    virtual void Show(Balloon* balloon) = 0;

    // Resize notification from webkit.
    virtual void ResizeNotification(Balloon* balloon,
                                    const gfx::Size& size) = 0;

    // Sets the active view.
    virtual void SetActiveView(BalloonViewImpl* view) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(NotificationUI);
  };

  BalloonCollectionImpl();
  virtual ~BalloonCollectionImpl();

  // BalloonCollectionInterface overrides
  virtual void Add(const Notification& notification,
                   Profile* profile);
  virtual bool Remove(const Notification& notification);
  virtual bool HasSpace() const;
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size);
  virtual void DisplayChanged() {}
  virtual void OnBalloonClosed(Balloon* source);
  virtual const Balloons& GetActiveBalloons() { return balloons_; }

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Adds new system notification.
  // |sticky| is used to indicate that the notification
  // is sticky and cannot be dismissed by a user. |controls| turns on/off
  // info label and option/dismiss buttons.
  void AddSystemNotification(const Notification& notification,
                             Profile* profile, bool sticky, bool controls);

  // Updates the notification's content. It uses
  // NotificationDelegate::id() to check the equality of notifications.
  // Returns true if the notification has been updated. False if
  // no corresponding notification is found. This will not change the
  // visibility of the notification.
  bool UpdateNotification(const Notification& notification);

  // Updates and shows the notification. It will open the notification panel
  // if it's closed or minimized, and scroll the viewport so that
  // the updated notification is visible.
  bool UpdateAndShowNotification(const Notification& notification);

  // Injects notification ui. Used to inject a mock implementation in tests.
  void set_notification_ui(NotificationUI* ui) {
    notification_ui_.reset(ui);
  }

  NotificationUI* notification_ui() {
    return notification_ui_.get();
  }

 protected:
  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

 private:
  friend class NotificationPanelTester;

  // Shutdown the notification ui.
  void Shutdown();

  // The number of balloons being displayed.
  int count() const { return balloons_.size(); }

  Balloons::iterator FindBalloon(const Notification& notification);

  // Queue of active balloons.
  Balloons balloons_;

  scoped_ptr<NotificationUI> notification_ui_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
