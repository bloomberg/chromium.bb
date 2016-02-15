// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "ios/chrome/browser/reading_list/reading_list_model_observer.h"

class GURL;
class ReadingListEntry;
class ReadingListModel;

namespace ios {
class ChromeBrowserState;
}

// The reading list model contains two list of entries: one of unread urls, the
// other of read ones. This object should only be accessed from one thread
// (Usually the main thread). The observers callbacks are also sent on the main
// thread.
class ReadingListModel {
 public:
  // Returns true if the model finished loading. Until this returns true the
  // reading list is not ready for use.
  virtual bool loaded() const = 0;

  // Returns the size of read and unread entries.
  virtual size_t unread_size() const = 0;
  virtual size_t read_size() const = 0;

  // Returns true if there are entries in the model that were not seen by the
  // user yet. Reset to true when new unread entries are added. Reset to false
  // when ResetUnseenEntries is called.
  virtual bool HasUnseenEntries() const = 0;
  virtual void ResetUnseenEntries() = 0;

  // Returns a specific entry.
  virtual const ReadingListEntry& GetUnreadEntryAtIndex(size_t index) const = 0;
  virtual const ReadingListEntry& GetReadEntryAtIndex(size_t index) const = 0;

  // Adds |url| at the top of the unread entries, and removes entries with the
  // same |url| from everywhere else if they exist. The addition may be
  // asynchronous, and the data will be available only once the observers are
  // notified.
  virtual const ReadingListEntry& AddEntry(const GURL& url,
                                           const std::string& title) = 0;

  // Removes an entry. The removal may be asynchronous, and not happen
  // immediately.
  virtual void RemoveEntryByUrl(const GURL& url) = 0;

  // If the |url| is in the reading list and unread, mark it read. If it is in
  // the reading list and read, move it to the top of unread if it is not here
  // already. This may trigger deletion of old read entries.
  virtual void MarkReadByURL(const GURL& url) = 0;

  // Observer registration methods.
  void AddObserver(ReadingListModelObserver* observer);
  void RemoveObserver(ReadingListModelObserver* observer);

 protected:
  ReadingListModel();
  virtual ~ReadingListModel();
  // The observers.
  base::ObserverList<ReadingListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListModel);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_H_
