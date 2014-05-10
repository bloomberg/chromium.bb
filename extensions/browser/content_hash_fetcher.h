// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_
#define EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_

#include "base/scoped_observer.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;

// This class is responsible for getting signed expected hashes for use in
// extension content verification. As extensions are loaded it will fetch and
// parse/validate/cache this data as needed, including calculating expected
// hashes for each block of each file within an extension. (These unsigned leaf
// node block level hashes will always be checked at time of use use to make
// sure they match the signed treehash root hash).
class ContentHashFetcher : public ExtensionRegistryObserver {
 public:
  // The consumer of this class needs to ensure that context and delegate
  // outlive this object.
  ContentHashFetcher(content::BrowserContext* context,
                     ContentVerifierDelegate* delegate);
  virtual ~ContentHashFetcher();

  // Begins the process of trying to fetch any needed verified contents, and
  // listening for extension load/unload.
  void Start();

  // Explicitly ask to fetch hashes for |extension|.
  void DoFetch(const Extension* extension);

  // ExtensionRegistryObserver interface
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

 private:
  content::BrowserContext* context_;
  ContentVerifierDelegate* delegate_;

  // For observing the ExtensionRegistry.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_
