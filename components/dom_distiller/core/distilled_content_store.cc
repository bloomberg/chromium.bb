// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_content_store.h"

#include "base/message_loop/message_loop.h"

namespace dom_distiller {

InMemoryContentStore::InMemoryContentStore() {}
InMemoryContentStore::~InMemoryContentStore() {}

void InMemoryContentStore::SaveContent(
    const ArticleEntry& entry,
    const DistilledArticleProto& proto,
    InMemoryContentStore::SaveCallback callback) {
  InjectContent(entry, proto);
  if (!callback.is_null()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, true));
  }
}

void InMemoryContentStore::LoadContent(
    const ArticleEntry& entry,
    InMemoryContentStore::LoadCallback callback) const {
  if (callback.is_null())
    return;

  ContentMap::const_iterator it = cache_.find(entry.entry_id());
  bool success = it != cache_.end();
  scoped_ptr<DistilledArticleProto> distilled_article;
  if (success) {
    distilled_article.reset(new DistilledArticleProto(it->second));
  } else {
    distilled_article.reset(new DistilledArticleProto());
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, success, base::Passed(&distilled_article)));
}

void InMemoryContentStore::InjectContent(const ArticleEntry& entry,
                                         const DistilledArticleProto& proto) {
  cache_[entry.entry_id()] = proto;
}

}  // namespace dom_distiller
