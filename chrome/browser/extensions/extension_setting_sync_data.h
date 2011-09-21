// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_SYNC_DATA_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/sync/api/sync_change.h"

class SyncData;

// Container for data interpreted from sync data/changes.  Safe and efficient
// to copy.
class ExtensionSettingSyncData {
 public:
  // Creates from a sync change.
  explicit ExtensionSettingSyncData(const SyncChange& sync_change);

  // Creates from sync data.  change_type will be ACTION_INVALID.
  explicit ExtensionSettingSyncData(const SyncData& sync_data);

  // Creates explicitly.
  ExtensionSettingSyncData(
      SyncChange::SyncChangeType change_type,
      const std::string& extension_id,
      const std::string& key,
      // May NOT be NULL.  Ownership taken.
      Value* value);

  ~ExtensionSettingSyncData();

  // Returns the type of the sync change; may be ACTION_INVALID.
  SyncChange::SyncChangeType change_type() const;

  // Returns the extension id the setting is for.
  const std::string& extension_id() const;

  // Returns the settings key.
  const std::string& key() const;

  // Returns the value of the setting.
  const Value& value() const;

 private:
  // Ref-counted container for the data.
  // TODO(kalman): Use browser_sync::Immutable<Internal>.
  class Internal : public base::RefCountedThreadSafe<Internal> {
   public:
    explicit Internal(
      SyncChange::SyncChangeType change_type,
      const std::string& extension_id,
      const std::string& key,
      Value* value);

    SyncChange::SyncChangeType change_type_;
    std::string extension_id_;
    std::string key_;
    scoped_ptr<Value> value_;

   private:
    friend class base::RefCountedThreadSafe<Internal>;
    ~Internal();
  };

  // Initializes internal_ from sync data.
  void Init(SyncChange::SyncChangeType change_type, const SyncData& sync_data);

  scoped_refptr<Internal> internal_;
};

typedef std::vector<ExtensionSettingSyncData> ExtensionSettingSyncDataList;

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_SYNC_DATA_H_
