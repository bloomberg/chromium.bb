// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_OBSERVER_H_

#import <set>
#import <vector>

class ReadingListModel;
class ReadingListEntry;

// Observer for the Reading List model. In the observer methods care should be
// taken to not modify the model.
class ReadingListModelObserver {
 public:
  // Invoked when the model has finished loading. Until this method is called it
  // is unsafe to use the model.
  virtual void ReadingListModelLoaded(const ReadingListModel* model) = 0;

  // Invoked from the destructor of the model. The model is no longer valid
  // after this call.
  virtual void ReadingListModelBeingDeleted(const ReadingListModel* model) {}

  // Invoked when elements are about to be removed from the read or unread list.
  virtual void ReadingListWillRemoveUnreadEntry(const ReadingListModel* model,
                                                size_t index) {}
  virtual void ReadingListWillRemoveReadEntry(const ReadingListModel* model,
                                              size_t index) {}

  // Invoked when elements are added to the read or the unread list. The new
  // entries are always added at the beginning. these methods may be called
  // multiple time (to process changes coming from a synchronization for
  // example) and they will be executed in call order, the last call will end up
  // in first position.
  virtual void ReadingListWillAddUnreadEntry(const ReadingListModel* model,
                                             const ReadingListEntry& entry) {}

  virtual void ReadingListWillAddReadEntry(const ReadingListModel* model,
                                           const ReadingListEntry& entry) {}

  // Called after all th"e changes signaled by calls to the "Will" methods are
  // done. All the "Will" methods are called as necessary, then the changes
  // are applied and then this method is called.
  virtual void ReadingListDidApplyChanges(ReadingListModel* model) {}

 protected:
  ReadingListModelObserver() {}
  virtual ~ReadingListModelObserver() {}

  DISALLOW_COPY_AND_ASSIGN(ReadingListModelObserver);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_OBSERVER_H_
