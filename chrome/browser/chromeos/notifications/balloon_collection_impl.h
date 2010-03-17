// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_

#include <deque>

#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/notifications/balloon_collection.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chromeos {

// A balloon collection represents a set of notification balloons being
// shown in the chromeos notification panel. Unlike other platforms,
// chromeos shows the all notifications in the notification panel, and
// this class does not manage the location of balloons.
class BalloonCollectionImpl : public BalloonCollection {
 public:
  // An interface to display balloons on the screen.
  // This is used for unit tests to inject a mock ui implementation.
  class NotificationUI {
   public:
    NotificationUI() {}
    virtual ~NotificationUI() {}

    // Add, remove and resize the balloon.
    virtual void Add(Balloon* balloon) = 0;
    virtual bool Update(Balloon* balloon) = 0;
    virtual void Remove(Balloon* balloon) = 0;
    virtual void ResizeNotification(Balloon* balloon,
                                    const gfx::Size& size) = 0;
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

  // Adds new system notification.
  // |sticky| is used to indicate that the notification
  // is sticky and cannot be dismissed by a user. |controls| turns on/off
  // info label and option/dismiss buttons.
  void AddSystemNotification(const Notification& notification,
                             Profile* profile, bool sticky, bool controls);

  // Update the notification's content. It uses
  // NotificationDelegate::id() to check the equality of notifications.
  // Returns true if the notification has been updated. False if
  // no corresponding notification is found.
  bool UpdateNotification(const Notification& notification);

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
  // The number of balloons being displayed.
  int count() const { return balloons_.size(); }

  typedef std::deque<Balloon*> Balloons;
  Balloons::iterator FindBalloon(const Notification& notification);

  // Queue of active balloons.
  Balloons balloons_;

  scoped_ptr<NotificationUI> notification_ui_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_H_
