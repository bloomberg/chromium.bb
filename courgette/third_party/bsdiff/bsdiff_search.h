// Copyright 2003, 2004 Colin Percival
// All rights reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted providing that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// For the terms under which this work may be distributed, please see
// the adjoining file "LICENSE".
//
// ChangeLog:
// 2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
//              values throughout.
//                --Benjamin Smedberg <benjamin@smedbergs.us>
// 2015-08-03 - Change search() to template to allow PagedArray usage.
//                --Samuel Huang <huangs@chromium.org>
// 2015-08-19 - Optimized search() to be non-recursive.
//                --Samuel Huang <huangs@chromium.org>
// 2016-06-17 - Moved matchlen() and search() to a new file; format; changed
//              search() use std::lexicographical_compare().
//                --Samuel Huang <huangs@chromium.org>

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_
#define COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_

#include <algorithm>
#include <cstring>

namespace courgette {

// Similar to ::memcmp(), but returns match length instead.
inline int matchlen(const unsigned char* old,
                    int oldsize,
                    const unsigned char* newbuf,
                    int newsize) {
  int i;

  for (i = 0; (i < oldsize) && (i < newsize); i++)
    if (old[i] != newbuf[i])
      break;

  return i;
}

// Finds a suffix in |old| that has the longest common prefix with |newbuf|,
// aided by suffix array |I| of |old|. Returns the match length, and writes to
// |pos| a position of best match in |old|. If multiple such positions exist,
// |pos| would take an arbitrary one.
template <class T>
int search(T I,
           const unsigned char* old,
           int oldsize,
           const unsigned char* newbuf,
           int newsize,
           int* pos) {
  int lo = 0;
  int hi = oldsize;
  while (hi - lo >= 2) {
    int mid = (lo + hi) / 2;
    if (std::lexicographical_compare(old + I[mid], old + oldsize, newbuf,
                                     newbuf + newsize)) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  int x = matchlen(old + I[lo], oldsize - I[lo], newbuf, newsize);
  int y = matchlen(old + I[hi], oldsize - I[hi], newbuf, newsize);
  if (x > y) {
    *pos = I[lo];
    return x;
  }
  *pos = I[hi];
  return y;
}

}  // namespace courgette

#endif  // COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_
