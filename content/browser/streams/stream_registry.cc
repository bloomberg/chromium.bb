// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_registry.h"

#include "content/browser/streams/stream.h"

namespace content {

StreamRegistry::StreamRegistry() {
}

StreamRegistry::~StreamRegistry() {
}

void StreamRegistry::RegisterStream(scoped_refptr<Stream> stream) {
  DCHECK(CalledOnValidThread());
  DCHECK(stream);
  DCHECK(!stream->url().is_empty());
  streams_[stream->url()] = stream;
}

scoped_refptr<Stream> StreamRegistry::GetStream(const GURL& url) {
  DCHECK(CalledOnValidThread());
  StreamMap::const_iterator stream = streams_.find(url);
  if (stream != streams_.end())
    return stream->second;

  return NULL;
}

bool StreamRegistry::CloneStream(const GURL& url, const GURL& src_url) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<Stream> stream(GetStream(src_url));
  if (stream) {
    streams_[url] = stream;
    return true;
  }
  return false;
}

void StreamRegistry::UnregisterStream(const GURL& url) {
  DCHECK(CalledOnValidThread());
  streams_.erase(url);
}

}  // namespace content
