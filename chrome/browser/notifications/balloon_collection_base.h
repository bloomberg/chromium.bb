// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_BASE_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_BASE_H_
#pragma once

#include <deque>
#include <string>

#include "base/basictypes.h"

class Balloon;
class GURL;
class Notification;

// This class provides support for implementing a BalloonCollection
// including the parts common between Chrome UI and ChromeOS UI.
class BalloonCollectionBase {
 public:
  BalloonCollectionBase();
  virtual ~BalloonCollectionBase();

  typedef std::deque<Balloon*> Balloons;

  // Adds a balloon to the collection. Takes ownership of pointer.
  virtual void Add(Balloon* balloon);

  // Removes a balloon from the collection (if present).  Frees
  // the pointer after removal.
  virtual void Remove(Balloon* balloon);

  // Finds any balloon matching the given notification id, and
  // calls CloseByScript on it.  Returns true if anything was
  // found.
  virtual bool CloseById(const std::string& id);

  // Finds all balloons matching the given notification source,
  // and calls CloseByScript on them.  Returns true if anything
  // was found.
  virtual bool CloseAllBySourceOrigin(const GURL& source_origin);

  // Calls CloseByScript on all balloons.
  virtual void CloseAll();

  const Balloons& balloons() const { return balloons_; }

  // Returns the balloon matching the given notification, or
  // NULL if there is no matching balloon.
  Balloon* FindBalloon(const Notification& notification);

  // The number of balloons being displayed.
  int count() const { return static_cast<int>(balloons_.size()); }

 private:
  // Queue of active balloons.  Pointers are owned by this class.
  Balloons balloons_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionBase);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_BASE_H_
