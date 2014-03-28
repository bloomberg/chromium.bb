// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

class ExtensionService;

namespace extensions {

// The class responsible for cleaning up the cruft left behind on the file
// system by uninstalled (or failed install) extensions.
// The class is owned by ExtensionService, but is mostly independent. Tasks to
// garbage collect extensions and isolated storage are posted once the
// ExtensionSystem signals ready.
class ExtensionGarbageCollector {
 public:
  explicit ExtensionGarbageCollector(ExtensionService* extension_service);
  ~ExtensionGarbageCollector();

#if defined(OS_CHROMEOS)
  // Enable or disable garbage collection. See |disable_garbage_collection_|.
  void disable_garbage_collection() { disable_garbage_collection_ = true; }
  void enable_garbage_collection() { disable_garbage_collection_ = false; }
#endif

  // Manually trigger GarbageCollectExtensions() for testing.
  void GarbageCollectExtensionsForTest();

 private:
  // Cleans up the extension install directory. It can end up with garbage in it
  // if extensions can't initially be removed when they are uninstalled (eg if a
  // file is in use).
  // Obsolete version directories are removed, as are directories that aren't
  // found in the ExtensionPrefs.
  // The "Temp" directory that is used during extension installation will get
  // removed iff there are no pending installations.
  void GarbageCollectExtensions();

  // The FILE-thread implementation of GarbageCollectExtensions().
  void GarbageCollectExtensionsOnFileThread(
      const std::multimap<std::string, base::FilePath>& extension_paths,
      bool clean_temp_dir);

  // Garbage collects apps/extensions isolated storage, if it is not currently
  // active (i.e. is not in ExtensionRegistry::ENABLED). There is an exception
  // for ephemeral apps, because they can outlive their cache lifetimes.
  void GarbageCollectIsolatedStorageIfNeeded();

  // The ExtensionService which owns this GarbageCollector.
  ExtensionService* extension_service_;

  // The BrowserContext associated with the GarbageCollector, for convenience.
  // (This is equivalent to extension_service_->GetBrowserContext().)
  content::BrowserContext* context_;

  // The root extensions installation directory.
  base::FilePath install_directory_;

#if defined(OS_CHROMEOS)
  // TODO(rkc): HACK alert - this is only in place to allow the
  // kiosk_mode_screensaver to prevent its extension from getting garbage
  // collected. Remove this once KioskModeScreensaver is removed.
  // See crbug.com/280363
  bool disable_garbage_collection_;
#endif

  // Generate weak pointers for safely posting to the file thread for garbage
  // collection.
  base::WeakPtrFactory<ExtensionGarbageCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionGarbageCollector);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
