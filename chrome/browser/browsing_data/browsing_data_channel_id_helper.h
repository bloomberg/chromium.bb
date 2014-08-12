// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANNEL_ID_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANNEL_ID_HELPER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "net/ssl/channel_id_store.h"

class Profile;

// BrowsingDataChannelIDHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in the channel ID store.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataChannelIDHelper
    : public base::RefCountedThreadSafe<BrowsingDataChannelIDHelper> {
 public:
  // Create a BrowsingDataChannelIDHelper instance for the given
  // |profile|.
  static BrowsingDataChannelIDHelper* Create(Profile* profile);

  typedef base::Callback<
      void(const net::ChannelIDStore::ChannelIDList&)>
      FetchResultCallback;

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(const FetchResultCallback& callback) = 0;
  // Requests a single channel ID to be deleted.  This must be called in
  // the UI thread.
  virtual void DeleteChannelID(const std::string& server_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataChannelIDHelper>;
  virtual ~BrowsingDataChannelIDHelper() {}
};

// This class is a thin wrapper around BrowsingDataChannelIDHelper that
// does not fetch its information from the ChannelIDService, but gets them
// passed as a parameter during construction.
class CannedBrowsingDataChannelIDHelper
    : public BrowsingDataChannelIDHelper {
 public:
  CannedBrowsingDataChannelIDHelper();

  // Return a copy of the ChannelID helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataChannelIDHelper* Clone();

  // Add an ChannelID to the set of canned channel IDs that is
  // returned by this helper.
  void AddChannelID(
      const net::ChannelIDStore::ChannelID& channel_id);

  // Clears the list of canned channel IDs.
  void Reset();

  // True if no ChannelIDs are currently stored.
  bool empty() const;

  // Returns the current number of channel IDs.
  size_t GetChannelIDCount() const;

  // BrowsingDataChannelIDHelper methods.
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void DeleteChannelID(const std::string& server_id) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataChannelIDHelper();

  void FinishFetching();

  typedef std::map<std::string, net::ChannelIDStore::ChannelID>
      ChannelIDMap;
  ChannelIDMap channel_id_map_;

  FetchResultCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataChannelIDHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANNEL_ID_HELPER_H_
