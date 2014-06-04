// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_STORE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_STORE_H_

#include <string>

#include "base/bind.h"
#include "base/containers/mru_cache.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"

namespace dom_distiller {

// The maximum number of items to keep in the cache before deleting some.
const int kDefaultMaxNumCachedEntries = 32;

// This is a simple interface for saving and loading of distilled content for an
// ArticleEntry.
class DistilledContentStore {
 public:
  typedef base::Callback<
      void(bool /* success */, scoped_ptr<DistilledArticleProto>)> LoadCallback;
  typedef base::Callback<void(bool /* success */)> SaveCallback;

  virtual void SaveContent(const ArticleEntry& entry,
                           const DistilledArticleProto& proto,
                           SaveCallback callback) = 0;
  virtual void LoadContent(const ArticleEntry& entry,
                           LoadCallback callback) = 0;

  DistilledContentStore() {};
  virtual ~DistilledContentStore() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(DistilledContentStore);
};

// This content store keeps up to |max_num_entries| of the last accessed items
// in its cache. Both loading and saving content is counted as access.
class InMemoryContentStore : public DistilledContentStore {
 public:
  explicit InMemoryContentStore(const int max_num_entries);
  virtual ~InMemoryContentStore();

  // DistilledContentStore implementation
  virtual void SaveContent(const ArticleEntry& entry,
                           const DistilledArticleProto& proto,
                           SaveCallback callback) OVERRIDE;
  virtual void LoadContent(const ArticleEntry& entry,
                           LoadCallback callback) OVERRIDE;

  // Synchronously saves the content.
  void InjectContent(const ArticleEntry& entry,
                     const DistilledArticleProto& proto);

 private:
  typedef base::MRUCache<std::string, DistilledArticleProto> ContentMap;
  ContentMap cache_;
};
}  // dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_CACHE_H_
