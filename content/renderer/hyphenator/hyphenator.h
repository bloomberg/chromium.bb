// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_HYPHENATOR_HYPHENATOR_H_
#define CONTENT_RENDERER_HYPHENATOR_HYPHENATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/string16.h"
#include "content/common/content_export.h"

namespace file_util {
class MemoryMappedFile;
}

typedef struct _HyphenDict HyphenDict;

namespace content {

// A class that hyphenates a word. This class encapsulates the hyphen library
// and manages resources used by the library. When this class uses a huge
// dictionary, it takes lots of memory (~1.3MB for English). A renderer should
// create this object only when it renders a page that needs hyphenation and
// deletes it when it moves to a page that does not need hyphenation.
class CONTENT_EXPORT Hyphenator {
 public:
  explicit Hyphenator(base::PlatformFile file);
  ~Hyphenator();

  // Initializes the hyphen library and allocates resources needed for
  // hyphenation.
  bool Initialize();

  // Returns the last hyphenation point, the position where we can insert a
  // hyphen, before the given position. If there are not any hyphenation points,
  // this function returns 0.
  size_t ComputeLastHyphenLocation(const string16& word, size_t before_index);

 private:
  // The dictionary used by the hyphen library.
  HyphenDict* dictionary_;

  // The dictionary file and its memory-mapping object. (Our copy of the hyphen
  // library uses a memory-mapped file opened by a browser so renderers can use
  // it without opening the file.)
  base::PlatformFile rule_file_;
  scoped_ptr<file_util::MemoryMappedFile> rule_map_;

  // A cached result. WebKit often calls ComputeLastHyphenLocation with the same
  // word multiple times to find the best hyphenation point when it finds a line
  // break. On the other hand, the hyphen library returns all hyphenation points
  // for a word. This class caches the hyphenation points returned by the hyphen
  // library to avoid calling the library multiple times.
  string16 word_;
  bool result_;
  std::vector<int> hyphen_offsets_;

  DISALLOW_COPY_AND_ASSIGN(Hyphenator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_HYPHENATOR_HYPHENATOR_H_
