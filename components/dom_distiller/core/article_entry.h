// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ENTRY_H_
#define COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ENTRY_H_

#include "sync/protocol/article_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace dom_distiller {

typedef sync_pb::ArticleSpecifics ArticleEntry;
typedef sync_pb::ArticlePage ArticleEntryPage;

bool IsEntryValid(const ArticleEntry& entry);

bool AreEntriesEqual(const ArticleEntry& left, const ArticleEntry& right);

sync_pb::EntitySpecifics SpecificsFromEntry(const ArticleEntry& entry);
ArticleEntry EntryFromSpecifics(const sync_pb::EntitySpecifics& specifics);

}  // namespace dom_distiller

#endif
