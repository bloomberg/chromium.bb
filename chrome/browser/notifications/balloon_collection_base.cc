// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_base.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "googleurl/src/gurl.h"

BalloonCollectionBase::BalloonCollectionBase() {
}

BalloonCollectionBase::~BalloonCollectionBase() {
  STLDeleteElements(&balloons_);
}

void BalloonCollectionBase::Add(Balloon* balloon) {
  balloons_.push_back(balloon);
}

void BalloonCollectionBase::Remove(Balloon* balloon) {
  // Free after removing.
  scoped_ptr<Balloon> to_delete(balloon);
  Balloons::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    if ((*iter) == balloon) {
      balloons_.erase(iter);
      return;
    }
  }
}

bool BalloonCollectionBase::CloseById(const std::string& id) {
  // Use a local list of balloons to close to avoid breaking
  // iterator changes on each close.
  Balloons to_close;
  Balloons::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    if ((*iter)->notification().notification_id() == id)
      to_close.push_back(*iter);
  }
  for (iter = to_close.begin(); iter != to_close.end(); ++iter)
    (*iter)->CloseByScript();

  return !to_close.empty();
}

bool BalloonCollectionBase::CloseAllBySourceOrigin(
    const GURL& source_origin) {
  // Use a local list of balloons to close to avoid breaking
  // iterator changes on each close.
  Balloons to_close;
  Balloons::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    if ((*iter)->notification().origin_url() == source_origin)
      to_close.push_back(*iter);
  }
  for (iter = to_close.begin(); iter != to_close.end(); ++iter)
    (*iter)->CloseByScript();

  return !to_close.empty();
}

void BalloonCollectionBase::CloseAll() {
  // Use a local list of balloons to close to avoid breaking
  // iterator changes on each close.
  Balloons to_close = balloons_;
  for (Balloons::iterator iter = to_close.begin();
       iter != to_close.end(); ++iter)
    (*iter)->CloseByScript();
}

Balloon* BalloonCollectionBase::FindBalloon(
    const Notification& notification) {
  Balloons::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    if ((*iter)->notification().notification_id() ==
        notification.notification_id()) {
      return *iter;
    }
  }
  return NULL;
}
