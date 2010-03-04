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

namespace chromeos {

class NotificationPanel;

// A balloon collection represents a set of notification balloons being
// shown in the chromeos notification panel. Unlike other platforms,
// chromeos shows the all notifications in the notification panel, and
// this class does not manage the location of balloons.
class BalloonCollectionImpl : public BalloonCollection {
 public:
  BalloonCollectionImpl();
  virtual ~BalloonCollectionImpl();

  // BalloonCollectionInterface overrides
  virtual void Add(const Notification& notification,
                   Profile* profile);
  virtual bool Remove(const Notification& notification);
  virtual bool HasSpace() const;
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size);
  virtual void DisplayChanged();
  virtual void OnBalloonClosed(Balloon* source);

 protected:
  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

 private:
  // The number of balloons being displayed.
  int count() const { return balloons_.size(); }

  // Queue of active balloons.
  typedef std::deque<Balloon*> Balloons;
  Balloons balloons_;

  scoped_ptr<NotificationPanel> panel_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_H_
