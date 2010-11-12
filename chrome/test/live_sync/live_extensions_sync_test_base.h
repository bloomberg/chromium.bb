// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_BASE_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_BASE_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_temp_dir.h"
#include "base/ref_counted.h"
#include "chrome/test/live_sync/live_sync_test.h"

class Extension;

class LiveExtensionsSyncTestBase : public LiveSyncTest {
 public:
  virtual ~LiveExtensionsSyncTestBase();

  // Like LiveSyncTest::SetupClients(), but also initializes
  // extensions for each profile.
  virtual bool SetupClients() WARN_UNUSED_RESULT;

 protected:
  // Gets a theme based on the given index.  Will always return the
  // same theme given the same index.
  scoped_refptr<Extension> GetTheme(int index) WARN_UNUSED_RESULT;

  // Gets a extension based on the given index.  Will always return the
  // same extension given the same index.
  scoped_refptr<Extension> GetExtension(int index) WARN_UNUSED_RESULT;

  // Installs the given extension to the given profile.
  static void InstallExtension(
      Profile* profile, scoped_refptr<Extension> extension);

  // Installs all pending extensions for the given profile.
  void InstallAllPendingExtensions(Profile* profile);

 private:
  friend class LiveThemesSyncTest;
  friend class LiveExtensionsSyncTest;

  // Private to avoid inadvertent instantiation except through
  // approved subclasses (above).
  explicit LiveExtensionsSyncTestBase(TestType test_type);

  // Returns an extension of the given type and index.  Will always
  // return the same extension given the same type and index.
  scoped_refptr<Extension> GetExtensionHelper(
      bool is_theme, int index) WARN_UNUSED_RESULT;

  typedef std::map<std::pair<bool, int>, scoped_refptr<Extension> >
      ExtensionTypeIndexMap;
  typedef std::map<std::string, scoped_refptr<Extension> > ExtensionIdMap;

  ScopedTempDir extension_base_dir_;
  ExtensionTypeIndexMap extensions_by_type_index_;
  ExtensionIdMap extensions_by_id_;

  DISALLOW_COPY_AND_ASSIGN(LiveExtensionsSyncTestBase);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_BASE_H_
