// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_H_
#pragma once

// ExtensionData is the class used to manage the client and server
// versions of the data for a particular extension.

#include <map>

#include "chrome/browser/sync/protocol/extension_specifics.pb.h"

namespace browser_sync {

class ExtensionData {
 public:
  enum Source {
    CLIENT,
    SERVER,
  };

  // Returns an ExtensionData constructed from the given data from the
  // given source.  merged_data() will be equal to |data| and
  // NeedsUpdate(source) will return false.
  static ExtensionData FromData(
      Source source, const sync_pb::ExtensionSpecifics& data);

  ~ExtensionData();

  // Implicit copy constructor and assignment operator welcome.

  // Returns the version of the data that all sources should
  // eventually have.
  const sync_pb::ExtensionSpecifics& merged_data() const;

  // Returns whether or not the data from the given source needs to be
  // updated from merged_data().
  bool NeedsUpdate(Source source) const;

  // Stores the given data as being from the given source, merging it
  // into merged_data().  May change what NeedsUpdate() returns for
  // any source.
  void SetData(
      Source source, bool merge_user_properties,
      const sync_pb::ExtensionSpecifics& data);

  // Marks the data from the given data as updated,
  // i.e. NeedsUpdate(source) will return false.
  void ResolveData(Source source);

 private:
  typedef std::map<Source, sync_pb::ExtensionSpecifics> SourceDataMap;
  SourceDataMap source_data_;
  sync_pb::ExtensionSpecifics merged_data_;

  // This is private; clients must use FromData().
  ExtensionData();
};

}  // namespace

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_H_
