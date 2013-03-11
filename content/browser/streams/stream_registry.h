// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_REGISTRY_H_
#define CONTENT_BROWSER_STREAMS_STREAM_REGISTRY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace content {

class Stream;

// Maintains a mapping of blob: URLs to active streams.
class CONTENT_EXPORT StreamRegistry : public base::NonThreadSafe {
 public:
  StreamRegistry();
  virtual ~StreamRegistry();

  // Registers a stream, and sets its URL.
  void RegisterStream(scoped_refptr<Stream> stream);

  // Clones a stream.  Returns true on success, or false if |src_url| doesn't
  // exist.
  bool CloneStream(const GURL& url, const GURL& src_url);

  void UnregisterStream(const GURL& url);

  // Gets the stream associated with |url|.  Returns NULL if there is no such
  // stream.
  scoped_refptr<Stream> GetStream(const GURL& url);

 private:
  typedef std::map<GURL, scoped_refptr<Stream> > StreamMap;

  StreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(StreamRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_REGISTRY_H_


