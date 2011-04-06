// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class ImporterBridge;

namespace importer {
struct SourceProfile;
}

// The base class of all importers.
class Importer : public base::RefCountedThreadSafe<Importer> {
 public:
  // All importers should implement this method by adding their import logic.
  // And it will be run in file thread by ImporterHost. Since we do async
  // import, the importer should invoke ImporterHost::NotifyImportEnded() to
  // notify its host that import stuff have been finished.
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) = 0;

  // Cancels the import process.
  virtual void Cancel();

  void set_import_to_bookmark_bar(bool import_to_bookmark_bar) {
    import_to_bookmark_bar_ = import_to_bookmark_bar;
  }

  void set_bookmark_bar_disabled(bool bookmark_bar_disabled) {
    bookmark_bar_disabled_ = bookmark_bar_disabled;
  }

  bool bookmark_bar_disabled() { return bookmark_bar_disabled_; }
  bool cancelled() const { return cancelled_; }

 protected:
  friend class base::RefCountedThreadSafe<Importer>;

  Importer();
  virtual ~Importer();

  // Given raw image data, decodes the icon, re-sampling to the correct size as
  // necessary, and re-encodes as PNG data in the given output vector. Returns
  // true on success.
  static bool ReencodeFavicon(const unsigned char* src_data,
                              size_t src_len,
                              std::vector<unsigned char>* png_data);

  bool import_to_bookmark_bar() const { return import_to_bookmark_bar_; }

  scoped_refptr<ImporterBridge> bridge_;

 private:
  // True if the importer is created in the first run UI.
  bool import_to_bookmark_bar_;

  // Whether bookmark bar is disabled (not shown) for importer. This is set
  // true during first run to prevent out of process bookmark importer from
  // updating bookmark bar settings.
  bool bookmark_bar_disabled_;

  // True if the caller cancels the import process.
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_H_
