// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/test_image_store.h"

#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

TestImageStore::TestImageStore() {
}

bool TestImageStore::HasKey(const GURL& page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return store_.find(page_url) != store_.end();
}

void TestImageStore::Insert(const GURL& page_url,
                              const GURL& image_url,
                              const gfx::Image& image) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Erase(page_url);
  store_.insert(std::make_pair(
      page_url,
      std::make_pair(image,
                     image_url)));
}

void TestImageStore::Erase(const GURL& page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  store_.erase(page_url);
}

std::pair<gfx::Image, GURL> TestImageStore::Get(const GURL& page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ImageMap::const_iterator pair_enumerator = store_.find(page_url);
  if (pair_enumerator == store_.end())
    return std::make_pair(gfx::Image(), GURL());

  return std::make_pair(store_[page_url].first, store_[page_url].second);
}

gfx::Size TestImageStore::GetSize(const GURL& page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ImageMap::const_iterator pair_enumerator = store_.find(page_url);
  if (pair_enumerator == store_.end())
    return gfx::Size();

  return store_[page_url].first.Size();
}

void TestImageStore::GetAllPageUrls(std::set<GURL>* urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(urls->empty());

  for (ImageMap::const_iterator it = store_.begin(); it != store_.end(); ++it)
    urls->insert(it->first);
}

void TestImageStore::ClearAll() {
  DCHECK(thread_checker_.CalledOnValidThread());

  store_.clear();
}

TestImageStore::~TestImageStore() {
  DCHECK(thread_checker_.CalledOnValidThread());
}
